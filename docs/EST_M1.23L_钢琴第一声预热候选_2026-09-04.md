# EST M1.23L 钢琴第一声预热候选

日期：2026-09-04。

## 背景

M1.23K 复现旧 EST 钢琴播放窗口后，用户确认不再是每个音符都有杂音，
剩余问题集中在第一声钢琴音之前的一次前沿杂音。

## 修改范围

- 保留 M1.23K 的 `Piano/*` 旧播放窗口：约 2/3 MP3 数据 + 2048 字节结尾 flush。
- 新增内部 `AUDIO_PREROLL` 阶段。
- 硬复位后的第一次音频播放前，先把 VS1003 音量写为静音并发送 2048 字节静音预填充。
- 预填充完成后再恢复用户音量，开始发送真正的 MP3 数据。
- 钢琴自然结束并 flush 后，后续音符不再重复预填充。
- 显式 `est.audio.stop()`、Back、HID STOP、断线、异常和故障路径仍保持硬停止。
- 不修改 Studio、协议、能力位、资源写入策略、电机和传感器逻辑。

## 自动验证

- 音频单测：19 项通过。
- 完整固件单测：314 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过；保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23L。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：293256 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123l_piano_start_prefill/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`a601200dfb83b738ff588c5a2998d15ecc3dc8aa23087f1b89b56b91968f16f1`。

## 2026-09-04 实机记录

- M1.23L 已发送成功，325/325 帧全部应答。
- 开机后确认 firmware=M1.23L，protocol=1.26，能力包含 `audio-playback`。
- A-D 均为 COAST/power 0。
- `tools/est_hid_sender/examples/m123k_piano_legacy_window_smoke.py`
  运行完成，返回 `result_value=12311`。
- 本轮脚本层验收通过；第一声前杂音是否改善等待用户现场听感确认。
