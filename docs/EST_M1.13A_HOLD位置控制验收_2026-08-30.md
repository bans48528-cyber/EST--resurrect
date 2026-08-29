# EST M1.13A HOLD 位置控制验收

日期：2026-08-30

## 基线信息

| 项目 | 结果 |
| --- | --- |
| 固件版本 | `M1.13A` |
| USB 应用协议 | `1.22` |
| 新设备能力 | `hold-position-control`，bit 21，`0x00200000` |
| 升级包 | `firmware/releases/M1.13A/est_minimal_upgrade_app_M1.13A.upgrade.bin` |
| SHA-256 | `a0b7b9f33f75aaab6d8085f3f749b4b819e5c915ae0f5ab5a0ef773fa145c93e` |
| 原始 APP | 248384 字节 |
| 未填充包 | 248388 字节 |
| 固定升级包 | 262144 字节 |

HOLD 是每个电机端口独立运行的编码器位置 PD 闭环。它不是电气 BRAKE，也不等同于速度 0；进入 HOLD 后 Python 调用正常返回，控制器继续在 10 ms C tick 中运行，直到新命令、COAST/BRAKE、拔出或统一安全清理取消。

## 自动验证

- 固件测试：126 项通过。
- HID 工具测试：120 项通过。
- ARM GCC 14.2.1：启用 `-Werror` 构建通过。
- 固定 256 KiB 升级包与 manifest 严格校验通过。
- 设备启动后返回 `firmware=M1.13A`、`protocol=1.22`，能力列表包含 `hold-position-control`。

## 实机接线

- A：大型电机。
- C：大型电机。
- D：中型电机。
- B：空口。

## 实机结果

| 验收项 | 结果 |
| --- | --- |
| A 大型显式 `stop(HOLD)` | 多轮测试有阻力并能回位，用户确认整体无问题 |
| D 中型显式 `stop(HOLD)` | 调整中型静摩擦恢复参数后有阻力并能回位，用户确认通过 |
| MotorPair 定时 HOLD | A/C 同时进入 HOLD，Python 继续执行；两侧受外力后保持并回位 |
| MotorPair 角度 HOLD | A/C 保持原角度命令目标；两侧受外力后保持并回位 |
| MotorPair 新命令与 COAST | 新速度命令正常接管旧 HOLD，显式 COAST 后两侧释放 |
| DriveBase 定时/角度 HOLD | A/C 两条路径均保持并回位，用户确认通过 |
| HOLD 中拔出 | C 口拔出后报告 `type:none`、tacho 归零、coast/power 0；重新插入后恢复大型识别 |
| 正常结束 | 验收程序完成后 A-D 全部 coast/power 0 |
| 电脑 STOP | 无限等待程序在活动 HOLD 中收到 STOP，状态为 stopped，A-D 全部 coast/power 0 |
| Python 异常 | 活动 HOLD 后故意抛异常，状态为 python-exception，A-D 全部 coast/power 0 |
| 零速/低速回归 | 结果码 `150`；20→0→75、1/5/9、0/50、50/0、0/0、切回 50/50 和零速角度等待均通过 |

## 保持的契约

- 速度 0 表示零速度闭环，不自动转换为 HOLD。
- 速度 1..9 不抬升到 10，也不抛参数异常。
- 定时速度 0 仍等待完整时间；非零角度配速度 0 持续等待 STOP。
- 新命令自动取消同端口旧 HOLD。
- 显式 COAST/BRAKE 取消 HOLD。
- 程序正常结束、电脑 STOP、异常、紧急停止和固件升级清理统一 COAST。
- HOLD 没有自动超时；电机拔出时立即取消并清除端口状态。
- 本阶段未修改 EST Studio，也未改变开环 `run_power` 行为。

## 验收程序

- `tools/est_hid_sender/examples/hold_position_large_a.py`
- `tools/est_hid_sender/examples/hold_position_medium_d.py`
- `tools/est_hid_sender/examples/hold_position_pair_ac.py`
- `tools/est_hid_sender/examples/hold_position_drive_ac.py`
- `tools/est_hid_sender/examples/hold_position_unplug_c.py`
- `tools/est_hid_sender/examples/hold_position_stop_cleanup.py`
- `tools/est_hid_sender/examples/hold_position_exception_cleanup.py`
- `tools/est_hid_sender/examples/zero_speed_motor_control_smoke.py`
