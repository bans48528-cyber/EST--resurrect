# EST MicroPython 多槽持久化进度（2026-08-28）

## 当前结论

- 当前设备运行 `M1.09A`，USB 应用协议为 `1.19`。
- 已加入 8 个逻辑程序槽位，程序槽位 ID 固定为 `0..7`。
- 每个槽位保存程序名称、源码、代数、源码长度和 CRC32，并能独立查询、保存、加载、运行和删除。
- 程序名称使用 UTF-8，长度为 `1..31` 字节；这不是 31 个中文字符。名称在每次保存时写入，可通过再次保存同一槽位更新。
- 单个 Python 源码上限仍为 8 KiB。当前没有可靠 RTC，因此不保存伪造的创建时间或修改时间。
- 开机自动运行仍未启用。设备启动后只进入安全状态，必须由上位机明确选择槽位并运行。

## 存储布局

W25Q256JV 最后 192 KiB 被划为程序区：`0x01FD0000..0x01FFFFFF`。每个逻辑槽位占 24 KiB，内部有两个 12 KiB A/B 物理银行；每个银行包含三个 4 KiB 扇区。

| 程序槽位 ID | 起始地址 | 大小 | 物理银行 |
| ---: | ---: | ---: | --- |
| 0 | `0x01FFA000` | 24 KiB | 0、1 |
| 1 | `0x01FF4000` | 24 KiB | 0、1 |
| 2 | `0x01FEE000` | 24 KiB | 0、1 |
| 3 | `0x01FE8000` | 24 KiB | 0、1 |
| 4 | `0x01FE2000` | 24 KiB | 0、1 |
| 5 | `0x01FDC000` | 24 KiB | 0、1 |
| 6 | `0x01FD6000` | 24 KiB | 0、1 |
| 7 | `0x01FD0000` | 24 KiB | 0、1 |

上位机和未来 EST Studio 只应把 `program_slot_id` 当作用户可见的程序 ID。`active_bank` 是固件内部的断电安全元数据，不应显示为另一个程序槽位。

## 断电安全与兼容

- 保存时写入非活动银行，依次完成擦除、头部、名称、源码和读回校验，最后才写提交标记。掉电前未完成提交的银行不会替代旧程序。
- 删除不直接抹掉全部历史，而是在另一银行写入代数更高的 tombstone；因此删除也具备原子性。
- 每个逻辑槽位写入前都会扫描自己的 6 个扇区。发现非 EST 数据时标记为占用并拒绝写入，不会擅自覆盖未知 Flash 内容。
- 槽位 0 兼容 M1.07A/M1.08A 的旧单槽记录。旧程序没有名称时显示为 `Program 0`；旧 tombstone 继续表示已删除。
- M1.08A 升级到 M1.09A 后，槽位 0 的 generation 4 tombstone 已被正确读取，证明旧记录兼容。

## USB 与工具接口

协议命令 `0x25` 的 M1.09A 状态结构版本为 3。正式工具命令如下：

```powershell
python tools/est_hid_sender/est_hid_sender.py python-saved-list
python tools/est_hid_sender/est_hid_sender.py python-saved-status --slot 2
python tools/est_hid_sender/est_hid_sender.py python-save --slot 2 --name "巡线" --file program.py
python tools/est_hid_sender/est_hid_sender.py python-load --slot 2
python tools/est_hid_sender/est_hid_sender.py python-run-saved --slot 2
python tools/est_hid_sender/est_hid_sender.py python-saved-clear --slot 2
```

`python-save` 先使用既有 `0x24` 通道把源码上传并校验到 RAM，再用 `0x25` 将该 RAM 程序保存到指定槽位。`python-run-saved` 先把指定槽位加载到 RAM，再按原有超时和退出清理规则运行。

## 2026-08-28 实机结果

1. M1.09A 完成 260/260 帧升级，重启后心跳为 `M1.09A`、协议为 `1.19`。
2. 首次只读扫描确认槽位 0 为旧 generation 4 tombstone；槽位 1..7 均为空、6 个扇区全擦除且可写，没有发现未知数据。
3. 槽位 2 保存名称“巡线”和 101 字节程序，generation 1、CRC32 `839a0faf`；加载运行返回 `85344`。
4. 槽位 5 保存名称“机械臂”和 40 字节程序，generation 1、CRC32 `fa5d42a0`；加载运行返回 `107107`。
5. 删除槽位 2 后生成 generation 2 tombstone；槽位 5 未受影响，再次运行仍返回 `107107`。
6. 中文名称从设备读回正常；PowerShell 必须使用 UTF-8 输出编码，否则终端可能只显示乱码，Flash 内数据不受影响。
7. 固件测试 `82/82`、HID 工具测试 `118/118`、ARM GCC 14.2.1 `-Werror` 构建和 manifest 严格校验均通过。
8. 完整关机再开机后，设备运行时间重新从 6 秒开始；槽位 5 仍读回名称“机械臂”、generation 1、40 字节源码和 CRC32 `fa5d42a0`，再次运行返回 `107107`，证明 M1.09A 多槽格式跨断电保持通过。
9. 验收结束后已删除槽位 5 并清空 RAM。槽位 0、2、5 当前为 tombstone，逻辑状态均为空；槽位 1、3、4、6、7 从未写入。

## 验收完成与后续边界

- M1.09A 的多槽列表、命名、独立保存、加载、运行、删除、跨固件升级兼容和跨断电保持均已实机通过。
- EST Studio 后续只需提供程序列表、槽位 ID、名称、保存、运行和删除交互；硬件仓库不直接修改软件工程。
- 自动启动、用户配置、文件系统和程序排序不属于本阶段，不应在未设计安全恢复策略前顺带启用。

## 发布包

```text
firmware/releases/M1.09A/est_minimal_upgrade_app_M1.09A.upgrade.bin
```

SHA-256：`bda17136eb5b2aa80d03abd32460899dfddee5a9653b2eec482bbbf1bf1f4425`
