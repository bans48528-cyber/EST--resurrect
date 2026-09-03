# M1.22Y 双电机追赶振荡修复实机验收记录

## 状态与范围

- 保持 M1.22V 的 PWM 巡线语义，不加入 M1.22W 三档动态基础功率。
- M1.22X 实机仍有追赶振荡，不作为验收基线。原 X 升级包保留。
- Y 已完成自动测试、构建、325 帧升级发送，并检测开机完成两轮真机运行。2026-09-03 用户反馈“可以通过了”，本轮双电机追赶振荡修复实机验收通过。
- 保留 Y 当前升级包与控制参数，作为本项修复后续开发的已验收工作区依据；尚未建立新的 Git 发布提交。
- 不修改 EST Studio、用户 API、协议 1.26、能力位、Bootloader 布局或升级包容量。
- 保留原有未提交的传感器、runtime、看门狗、文档和示例成果；未提交、未推送 Git。

## M1.22X 实机复现

用户截图为“向前移动 10 圈”，不是持续速度积木。
调用链为 `rt.drive_move_for()` -> `DriveBase.straight_angle()` -> 双电机位置任务。
2026-09-03 确认设备版本 M1.22X、协议 1.26，B/C 均识别为大型电机。

通过 HID 发起同底层任务：

```powershell
python tools/est_hid_sender/est_hid_sender.py drive-run --left-port B --right-port C --rotations 10 --speed 50 --stop coast
```

关键实际输出：

```text
actual=1119,1207 sync_error=-88 max_sync_error=89
actual=1334,1315 sync_error=18 max_sync_error=89
actual=1486,1338 sync_error=148 max_sync_error=148
actual=1554,1411 sync_error=143 max_sync_error=155
actual=1752,1758 sync_error=-5 max_sync_error=155
actual=2142,2089 sync_error=53 max_sync_error=155
actual=2177,2183 sync_error=-6 max_sync_error=155
actual=3574,3603 sync_error=-29 max_sync_error=155
actual=3600,3603 sync_error=-3 max_sync_error=155
drive_run=complete
safe_final_state=coast
```

最大记录同步误差 155 度，途中多次换向；末段 C 先到，B 停留后继续补角度。
该轮是否施加外力尚未得到用户确认，不能标记为已验证的纯空载数据。

## 根因与修改

1. X 虽对同步 correction 做限幅，仍按 correction 符号切换 leader，随后把 leader 的目标速度硬压到同伴的旧测速值。微小符号变化也可能触发较大速度跳变。
   Y 去掉这条硬限速路径，使用连续、按目标比例缩放的减速修正，可减至零但不抬高落后侧速度。
2. 原速度控制每 10 ms 执行 `PWM += speed_error * 8`，属于累加控制。测速又可能等待到 150 ms，负载释放时撤销多余 PWM 太晚。
   Y 为配对端口新增独立数值控制器：实际编码器增量与实际采样时间、20 ms 一阶滤波、增量 PI。P 立即响应速度变化，I 保留原每 10 ms 的增益 0.08；控制状态采用实际限幅后的 PWM，不另存可无限积累的隐藏输出。
3. X 位置恢复依赖较晚的公开堵转判定（大型电机高 PWM、低速持续约 1 秒），短暂压住后释放可能根本未触发。
   Y 的 PI 撤销多余 PWM 在每个控制周期生效，不以公开 `stalled()` 为前提；X 的候选/恢复限幅作为额外保护保留。
4. 同步修正最大范围允许覆盖全速，但通过连续修正施加；增加修正每 tick 最多 2 个百分点，释放每 tick 最多 4 个百分点，换向经过零。差分反馈提前降低过度追赶。
5. 双电机角度末段复用现有连续减速函数，不再一进入减速区就固定为 1%。原目标角度、完成状态与 HOLD 语义不变。
6. 新配对控制状态在启动/全局停止时清理；持续任务改速不重建控制器。单电机旧速度环和开环 run_power 不改。

新增纯 C 模块：

- `firmware/minimal_upgrade_app/include/board_motor_pair_regulator.h`
- `firmware/minimal_upgrade_app/src/board_motor_pair_regulator.c`

