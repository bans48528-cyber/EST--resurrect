# M1.23I 连续电子音实验候选

## 背景

用户确认当前钢琴旋律测试中的每个音符都是独立 MP3，且音符前沿仍有杂音。
本轮尝试另一条路径：使用连续 PCM tone 流播放旋律，不再为每个音符重新打开一个 MP3 文件。

## 修改范围

本轮只修改下位机 tone 播放行为和测试脚本，不修改 Studio、协议、能力位、资源表或电机逻辑。

- `est.audio.tone()` 在已有 tone 正在播放时，不再重启解码器。
- 同一条 PCM/WAV 流中直接更新 MIDI note 和持续时间。
- 音高切换保持当前波形相位，不重新发送 WAV 头，尽量避免音符之间的前沿杂音。
- 每个 tone 命令的持续时间从新命令开始重新计时。
- MP3 资源播放逻辑保持 M1.23H 的静音、淡入、淡出和软复位策略。
- 新增连续电子音测试脚本：`tools/est_hid_sender/examples/m123i_continuous_tone_melody.py`。

## 自动验证

- 音频单测：16 项通过。
- 固件单测：311 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过，保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23I。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：293952 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123i_continuous_tone_candidate/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`30550ee87fdc180f2accb249f40959cef825050acd6b290a960c8e6c627aa0d1`。

## 实机状态

M1.23I 已发送成功，325/325 帧全部应答。
日志：`C:/Users/64264/AppData/Local/ESTHidSender/logs/upgrade_20260904_010643_833713.log`。

发送后自动等待 90 秒，未检测到 M1.23I 重新枚举。
最后一次设备探测返回 device-not-found。
当前状态是：升级包已发送，但 M1.23I 尚未开机确认，连续 tone 旋律尚未执行。

## 开机后复测

用户开机后，设备已枚举并确认：

- firmware=M1.23I。
- protocol=1.26。
- A-D 均为 COAST/power 0。
- `audio-playback` 能力仍存在。

连续 tone 旋律脚本完成：

- 脚本：`tools/est_hid_sender/examples/m123i_continuous_tone_melody.py`。
- `result_value=12302`。
- 运行约 8575 ms。

该测试不使用独立 MP3 音符，而是在同一条 PCM tone 流中切换音高。
仍等待用户确认听感：音符前沿杂音是否消失，以及电子音色是否可接受。
