# EST MicroPython RAM 程序进度

日期：2026-08-28

## 当前结论

当前设备运行 `M1.02A`，官方 MicroPython `v1.29.0` 已经真实集成、启动并执行电脑上传的 Python 源码；统一 C 服务 tick、传感器/按键/电池只读 API、单马达 API，以及双马达/底盘首轮自动实机测试均已完成。

`M1.02A` 新增 `MotorPair`、`DriveBase`、同型号配对限制和双电机 `wait=True`。A/C 双大型两次完整自动测试均通过，用户确认肉眼未见问题；对象停止、电脑主动停止和异常清理也已完成实机验收。测试脚本位于 `tools/est_hid_sender/examples`。

- APP 起点仍为 `0x08010000`，现有 `03.00B` Bootloader 和 `APP=` 升级格式不变。
- 应用协议为 `1.15`；`0x23` 查询解释器健康，`0x24` 管理 RAM Python 程序。
- Python 堆为 48384 字节；M0.94A 冷启动实测剩余 45728 字节，启动 10 ms，最大 GC 停顿 593 us，自检值 96。
- RAM 源码最大 8192 字节，只在本次开机有效；分块必须连续，并由设备核对 CRC32。
- 单次硬执行上限为 100-10000 ms。
- 正常结束、Python 异常、主动停止和硬超时都会停止全部马达。

## 实机结果

| 测试 | 结果 |
| --- | --- |
| 计算脚本 | 101 字节源码，CRC32 双端一致，4 ms 完成，结果 85344 |
| Python 异常 | 1 ms 捕获，状态 exception，清理标志有效 |
| 死循环硬超时 | 300 ms 精确终止，状态 timed-out，设备未重启或失联 |
| 电脑主动停止 | 10 秒死循环约 971 ms 被停止，最终 run_count=1、flags=0x05 |
| 清理后硬件 | A-D 均为 coast/power 0；四路传感器约 2 秒重新同步 |
| 清理后再次执行 | 同一计算脚本再次返回 85344 |
| M0.97A 统一服务 tick | 1 秒忙循环运行 1003 ms，runtime tick 增加 1001 次 |
| M0.97A UART 连续接收 | 红外端口接收计数在忙循环期间从 3285 增至 6030 |
| M0.97A 回归 | 计算脚本 3 ms 返回 85344；300 ms 超时精确；显式停止约 333 ms 进入最终状态 |
| M0.98A 正式只读 API | 四个 `Sensor` 对象、按键、电池和非法端口异常全部通过；1015 字节脚本 14 ms 返回 6 |
| M0.98A 解释器指标 | 堆剩余 45648 字节，启动 12 ms，最大 GC 停顿 599 us，自检值 96 |
| M0.98A tick 回归 | 1002 ms 内 tick 增加 1001 次；红外 UART 从 15134 增至 15611 |
| M0.99A `Motor` API | A/C 大型、B 空、D 中型全部匹配；状态字段、非法端口/停止方式及四路 `BRAKE -> COAST` 通过；959 字节脚本 15 ms 返回 4 |
| M0.99A 解释器指标 | 堆剩余 45632 字节，启动 13 ms，最大 GC 停顿 599 us，自检值 96 |
| M1.00A A 口大型马达 | 开环 20%、闭环 20%、六次强制 GC 和相对 `+90°` 全部通过；1393 字节脚本 2187 ms 返回 22090，即最大速度 22% 且角度增量 90° |
| M1.00A D 口中型马达 | 闭环 20% 和相对 `-90°` 通过；678 字节脚本 1133 ms 返回 41090，即最大速度 41% 且角度增量 -90° |
| M1.00A 退出清理 | 故意 Python 异常 403 ms、10 秒硬超时和电脑主动停止 7383 ms 均为 flags=0x05；每次退出后 A-D 全部 coast/power 0 |
| M1.00A 解释器指标 | 堆总量 48384 字节，剩余 47536 字节，启动 12 ms，最大 GC 停顿 602 us，自检值 96 |
| M1.01A A 口大型定时 | 30% 正转 1200 ms 自动滑行、-30% 反转 1000 ms 自动刹车；13 次强制 GC 下计时准确，脚本 2220 ms 返回 12001013 |
| M1.01A D 口中型定时 | 25% 正反转各 800 ms，分别自动滑行和自动刹车；脚本 1610 ms 返回 800800 |
| M1.01A 取消与异常 | 5 秒任务在 400 ms 主动取消；运行中故意异常在 403 ms 得到 flags=0x05，超过原 5 秒截止点后仍为 coast/power 0 |
| M1.01A 解释器指标 | 堆总量 48384 字节，剩余 47696 字节，启动 12 ms，最大 GC 停顿 599 us，自检值 96 |
| M1.02A 双电机 API | A/C 双大型完成 80/40% 定时速度、80% 同步 `720°/720°`、-80% 定时直行和 `+50` 定量转向；A/D 大型/中型混合配对在动作前明确拒绝 |
| M1.02A GC 与同步 | 同步角度期间完成 7334 次强制 GC，最大同步误差 11°；完整脚本 5445 ms、state=completed、error=none |
| M1.02A 重复测试 | 第二次完整脚本 5670 ms，7318 次强制 GC，最大同步误差 28°；state=completed、error=none，用户确认肉眼未见问题 |
| M1.02A 对象停止 | 完成 `+360°/+360°` 后启动 80% 持续速度，运行 800 ms 调用 `MotorPair.stop(BRAKE)`；脚本内运行状态及 A/C 功率归零断言通过，整体 1781 ms 完成 |
| M1.02A 异常清理 | A/C 以 60% 持续运行 500 ms 后故意抛出异常；504 ms 进入 exception，flags=0x07，随后 A-D 全部 coast/power 0 |
| M1.02A 电脑主动停止 | A/C 以 60% 持续运行时由电脑发送 `python-stop`；952 ms 进入 stopped，flags=0x07，随后 A-D 全部 coast/power 0 |
| M1.02A 结束状态 | A-D 均为 coast/power 0；四路传感器 streaming；解释器 state=passed、堆剩余 46928 字节、最大 GC 停顿 601 us；RAM 程序已清空 |

