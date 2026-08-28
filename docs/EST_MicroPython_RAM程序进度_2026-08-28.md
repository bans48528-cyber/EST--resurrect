# EST MicroPython RAM 程序进度

日期：2026-08-28

## 当前结论

当前设备运行 `M0.97A`，官方 MicroPython `v1.29.0` 已经真实集成、启动并执行电脑上传的 Python 源码；Python 占用 VM 时，统一 C 服务 tick 也已完成实机连续性验证。

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

M0.95A 已完成正常执行、异常和硬超时，但 Python VM 运行时没有轮询 USB，电脑主动停止会写入超时。M0.96A 修复了 USB 主动停止；M0.97A 再把电机、传感器、按键、电池、背光、音频和协议超时收口到 VM 与主循环共用的服务 tick。因此后续开发和恢复应使用 M0.97A，不应回退到 M0.95A。

## 使用命令

```powershell
python tools/est_hid_sender/est_hid_sender.py micropython-status
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/compute_result.py --timeout-ms 2000
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/runtime_service.py --timeout-ms 2000
python tools/est_hid_sender/est_hid_sender.py python-program-status
python tools/est_hid_sender/est_hid_sender.py python-stop
python tools/est_hid_sender/est_hid_sender.py python-clear
```

可重复测试脚本位于 `tools/est_hid_sender/examples`。

## 当前限制

- 内建 `est` 模块目前主要用于最小验证，RAM 脚本尚不能启动或控制马达。
- 用户脚本执行期间，电机闭环、传感器解析、按键、电池、背光、音频维护、协议超时、USB 与看门狗都持续运行；LCD 页面刷新仍在主循环，忙循环期间暂不刷新。
- 脚本入口仍会先停止全部马达，正式马达启动 API 尚未开放；开放后还需要用真实马达验证普通循环、强制 GC、停止和异常路径的控制周期。
- Python 标准输出当前被丢弃，运行结果暂以 `_program_result(int32)` 测试接口和状态码返回。
- 源码未写入外部 Flash，重启后不会保留。

## 下一步

1. 先把私有验证接口逐步替换为正式只读 `Sensor`、`Buttons` 和 `Battery` API。
2. 再开放 `Motor`、`MotorPair` 和底盘动作 API，并验证 Python 循环、强制 GC、显式停止和异常时的控制周期与安全清理。
3. 设计 LCD 刷新与 VM 并行方案，再开放 `Display`、`LED` 和背光用户 API。
4. 设计用户程序持久化和启动策略；在该策略完成前继续使用 RAM 上传，避免把不稳定脚本写入 Flash 自动运行。

## 构建与发布

- 发布包：`firmware/releases/M0.97A/est_minimal_upgrade_app_M0.97A.upgrade.bin`
- manifest：`firmware/releases/M0.97A/est_minimal_upgrade_app_M0.97A.manifest.json`
- SHA-256：`735e1047419c71fa168934ade4576f115c3c8d8a7edf519cceeade5b980fdf34`
- 固件测试：76 项通过。
- HID 工具测试：112 项通过。
- ARM GCC 12.2.1 `-Werror`、向量表、包头、固定 256 KiB 长度和 manifest 校验通过。
