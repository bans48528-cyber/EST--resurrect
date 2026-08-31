# EST 第 2 批下位机收口记录

日期：2026-08-31
基线：M1.22D 工作区（包含未提交的 P0 看门狗与 min/max 成果）
候选版本：M1.22E
协议：1.26（未修改）
能力位：未新增

## 范围

本批只修改下位机，不修改 EST Studio，不升级真机，不提交或推送 Git。
现有未提交文档、示例、看门狗和 min/max 修改均保留。

## 电机 runtime API

冻结模块 `est_runtime.py` 新增：

```python
rt.motor_start_speed(port, speed)
rt.motor_start_power(port, power)
```

统一行为如下：

1. 端口先转为字符串和大写，只接受 A、B、C、D。
2. 速度和功率先执行 `int()`，再截断到 `-100..100`。
3. 设置型速度继续使用绝对值，并截断到 `0..100`。
4. 速度 0 和 1..9 原样进入已有闭环，不提高到 10。
5. 每条连续命令都经过 `_begin_motor_command()`，记录任务 owner、generation
   和控制类型。
6. 同一端口连续 speed 更新直接重设目标，20→0→75 不插入停止。
7. speed 与 power 类型互换时先 COAST 旧控制，再由新 generation 接管。
8. 新任务接管端口后，旧任务结束只释放自己仍拥有的 generation，不会停止
   后来的命令。
9. native 启动异常会撤销失败 generation；未捕获异常、Back、HID STOP 和
   断线继续使用既有 C 层全局安全清理。
10. 旧 `motor_set_speed()` + `motor_start()` 保持可用，并复用新的 speed 入口。

已审计原先依赖 Studio `_est_speed()` 或 `_est_speed_magnitude()` 的可执行
runtime 入口：`motor_set_speed`、`motor_run_for`、`drive_set_speed`、
`drive_steer_for` 和 `drive_start_steer` 均已由 runtime 自行截断。尚未实现的
`drive_dual_speed_for` 与 `drive_start_dual_speed` 继续明确抛
`NotImplementedError`，本批不扩展功能。

旧 Studio 已保存的 `rt.motor(port).run_speed/run_power` 程序仍可运行，因为
原生 Motor API 未删除或改变；但只有改用新 runtime API 的生成代码才具备
多任务 owner/generation 接管保护。

## display.text_line 兼容修复

补齐既有 Studio 与接口映射文档声明的：

```python
est.display.text_line(line, text)
```

- `line` 只接受 1..12，越界抛 `ValueError`，绘制前不会访问 framebuffer。
- `text` 必须为字符串，否则抛 `TypeError`。
- 行坐标为 `(line - 1) * 10`，每行只清除自身 `180x10` 区域。
- 使用现有普通 5x7 字体和裁剪逻辑，不改变 `est.display.text()`。
- `text_line()` 写入后立即执行带 VM hook 的 LCD refresh，匹配 Studio 当前
  不额外生成 `refresh()` 的契约。

对应 smoke 为：

- `tools/est_hid_sender/examples/m122e_display_text_line_smoke.py`
- `tools/est_hid_sender/examples/m122e_runtime_motor_api_smoke.py`

## Studio 后续门禁

新 API 和 `display.text_line()` 的最低固件版本均为 **M1.22E**。本批未增加
capability，因此 Studio 后续应在生成代码使用 `motor_start_speed()`、
`motor_start_power()` 或 `display.text_line()` 时，依据设备上报版本执行
`>= M1.22E` 下载前门禁；旧固件必须提示升级，不能下载后再显示 E3。

## 验证

- frozen runtime 聚焦测试：41/41。
- 完整固件测试：246/246。
- HID 工具测试：127/127。
- 两个 MicroPython smoke 通过 CPython 语法检查和 frozen MPY 构建。
- GNU Arm Toolchain 12.2.1，C 编译启用 `-Werror`，构建通过。
- manifest、APP 地址、向量表、`APP=` 包头和固定包长度严格校验通过。
- `git diff --check` 通过。

测试覆盖动态 `+101/-101`、20→0→75、speed/power 切换、端口规范化、
多任务接管、旧任务清理、native 启动失败清理、零速/低速旧契约、HOLD、
Back、HID STOP、断线和异常安全路径的既有回归。

参考 Pybricks 的 API 职责与“新命令替代当前连续运行目标”语义独立实现，
未复制其源码：

- https://docs.pybricks.com/en/latest/pupdevices/motor.html
- https://pybricks.com/learn/intro/story-mission/
- https://github.com/pybricks/pybricks-micropython/blob/master/CHANGELOG.md

## M1.22E 候选包

- 构建目录：`firmware/minimal_upgrade_app/build/m122e_runtime_api_candidate_gcc12`
- raw APP：280260 字节（比 M1.22D 增加 504 字节）
- text/data/bss：278540 / 1720 / 70952 字节
- APP 区剩余：178492 字节
- 未填充包：280264 字节
- 固定升级包：327680 字节
- 固定升级包剩余：47420 字节
- 包头：`APP=`
- SHA-256：`57e5531988bc81bddcc114b0ea9e4cfe308688221a0523cd205fd4c5c9c6ab97`
- 升级包：`firmware/minimal_upgrade_app/build/m122e_runtime_api_candidate_gcc12/est_minimal_upgrade_app.upgrade.bin`

## 真机验收

2026-08-31 已完成升级与基础验收：

- 从设备上的临时候选 `M1.22T` 强制升级，325/325 帧完成，manifest 与
  SHA-256 校验一致。
- 设备重新枚举后上报 `M1.22E`、协议 `1.26`，能力集合未变化。
- B 口大型电机运行 `m122e_runtime_motor_api_smoke.py`，完成 20→0→75、
  `power=101` 截断和跨任务接管，结果 122，无 E3。
- `m122e_display_text_line_smoke.py` 在第 1 行和第 12 行写字，结果 122，
  无 AttributeError/E3，确认 Studio 现有调用可执行。
- 无限协作程序运行期间发送 HID STOP，状态从 `running` 进入
  `stopped/error=stopped`；随后 A-D 均为 `coast/power 0`。
- 4 号口颜色传感器在验收后继续保持 streaming。

候选包已完成真机基础验收；提交和推送状态以 Git 提交记录为准。