M0.95A 已完成正常执行、异常和硬超时，但 Python VM 运行时没有轮询 USB，电脑主动停止会写入超时。M0.96A 修复 USB 主动停止，M0.97A 建立统一 C 服务 tick，M0.98A 开放正式只读传感器、按键和电池 API，M0.99A 开放马达状态与安全停止，M1.00A 开放首批单马达运行方法，M1.01A 补齐 C 层单马达计时，M1.02A 开放双马达/底盘。因此后续开发和恢复应使用 M1.02A，不应回退到 M0.95A。

## 使用命令

```powershell
python tools/est_hid_sender/est_hid_sender.py micropython-status
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/compute_result.py --timeout-ms 2000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/runtime_service.py --timeout-ms 2000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/read_services.py --timeout-ms 3000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/read_motors.py --timeout-ms 2000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/run_single_motor.py --timeout-ms 8000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/run_medium_motor.py --timeout-ms 8000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/run_timed_motor.py --timeout-ms 6000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/run_timed_medium_motor.py --timeout-ms 5000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/test_motor_pair_drive.py --timeout-ms 10000
python tools/est_hid_sender/est_hid_sender.py python-program-status
python tools/est_hid_sender/est_hid_sender.py python-stop
python tools/est_hid_sender/est_hid_sender.py python-clear
```

可重复测试脚本位于 `tools/est_hid_sender/examples`。

## 当前限制

- `Sensor`、`buttons`、`battery`、`Motor`、`MotorPair` 和 `DriveBase` 已完成对应阶段的实机自动验证，包括双电机对象停止、电脑主动停止和异常清理。各传感器便捷类型类、模式切换和重置仍未开放。
- `Motor` 已支持 `run_power()`、`run_speed()`、`run_time()`、`run_angle()`、`reset_angle()` 和 `stop()`。`HOLD` 以及角度完成后的 `STOP_BRAKE` 仍未实现，并会明确抛出不支持异常。
- `run_time()` 接受 `1..600000 ms`，但当前 RAM 程序本身的硬执行上限为 10000 ms；需要在脚本内等待完成，脚本直接返回会触发统一清理并停止马达。
- 用户脚本执行期间，电机闭环、传感器解析、按键、电池、背光、音频维护、协议超时、USB 与看门狗都持续运行；LCD 页面刷新仍在主循环，忙循环期间暂不刷新。
- 脚本入口仍会先停止全部马达；大型和中型马达已验证普通循环、强制 GC、正常结束、异常、硬超时和主动停止路径。
- Python 标准输出当前被丢弃，运行结果暂以 `_program_result(int32)` 测试接口和状态码返回。
- 源码未写入外部 Flash，重启后不会保留。

## 下一步

1. 开放传感器便捷类型类、模式操作和重置，并逐类完成实机回归。
2. 设计 LCD 刷新与 VM 并行方案，再开放 `Display`、`LED` 和背光用户 API。
3. 设计用户程序持久化和启动策略；在该策略完成前继续使用 RAM 上传，避免把不稳定脚本写入 Flash 自动运行。

## 构建与发布

M1.01A 上一实机基线：

- 发布包：`firmware/releases/M1.01A/est_minimal_upgrade_app_M1.01A.upgrade.bin`
- manifest：`firmware/releases/M1.01A/est_minimal_upgrade_app_M1.01A.manifest.json`
- SHA-256：`6d817cce5c2628c0cf081fa4eb5332cb012eb7b9ff5b0480e7d7f8236e8ca9ea`
- 固件测试：78 项通过。
- HID 工具测试：112 项通过。
- ARM GCC 12.2.1 `-Werror`、向量表、包头、固定 256 KiB 长度和 manifest 校验通过。

M1.02A 当前实机包：

- 发布包：`firmware/releases/M1.02A/est_minimal_upgrade_app_M1.02A.upgrade.bin`
- manifest：`firmware/releases/M1.02A/est_minimal_upgrade_app_M1.02A.manifest.json`
- SHA-256：`f203a73428054c151fc92a1b25374484c851ab96f3d5702f2e3269dcbebe2610`
- 固件测试：79 项通过；HID 工具测试：112 项通过。
- ARM GCC 12.2.1 `-Werror`、向量表、`APP=` 包头、固定 256 KiB 长度和 manifest 校验通过；双电机动作、对象停止、异常清理和电脑主动停止均已实机通过。
