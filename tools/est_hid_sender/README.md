# EST HID 固件升级工具

该工具用于 Windows 上的 EST USB HID 心跳检测和固件升级。通信、协议、固件解析与升级流程分别位于独立模块中，保留现有 HID 协议、帧格式和逐包 ACK 机制。

## 常用命令

以下命令都在仓库根目录执行。

读取设备当前版本：

```powershell
python tools/est_hid_sender/est_hid_sender.py ping
```

只读检测外部 Flash 型号（不会擦除或写入）：

```powershell
python tools/est_hid_sender/est_hid_sender.py flash-id
```

已知型号映射包括 `EF4017 -> W25Q64` 和实物确认的
`EF4019 -> W25Q256JV`（32 MiB）。未知编号仍会显示原始 JEDEC ID、厂商和容量，
并以 `model_known=no` 标记。

只读检查测试区、保护位和 4 字节模式：

```powershell
python tools/est_hid_sender/est_hid_sender.py flash-scan
python tools/est_hid_sender/est_hid_sender.py flash-status
python tools/est_hid_sender/est_hid_sender.py flash-mode-probe
```

执行可恢复的高地址读写测试：

```powershell
python tools/est_hid_sender/est_hid_sender.py flash-test
```

`flash-test` 不是只读命令。它会先确认设备为 W25Q256JV 且最后一个 4 KiB 扇区为空，再临时写入 32 字节、读回核对、检查低地址没有被误写，最后擦除并复查为空。

测试输出 A 口马达：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-test
```

执行前必须保证马达轴可以自由转动。该命令会让 A 口以低速正转、暂停、反转并停止；固件还会在升级、关机或命令异常结束时强制停转。此命令要求设备运行 `M0.33A` 或更新版本。

同时测试 A 口马达和内部测速反馈：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-tacho-test
```

指定测试 B、C 或 D 口：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-tacho-test --port D
```

该命令除执行相同的安全正反转动作外，还会显示正转、反转和总计数，并自动判断是否读到运动以及两个方向的符号是否相反。此命令要求设备运行 `M0.34A` 或更新版本。

`M0.35A` 起可以指定测试功率，例如短时间全功率测试：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-tacho-test --power 100
```

功率范围为 1%-100%。80% 及以上会自动缩短正反转时间并延长中间关闭驱动时间，减少高速换向冲击。

`M0.43A` 起可使用面向新上位机的通用马达控制命令。单端口短时运行：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-control --port A --power 30 --duration 1 --stop coast
python tools/est_hid_sender/est_hid_sender.py motor-control --port D --power -30 --duration 1 --stop brake
```

`--power` 为 `-100..+100` 的有符号功率，正负号代表两个方向；`--stop` 可选 `coast`（自由滑行）或 `brake`（主动刹车）。命令会显示设定功率和累计测速计数，并在指定时间后发送停车命令。

同时独立控制两个端口：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-pair-control `
  --first-port A --first-power 30 `
  --second-port D --second-power -30 `
  --duration 1 --stop coast
```

底层 `0x17` 命令还提供状态读取和测速计数清零。通用控制不设置固件自动超时：设定功率会保持到收到下一条功率、自由滑行或刹车命令；升级开始、关机和旧诊断停止命令仍会关闭全部输出。新上位机应明确管理启动与停止，不依赖固件定时续约。

只查看升级包信息，不连接设备：

```powershell
python tools/est_hid_sender/est_hid_sender.py info --file firmware/releases/M0.21A/est_minimal_upgrade_app_M0.21A.upgrade.bin
```

严格核对升级包头、大小、SHA256 和 manifest，不连接设备：

```powershell
python tools/est_hid_sender/est_hid_sender.py verify --file firmware/releases/M0.21A/est_minimal_upgrade_app_M0.21A.upgrade.bin
```

发送升级包：

```powershell
python tools/est_hid_sender/est_hid_sender.py flash --file <目标固件.upgrade.bin>
```

工具会自动寻找与 `.upgrade.bin` 同名的 `.manifest.json`，发送前显示当前设备版本和目标固件版本。同版本重刷或降级会被阻止，确认操作后可加 `--force`。旧升级包没有 manifest 时，可显式使用 `--allow-missing-manifest`，但这会失去目标版本和完整性保护。

每次刷写默认在 `%LOCALAPPDATA%\ESTHidSender\logs` 保存日志。可用 `--log-dir <目录>` 修改位置，或用 `--no-log` 禁用。

## 错误分类

CLI 会输出稳定的错误代码，便于人工判断和后续 GUI 调用：

- `device-not-found`：USB HID 未枚举；
- `heartbeat-timeout`：设备已枚举，但心跳超时；
- `ack-timeout`：指定数据包 ACK 超时；
- `ack-rejected`：设备返回的 ACK flag 不是 1；
- `firmware-invalid`、`manifest-not-found`、`manifest-mismatch`：升级包或 manifest 校验失败；
- `version-safety`：同版本重刷或降级被安全检查阻止。

## 测试

```powershell
python -m unittest discover -s tools/est_hid_sender/tests -v
python -m unittest discover -s firmware/minimal_upgrade_app/tests -v
```

升级工具测试使用模拟 HID 传输，不会向实机发送数据。只有人工执行 `flash` 或 `flash-test` 才会向实机写入数据；所有 `motor-*` 命令都不写 Flash，但相关控制命令会让实物马达转动。
