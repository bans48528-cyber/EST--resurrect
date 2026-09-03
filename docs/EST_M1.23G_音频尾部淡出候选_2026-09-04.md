# M1.23G 音频尾部淡出候选

## 背景

M1.23F 播放钢琴旋律时，用户反馈杂音更像出现在音符后面。
这说明剩余问题主要不是音符启动，而是 `est.audio.stop()` 对当前音频尾部的截断。

## 修改范围

本轮只修改下位机声音停止边界，不修改 Studio、协议、能力位、资源表或电机逻辑。

- 新增 `AUDIO_FADE_OUT` 内部阶段。
- `est.audio.stop()` 不再立即写静音和结束填充，而是继续保持解码器工作，按时间将音量分四步淡出到 0。
- 淡出完成后再发送 MP3 结束填充并释放播放任务。
- 若钢琴 MP3 数据已经提前送完，仍按时间淡出音量，不依赖继续发送音频字节。
- 下一音符可在淡出阶段直接接管，不触发硬复位。
- Python 层查询 `est.audio.state()` 时，温和停止阶段表现为已停止。
- Back、HID STOP、断线、异常和系统清理仍保持硬停止。

## 自动验证

- 音频单测：14 项通过。
- 固件单测：309 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过，保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23G。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：293696 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123g_audio_fadeout_candidate/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`cb515b98b5f9ea76d3e98ba17e342a32a89e9224a8ca18a66da5dfca5f9aaa62`。

## 实机记录

M1.23G 已发送成功，325/325 帧全部应答。
日志：`C:/Users/64264/AppData/Local/ESTHidSender/logs/upgrade_20260904_004309_498340.log`。

开机后确认：

- firmware=M1.23G。
- protocol=1.26。
- A-D 均为 COAST/power 0。
- `pin_layout=2`，`pin_probe=13`。
- `payload_crc31=1459300450`。
- 解码采样脚本完成，`result_value=15000856`。
- 旋律播放脚本完成，`result_value=12301`。

## 待用户听感确认

脚本层和解码诊断均通过。
重点确认：音符后面的小杂音是否比 M1.23F 明显降低。
