# EST HID 固件升级工具

该工具用于 Windows 上的 EST USB HID 设备检测、功能验证和固件升级。通信、协议、固件解析与升级流程分别位于独立模块中，保留现有 HID 协议、帧格式和逐包 ACK 机制。

## 常用命令

以下命令都在仓库根目录执行。

读取设备当前版本：

```powershell
python tools/est_hid_sender/est_hid_sender.py ping
```

`M0.52A` 起可一次读取新上位机需要的整机状态：

```powershell
python tools/est_hid_sender/est_hid_sender.py device-status
```

输出包括固件与应用协议版本、支持功能、电量、按键、A-D 马达状态和 1-4 号输入口状态。该命令只读，不会让马达转动，也不会改变传感器模式。精确帧定义见 `docs/EST_USB应用协议_V1.md`。

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

`M0.62A` 起可按已识别的电机类型运行指定角度或圈数：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-position --port A --speed 30 --degrees 360
python tools/est_hid_sender/est_hid_sender.py motor-position --port A --speed 30 --rotations -1
```

`--degrees` 与 `--rotations` 必须二选一；负数代表反转。固件按大型/中型电机分别选择测速和减速参数，短行程会自动降低有效转速。单次范围限制为 `±3600°` 或 `±10` 圈，执行中会回报实测转速、当前计数和剩余误差；到位后短暂主动刹车并自动自由滑行，堵转或计数异常会超时停机。

`M0.65A` 起提供不转动马达、不重启设备的单口类型刷新命令；应使用已修正识别时序和低相位窗口的 `M0.67A` 或更高版本：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-identify --port B
```

刷新前必须确保 A-D 四路马达均已停止。`M0.68A` 起会在 H 桥关闭时依次采集 pin 6 浮空、pin 6 拉低和 pin 5 弱上拉三组 ADC 数据，随后恢复输入模式和原测速计数；电脑端打印 `id_mv`、`pin6_low_mv` 和 `pin5_pullup_mv`。`M0.71A` 起还打印 `pin5_pullup_high` 数字状态；电脑端兼容协议 1.1-1.5 的 21/53/57 字节回包。M0.72A 根据受控换口实测和 EST 3.0 官方类别顺序，把弱上拉低相位映射修正为大型、中型、空口依次升高。

按已识别的大型或中型马达类型持续闭环定速：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-speed --port B --speed 30 --duration 5 --stop coast
python tools/est_hid_sender/est_hid_sender.py motor-speed --port B --speed -30 --duration 5 --stop brake
```

`--speed` 接受 `-100..-10` 或 `10..100`，`--duration` 限制为 0.5-30 秒。电脑端会持续读取实测转速和输出功率，并在结束或异常时发送停车命令；执行前仍须保证马达轴及周围机构可以安全转动。

`M0.75A` 起可让两路已识别的大型或中型马达按编码器进度差同步运行：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-pair-position --left-port A --right-port C --left-degrees 360 --right-degrees -360 --speed 20
```

`M0.86A` 起两路目标角度可以不同，但都必须非零且位于 `±3600°`；正负号可分别指定同向或反向。长行程侧按指定最大速度运行，短行程侧按目标角度比例降低基础速度；同步误差是投影到较长目标后的等效角度差。默认端口为当前正式验收使用的 A/C 双大型马达，但固件不会强制两路类型相同。电脑端正常或异常退出时都会发送最终自由滑行命令。

`M0.76A` 起双电机同步任务不再设置自动时间上限，电脑端也会持续等待到两路完成或用户中断。受阻电机松开后继续追赶目标；永久受阻时必须由用户用 `Ctrl+C` 或上层显式停止命令结束。升级和关机路径仍会强制关闭全部马达。

双电机同步状态行中的 `speed=A,C` 是两路编码器实测速度百分比，`power=A,C` 是当时实际 PWM 百分比。它们用于区分目标速度、真实转速和闭环为抵抗负载施加的功率。

`M0.83A` 起可让一组双电机持续维持闭环转速，直到收到明确的停止命令；`M0.87A` 起两路可分别设置绝对值 `10..100%` 的独立有符号速度。固件按目标速度比例投影两侧编码器进度和同伴限速，例如 `40%/20%` 按 `2:1` 行程比较，不会被纠正成同速。允许用户明确选择大型/中型混合配对。`--duration` 仅决定电脑端观察多久，结束后工具会发送 `--stop coast` 或 `--stop brake`，固件自身没有超时：

