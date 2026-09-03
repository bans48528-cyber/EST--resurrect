# EST M1.23J 钢琴播放回归基线

日期：2026-09-04。

## 目的

用户确认 `est.audio.tone()` 的电子音不是目标音色。本轮停止电子音路线，
将钢琴 MP3 播放链路回到最早确认能听到钢琴音的 M1.23D 基线行为，
作为后续继续优化音符切换杂音的对照版本。

## 回归边界

- 保留 VS1003 引脚自动识别：当前实机为旧接线 layout 2。
- 保留内部 Flash 钢琴资源播放：`Piano/C4` 到 `Piano/C7`。
- `est.audio.play()` 每次播放新 MP3 时重新硬复位 VS1003。
- `est.audio.stop()` 恢复硬停止，不再走温和停止。
- 移除后续实验中的软复位、淡入、淡出、切音温和停止和连续电子音接管逻辑。
- 不修改 Studio、协议、能力位、资源写入策略、电机和传感器逻辑。

## 自动验证

- 音频单测：16 项通过。
- 完整固件单测：311 项通过。
- HID 工具测试：133 项通过。
- 引脚检查：94 项占用，0 冲突。
- ARM `-Werror` 构建通过；保留既有 newlib 空系统调用链接警告。

## 候选包

- 版本：M1.23J。
- 协议：1.26。
- 能力：沿用 `audio-playback` bit 26。
- APP raw size：292928 字节。
- 固定升级包：327680 字节。
- 包：`firmware/minimal_upgrade_app/build/m123j_piano_baseline/est_minimal_upgrade_app.upgrade.bin`。
- SHA-256：`ce6e56fdff69c7c7d050b87e64898ae94a3b3724a51056c5e492bd675dab5982`。

## 实机验收

升级后运行 `tools/est_hid_sender/examples/m123a_internal_piano_melody.py`。
预期仍能听到钢琴旋律；若仍有每个音符开始前的小杂音，以该版本作为后续优化对照。

## 2026-09-04 实机记录

- M1.23J 已发送成功，325/325 帧全部应答。
- 开机后确认 firmware=M1.23J，protocol=1.26，能力包含 `audio-playback`。
- A-D 均为 COAST/power 0。
- 基线冒烟脚本 `tools/est_hid_sender/examples/m123j_piano_baseline_smoke.py`
  运行完成，返回 `result_value=12304`。
- 旧 `m123a_internal_piano_melody.py` 带有中途诊断断言，在硬复位基线下触发
  “播放状态提前不是 PLAYING”的自检异常，后续不再用它作为基线听感脚本。
