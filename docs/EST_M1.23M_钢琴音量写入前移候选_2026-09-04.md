# EST M1.23M 钢琴音量写入前移候选

日期：2026-09-04。

## 背景

M1.23L 仍存在第一声钢琴音前的杂音。继续分析后，怀疑杂音可能来自
第一次播放前的音量寄存器写入或模拟输出状态变化。

## 修改范围

- 保留 M1.23K 的旧钢琴播放窗口：约 2/3 MP3 数据 + 2048 字节结尾 flush。
- 保留 M1.23L 的第一次播放前 2048 字节预填充。
- 不再先写静音音量再恢复用户音量。
- 硬复位后的第一次播放改为先写用户音量，再进行静音数据预填充，然后播放 MP3。
- 空闲且解码器已就绪时，`est.audio.set_volume()` 立即尝试写入 VS1003，
  避免把音量变化拖到播放开始瞬间。
- 显式 `est.audio.stop()`、Back、HID STOP、断线、异常和故障路径仍保持硬停止。
- 不修改 Studio、协议、能力位、资源写入策略、电机和传感器逻辑。

## 自动验证

- 音频单测：21 项通过。
- 完整固件单测：316 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过；保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23M。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：293320 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123m_piano_volume_prefill/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`fce24ed64595955cc9a3588c7cbde290a38907201cde601f27292fa722de7c42`。

## 2026-09-04 实机记录

- M1.23M 已发送成功，325/325 帧全部应答。
- 开机后确认 firmware=M1.23M，protocol=1.26，能力包含 `audio-playback`。
- A-D 均为 COAST/power 0。
- `tools/est_hid_sender/examples/m123k_piano_legacy_window_smoke.py`
  运行完成，返回 `result_value=12311`。
- 本轮脚本层验收通过；第一声前杂音是否改善等待用户现场听感确认。
