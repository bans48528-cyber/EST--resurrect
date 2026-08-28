# EST MicroPython RAM 程序进度

日期：2026-08-28

## 当前结论

当前设备运行 `M0.96A`，官方 MicroPython `v1.29.0` 已经真实集成、启动并执行电脑上传的 Python 源码，不只是预留文件或接口。

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

M0.95A 已完成正常执行、异常和硬超时，但 Python VM 运行时没有轮询 USB，电脑主动停止会写入超时。M0.96A 在 VM 中每 1 ms 轮询 USB，并在脚本运行期间只接受 `0x24` 状态/停止命令，已修复该问题。因此后续开发和恢复应使用 M0.96A，不应回退到 M0.95A。

## 使用命令

```powershell
python tools/est_hid_sender/est_hid_sender.py micropython-status
python tools/est_hid_sender/est_hid_sender.py python-run --file tools/est_hid_sender/examples/compute_result.py --timeout-ms 2000
python tools/est_hid_sender/est_hid_sender.py python-program-status
python tools/est_hid_sender/est_hid_sender.py python-stop
python tools/est_hid_sender/est_hid_sender.py python-clear
```

可重复测试脚本位于 `tools/est_hid_sender/examples`。

## 当前限制

- 内建 `est` 模块目前主要用于最小验证，RAM 脚本尚不能启动或控制马达。
- 用户脚本执行期间，VM 目前只维持 USB `0x24` 与看门狗；普通电机闭环、传感器解析、按键、电池和显示 tick 暂停。
- 因为脚本入口会先停止全部马达，并且尚未开放马达启动 API，当前最小阶段没有让运行中的马达失去闭环；但在开放正式 Motor API 前必须解决统一 C 服务 tick。
- Python 标准输出当前被丢弃，运行结果暂以 `_program_result(int32)` 测试接口和状态码返回。
- 源码未写入外部 Flash，重启后不会保留。

## 下一步

1. 建立可从 VM hook 调用的统一 C 服务 tick，持续更新电机闭环、传感器、按键、电池和必要的系统任务。
2. 测量 Python 循环和强制 GC 期间的 C 控制周期抖动，先保证急停与电机安全，再开放控制 API。
3. 把私有 `_program_result` 验证接口逐步替换为正式的 `Motor`、`MotorPair`、`Sensor`、`Display`、`Buttons`、`LED` 和 `Battery` API。
4. 设计用户程序持久化和启动策略；在该策略完成前继续使用 RAM 上传，避免把不稳定脚本写入 Flash 自动运行。

## 构建与发布

- 发布包：`firmware/releases/M0.96A/est_minimal_upgrade_app_M0.96A.upgrade.bin`
- manifest：`firmware/releases/M0.96A/est_minimal_upgrade_app_M0.96A.manifest.json`
- SHA-256：`70428ced1590117f45bdaebe6d4b761498345f2bc3d302fee42e5cfd2dc78ee5`
- 固件测试：75 项通过。
- HID 工具测试：112 项通过。
- ARM GCC 12.2.1 `-Werror`、向量表、包头、固定 256 KiB 长度和 manifest 校验通过。
