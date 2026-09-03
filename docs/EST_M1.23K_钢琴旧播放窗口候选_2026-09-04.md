# EST M1.23K 钢琴旧播放窗口候选

日期：2026-09-04。

## 背景

M1.23J 已回到最早可播放钢琴 MP3 的基线。继续对照旧 EST 3.0 固件后发现，
旧项目播放钢琴音时没有发送完整 MP3，而是按 `PIANO_LEN / 3 * 2` 的窗口
发送音频数据，然后发送 2048 字节静音 flush。

本轮只复现这个旧播放窗口，并保持 M1.23J 的整体基线，不恢复 M1.23E～M1.23I
里的淡入、淡出、软复位或电子音方案。

## 修改范围

- `Piano/*` 资源播放时按旧固件约 2/3 长度发送，按 32 字节 chunk 向上对齐。
- 钢琴资源结尾继续发送 2048 字节静音 flush。
- 若 VS1003 已初始化且处于空闲状态，下一首钢琴音可复用当前解码器状态，
  不再做一次新的硬复位。
- 显式 `est.audio.stop()`、系统 STOP、Back、断线、异常和故障路径仍为硬停止。
- 不修改 Studio、协议、能力位、外部 Flash 写入策略、电机和传感器逻辑。

## 自动验证

- 音频单测：18 项通过。
- 完整固件单测：313 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过；保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23K。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：293064 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123k_piano_legacy_window/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`9fa0c6054030fd1bc3873574c70df994a65f1e5fbe650610cacfef62462dbba4`。

## 实机听感脚本

使用 `tools/est_hid_sender/examples/m123k_piano_legacy_window_smoke.py`。

该脚本不在每个音符后主动 `est.audio.stop()`，而是调用
`est.audio.play("Piano/...", wait=True)`，让固件自然发送旧播放窗口、flush 并结束，
用于判断前置杂音是否主要来自每音符硬切换边界。

## 2026-09-04 实机记录

- M1.23K 已发送成功，325/325 帧全部应答。
- 开机后确认 firmware=M1.23K，protocol=1.26，能力包含 `audio-playback`。
- A-D 均为 COAST/power 0。
- `m123k_piano_legacy_window_smoke.py` 运行完成，返回 `result_value=12311`。
- 本轮自动和脚本层验收通过；音质是否改善仍以用户现场听感为准。
