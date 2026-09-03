# M1.23E 音符切换消爆音候选

## 背景

M1.23D 已确认可以播放内置钢琴 MP3，用户能听到正确钢琴音高，但每个音符前有一声较小的电流杂音。
这说明音频引脚识别、Flash 资源读取和 MP3 解码链路已基本成立，剩余问题集中在每个音符开始/结束时的模拟输出边界。

## 修改范围

本轮只修改下位机声音播放边界，不修改 Studio、协议、能力位、资源表或电机逻辑。

- `est.audio.stop()` 改为温和停止：先尽量将 VS1003 音量写为静音，再发送短填充数据，让当前解码流结束，不再每次直接硬复位。
- 系统级 Back、HID STOP、断线、异常和程序清理仍调用硬停止，保持立即复位音频芯片的安全行为。
- 每次播放新资源时先短暂静音，送入一小段有效音频数据后再恢复用户音量，降低启动瞬间噪声。
- 如果解码器已就绪且处于空闲状态，下一次播放可复用当前初始化状态，不重新执行硬复位。

## 自动验证

- 固件单测：308 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过，保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23E。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：293376 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123e_audio_depop_candidate/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`65e69c212f29caffdde0e8c311b06e0c07fc9465dfe62b8e9a101cb4fb8985ee`。

## 实机记录

M1.23E 已发送成功，325/325 帧全部应答。
日志：`C:/Users/64264/AppData/Local/ESTHidSender/logs/upgrade_20260904_002925_504847.log`。

开机后确认：

- firmware=M1.23E。
- protocol=1.26。
- A-D 均为 COAST/power 0。
- `pin_layout=2`，`pin_probe=13`。
- `payload_crc31=1459300450`。
- 静音解码检查通过，`m123c_audio_decode_diagnostic.py` 返回 `result_value=15000860`。
- 旋律播放脚本完成，`m123a_internal_piano_melody.py` 返回 `result_value=12301`。

## 待用户听感确认

本轮脚本和硬件诊断均通过，但音质验收必须以用户听到的结果为准。
重点确认：每个音符前的小电流杂音是否消失，或至少明显减轻。
