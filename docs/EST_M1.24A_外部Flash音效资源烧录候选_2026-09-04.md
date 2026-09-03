# EST M1.24A 外部 Flash 音效资源烧录候选

## 背景

M1.23M 保持钢琴 MP3 可播放基线，但第一声音符前的小杂音暂缓到后期处理。本阶段不继续调音质，先补齐外部 Flash 音效资源写入链路，让旧项目里的 MP3 音效可以写入设备内部外部 Flash，再通过 `est.audio.play("资源名")` 播放。

## 固件改动

- 新增 HID 命令 `0x27`：外部 Flash 音效资源状态、开始写入、分片写入、提交、清除。
- 协议升级到 `1.27`，新增能力 `audio-resource-flash`，bit 27。
- 新增资源区：
  - 起点：`0x01F81000`
  - 大小：`0x0004C000`
  - 槽位：19 个，每个 16 KiB
  - 单个资源最大数据：16256 字节
  - 结束地址：`0x01FCD000`，保留 12 KiB 间隙后才到程序槽位 `0x01FD0000`
- 写入流程先擦除目标槽、写入数据并校验，再最后写入 `DONE` 提交标记，避免断电后半成品被当作有效资源。
- `board_audio` 保持内置 `Piano/*` 优先；找不到内置资源时再按名称查外部 Flash 资源。

## 工具改动

`tools/est_hid_sender` 新增：

- `audio-resource-list`
- `audio-resource-write <file> --name <name> [--duration-ms N] [--replace]`
- `audio-resource-clear --slot N`

示例：

```powershell
python tools/est_hid_sender/est_hid_sender.py audio-resource-list
python tools/est_hid_sender/est_hid_sender.py audio-resource-write old_hello.mp3 --name Sounds/Hello --replace
python tools/est_hid_sender/est_hid_sender.py audio-resource-clear --slot 0
```

当前项目内已整理出一个可用于首轮验证的样本：

```powershell
python tools/est_hid_sender/est_hid_sender.py audio-resource-write firmware/minimal_upgrade_app/assets/audio/Communication/Hello.mp3 --name Communication/Hello --replace
```

## Studio 改动

- Studio 增加对 `M1.24A` / 协议 `1.27` 的识别。
- Studio 增加能力名 `audio-resource-flash` 的显示。
- 本阶段不把资源烧录做进积木界面，音效资源写入仍使用 HID 工具。

## 验证

- `tools/est_hid_sender` 单测：137 项通过。
- 固件单测：320 项通过。
- Studio EST 测试：通过。
- APP C 层对象已使用 ARM `-Werror` 编译通过。
- 当前 Windows 环境缺少 `sh.exe`，完整 `make all` 停在 MicroPython 子构建入口；本阶段未修改 MicroPython，本次使用 M1.23M 已构建的 `libest_micropython.a` 完成手工链接、打包和校验。

## 候选包

- 版本：`M1.24A`
- 协议：`1.27`
- 能力：`audio-resource-flash` bit 27
- APP 原始大小：295784 字节
- 固定升级包大小：327680 字节
- 剩余 APP 包空间：31892 字节
- 升级包：
  `firmware/minimal_upgrade_app/build/m124a_audio_resource_flash/est_minimal_upgrade_app.upgrade.bin`
- SHA-256：
  `fcfe9e5ff9ab7facd46ef6fe64e3e55e93eb66f998e81557a5b70c5de5f2bc0d`

## 后续验收建议

1. 升级 M1.24A，确认 device-status 显示协议 `1.27` 和能力 `audio-resource-flash`。
2. 先执行 `audio-resource-list`，确认设备返回 19 个槽位。
3. 选择一个小于 16256 字节的旧 MP3，写入为稳定名称，例如 `Sounds/Hello`。
4. 用 Python 程序执行：

```python
import est_runtime as rt
import est

@rt.on_start
def stack_1():
    est.audio.play("Sounds/Hello", wait=True)
    rt.sleep(1)

rt.run()
```

5. 验收能播放后，再规划批量转换/压缩旧资源，必要时再扩展 Studio 的资源管理入口。
