# M1.23F 音频淡入与切音复测

## 背景

M1.23E 相比 M1.23D 已让音符前的小电流杂音略有减轻，但用户反馈改善不明显。
本轮继续只优化声音切音边界，不修改 Studio、协议、能力位、资源表或电机逻辑。

## 修改范围

- 音频播放开始时静音窗口从 35 ms / 512 字节调整为 50 ms / 1024 字节。
- 静音后不再一次性恢复音量，改为按 25%、50%、75%、100% 分四步淡入。
- 如果上一音符处于温和停止的填充阶段，下一音符可以直接接管播放，不再触发硬复位。
- `est.audio.stop()` 继续走温和停止；Back、HID STOP、断线、异常和系统清理仍走硬停止。

## 自动验证

- 音频单测：14 项通过。
- 固件单测：309 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过，保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23F。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：293504 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123f_audio_ramp_candidate/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`2bb2646ab03894dff8f61cef864e27aaf2e17030e2b01b96f0526f13572c746d`。

## 实机状态

M1.23F 已发送成功，325/325 帧全部应答。
日志：`C:/Users/64264/AppData/Local/ESTHidSender/logs/upgrade_20260904_003456_983475.log`。

发送后自动等待 90 秒，未检测到 M1.23F 重新枚举。
最后一次设备探测返回 device-not-found。
当前状态是：升级包已发送，但 M1.23F 尚未开机确认，静音诊断和旋律复测尚未执行。

## 开机后复测

用户开机后，设备已重新枚举并确认：

- firmware=M1.23F。
- protocol=1.26。
- A-D 均为 COAST/power 0。
- `pin_layout=2`，`pin_probe=13`。
- `payload_crc31=1459300450`。
- `dreq_waits=209`。
- 解码采样脚本完成，`result_value=15000859`。
- 旋律播放脚本完成，`result_value=12301`。

脚本层和解码诊断均通过；本轮仍等待用户确认实际听感，尤其是每个音符前的小杂音是否比 M1.23E 更低。