```powershell
python tools/est_hid_sender/est_hid_sender.py motor-pair-speed --left-port A --right-port C --left-speed 40 --right-speed 20 --duration 10 --stop coast
```

`M0.88A` 起新增与 EV3 Classroom 移动积木一致的持续转向命令。`--steering` 为 `-100..100`，`--speed` 为有符号移动速度；转向 `0` 为直行，`+50` 配合速度 `80` 得到左/右 `80%/40%`，`-50` 得到 `40%/80%`，`±100` 为两轮反向的原地旋转。该命令不使用轮径、轮距或机身角度：

```powershell
python tools/est_hid_sender/est_hid_sender.py drive-steer --left-port A --right-port C --steering 50 --speed 80 --duration 5 --stop coast
```

持续转向复用双电机比例定速控制，固件没有自动超时，工具会在观察结束或异常退出时显式停止。当前闭环要求换算后的左右速度绝对值均不低于 10%；接近但未达到 `±100` 的转向值可能使内侧轮低于该下限并被拒绝，原地旋转应直接使用 `±100`。

`M0.90A` 起可让同一转向动作按圈数、角度或秒数自动完成。方向由有符号 `--speed` 决定，`--rotations` 和 `--degrees` 表示较快一侧车轮的目标行程，秒数由固件内部计时：

```powershell
python tools/est_hid_sender/est_hid_sender.py drive-steer-for --left-port A --right-port B --steering 50 --speed 40 --rotations 2
python tools/est_hid_sender/est_hid_sender.py drive-steer-for --left-port A --right-port B --steering -50 --speed -40 --degrees 720
python tools/est_hid_sender/est_hid_sender.py drive-steer-for --left-port A --right-port B --steering 100 --speed 40 --seconds 3 --stop brake
```

角度/圈数模式当前完成后自由滑行；时间模式可选择自由滑行或主动刹车。工具会显示换算后的左右速度、左右目标角度和实时进度。换算后任一侧绝对速度低于 10% 时，固件仍会拒绝启动。

`M0.84A` 起可在固件 C 层按轮径把毫米距离换算为两轮同步角度。正距离前进，负距离后退；首版完成后采用自由滑行。`--axle-track` 已进入底盘配置，但当前毫米直行和 M0.88A 的 EV3 转向都不使用它，只为以后确实需要几何换算的接口保留：

```powershell
python tools/est_hid_sender/est_hid_sender.py drive-straight --left-port A --right-port C --wheel-diameter 56 --axle-track 120 --distance 500 --speed 40
```

毫米直行链路已在 M0.84A 实机通过，但最终软件界面按产品要求使用圈、度、秒，不显示毫米。`M0.85A` 起统一使用 `drive-run`，三个目标参数必须三选一。正数前进、负数后退；按秒模式由固件内部计时，到时自动停止：

```powershell
python tools/est_hid_sender/est_hid_sender.py drive-run --left-port A --right-port C --rotations 2.5 --speed 40
python tools/est_hid_sender/est_hid_sender.py drive-run --left-port A --right-port C --degrees -720 --speed 40
python tools/est_hid_sender/est_hid_sender.py drive-run --left-port A --right-port C --seconds 3 --speed 60 --stop brake
```

`M0.44A` 起可读取 EST/EV3 兼容颜色/灰度传感器；`M0.46A` 起支持 1-4 号输入口：

```powershell
python tools/est_hid_sender/est_hid_sender.py sensor-read --port 1 --mode reflect
python tools/est_hid_sender/est_hid_sender.py sensor-read --port 1 --mode ambient
python tools/est_hid_sender/est_hid_sender.py sensor-read --port 1 --mode color
python tools/est_hid_sender/est_hid_sender.py sensor-read --port 4 --mode reflect
python tools/est_hid_sender/est_hid_sender.py sensor-read --port 1 --mode celsius
python tools/est_hid_sender/est_hid_sender.py sensor-read --port 1 --mode fahrenheit
```

持续观察数值可增加 `--watch --duration 10`。输出包括传感器连接状态、型号、模式、读数、两路原始 ADC、接收字节数和通信错误数。M0.44A-M0.45A 只启用 1 号输入口；M0.46A 可分别访问 1-4 号口；M0.49A 起支持温度传感器的 `celsius` 和 `fahrenheit` 模式。

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
