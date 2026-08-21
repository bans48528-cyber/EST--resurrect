# EST HID 固件升级工具

该工具用于 Windows 上的 EST USB HID 心跳检测和固件升级。通信、协议、固件解析与升级流程分别位于独立模块中，保留现有 HID 协议、帧格式和逐包 ACK 机制。

## 常用命令

以下命令都在仓库根目录执行。

读取设备当前版本：

```powershell
python tools/est_hid_sender/est_hid_sender.py ping
```

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

升级工具测试使用模拟 HID 传输，不会向实机发送数据。固件端代码未因本工具改造而修改。