参考 Pybricks 的即时速度阻尼、抗饱和和连续共模/差模控制思路，按 EST PWM/编码器独立实现，不是 Pybricks 控制器的逐行移植：

- [control.c](https://github.com/pybricks/pybricks-micropython/blob/master/lib/pbio/src/control.c)
- [drivebase.c](https://github.com/pybricks/pybricks-micropython/blob/master/lib/pbio/src/drivebase.c)

## 验证

- 完整固件测试：290 项通过。
- HID 工具测试：127 项通过。
- 引脚检查：144 封装引脚，94 项固件占用，0 冲突。
- GCC 12.2.1 ARM C 编译 `-Werror` 通过，打包与校验通过。链接仍有既有 newlib 未实现主机文件系统调用的提示，不涉及本次控制代码。
- 新增测试直接以 CFFI 编译并执行生产数值控制代码，包括：限幅不隐藏积累、首次恢复编码器帧及时回撤 PWM、正反转、零速/低速、连续 20->0->75、tick 回绕、清理、连续修正和换向。
- 简化双电机一阶模型覆盖 20/50/80%、任一侧堵转 2 秒后恢复、正反转与编码器量化，晚期误差/速度差收敛检查通过。
- 模型仅验证数值行为，不代表真实电机参数，不用于宣称真机振荡已经消除。

## 已验收固件包

- 版本：M1.22Y；协议：1.26；能力位不变。
- 原始 APP：288016 字节，比 X 增加 320 字节，比 V 增加 1956 字节。
- 固定升级包：327680 字节（320 KiB），`APP=` 头；余量 39660 字节。
- 路径：`firmware/minimal_upgrade_app/build/m122y_pair_pi_candidate/est_minimal_upgrade_app.upgrade.bin`
- SHA-256：`16dbbd31e53eb7ae20dc0db03d05b547b9b5021b80f0903a30b641ef0e14cb2f`
- 升级发送日志：`C:/Users/64264/AppData/Local/ESTHidSender/logs/upgrade_20260903_114351_376970.log`

## M1.22Y 开机后实测

已通过 USB 心跳与 device-status 确认 M1.22Y、协议 1.26。
第一轮沿用 X 的 B/C、50%、前进 10 圈、COAST 命令：

```text
actual=527,525 sync_error=3 max_sync_error=4
actual=1531,1530 sync_error=1 max_sync_error=4
actual=2440,2438 sync_error=2 max_sync_error=5
actual=3518,3518 sync_error=-1 max_sync_error=5
actual=3585,3585 sync_error=0 max_sync_error=5
drive_state=complete actual=3601,3602 sync_error=-1 max_sync_error=5
drive_run=complete
safe_final_state=coast
```

本轮最大同步误差 5 度；X 记录为 155 度。两轮的外力条件未得到用户确认一致，不能仅凭这个数值对比证明堵转恢复效果；本轮实机验收结论另依据下方用户反馈。

第二轮通过 python-run 执行与截图相同的 `rt.drive_move_for("forward", 10, "rotations")`：

```text
state=completed
error=none
duration_ms=7859
timeout_ms=0
result_value=0
```

程序结束后再次读取设备状态，A-D 全部 `state:coast power:0`。固件 SHA-256 复核一致。

## 用户验收结论

- 日期：2026-09-03。
- 用户原话：“可以通过了”。
- 结论：M1.22Y 本轮双电机追赶振荡修复通过实机验收，停止继续调参。
- 已记录的真机路径：B/C 大型电机、50%、前进 10 圈，以及同等 Python 调用；程序正常完成后 A-D 全部 COAST、power 0。
- 用户未逐项说明外力的施加端口、时长，以及其他速度与方向，不将本轮反馈扩写成完整测试矩阵已全部通过。
- 原图等价 Python 文件：`tools/est_hid_sender/examples/m122y_drive_10_rotations_smoke.py`，默认 B/C、50%。

## 后续回归范围

20%、80%、反向，以及运行中 Back / HID STOP 等专项真机记录可在后续相关变更时补充；本次不自动再运行马达或重新刷写。

验收通过不等于授权提交或推送 Git。当前保留所有未提交成果，等待用户下一步指示。
