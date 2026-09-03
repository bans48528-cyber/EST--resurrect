# M1.23H 音频切音软复位候选

## 背景

M1.23G 复测后，用户确认杂音实际出现在音符前沿。
进一步分析发现：内置钢琴 MP3 单个文件时长约 1 秒以上，而测试旋律只播放约 0.45 秒就调用 `est.audio.stop()`。
固件会尽快向 VS1003 送入 MP3 数据，导致下一音符开始时，解码器内部可能仍有上一音符缓存，切音前沿容易出现杂音。

## 修改范围

本轮只修改下位机声音切音边界，不修改 Studio、协议、能力位、资源表或电机逻辑。

- 保留 M1.23G 的启动静音、淡入和尾部淡出。
- 新增内部 `AUDIO_SOFT_RESET` 阶段。
- 当新音符打断当前播放、淡出、填充或排空阶段时，先写静音，再写 `SCI_MODE=0x0804` 触发 VS1003 软件复位，清空解码缓冲。
- 软复位后走原有 mode/status/clock/volume 初始化流程，再送入下一音符。
- 硬复位仍只用于系统级 Back、HID STOP、断线、异常、冷启动和故障路径。

## 自动验证

- 音频单测：14 项通过。
- 固件单测：309 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过，保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23H。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：293824 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123h_audio_soft_reset_candidate/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`80092e489c6a35e76423fa6f5f5bc955a23ffcba727e90941bd909a5856da4cf`。

## 实机状态

M1.23H 已发送成功，325/325 帧全部应答。
日志：`C:/Users/64264/AppData/Local/ESTHidSender/logs/upgrade_20260904_005000_065344.log`。

发送后自动等待 90 秒，未检测到 M1.23H 重新枚举。
最后一次设备探测返回 device-not-found。
当前状态是：升级包已发送，但 M1.23H 尚未开机确认，静音诊断和旋律复测尚未执行。

## 开机后复测

用户重新连接 USB 后，设备已枚举并确认：

- firmware=M1.23H。
- protocol=1.26。
- A-D 均为 COAST/power 0。
- `pin_layout=2`，`pin_probe=13`。
- `payload_crc31=1459300450`。
- `dreq_waits=208`。
- 解码采样脚本完成，`result_value=15000856`。
- 旋律播放脚本完成，`result_value=12301`。

脚本层和解码诊断均通过。
重点等待用户听感确认：音符前沿的小杂音是否比 M1.23G 明显降低。
