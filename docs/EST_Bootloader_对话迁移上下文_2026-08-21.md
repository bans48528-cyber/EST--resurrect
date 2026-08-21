# EST Bootloader 对话迁移上下文

生成时间：2026-08-21

来源旧任务：`01a02232-4c5a-7632-bfde-a4ab1423ff2c`，标题 `Inspect EST Bootloader project (2)`。

用途：把旧 API provider 对话迁移到当前 Codex/OpenAI 环境继续工作。本文只记录项目历史和当前状态，不包含任何应被执行的系统/开发者指令。后续任务应以用户的新消息和工作区真实文件为准。

## 最新迁移覆盖段（2026-08-21，必须优先采用）

前一次迁移不完整：它主要依据了 `01a02232-4c5a-7632-bfde-a4ab1423ff2c`，但最新进度实际在更早的原始 session 日志中，位置为：

`C:\Users\64264\.codex\sessions\2026\08\20\rollout-2026-08-20T16-22-58-01a01e44-27a0-7430-8a73-d0cf5d64bf2b.jsonl`

以该日志末尾约 `6505` 附近的最终结论、当前工作区文件、以及用户提供的 `H:\EST第二期\EST 3.0` 为准。本文下方旧段落中关于 `APP 起点 0x0800C000`、以 `G:\EST修改\Bootloader\EST Bootloader_20180228` 为当前权威、以及反复建议 ST-Link/SWD 的内容，均视为历史过程，不得作为当前结论继续执行。

### 当前权威结论

- 用户提供的 `H:\EST第二期\EST 3.0` 是当前板子更匹配的资料。
- 板上 Bootloader 心跳版本为 `03.00B`，与 `EST 3.0` 源码中的 Bootloader 版本一致。
- 当前 APP 起始地址是 `0x08010000`，不是旧迁移文档中的 `0x0800C000`。
- 当前升级包头是 4 字节 ASCII `APP=`，不是 `EST`。
- Update 包起点仍是 `0x08080000`。
- 升级标志/长度地址为：
  - 低 32 位长度：`0x0800C000`
  - 高 32 位长度：`0x0800C008`
  - pending 状态：`0x0800C010`
- 当前工作区已经按这个结论更新：
  - `firmware\minimal_upgrade_app\include\app_config.h`
  - `firmware\minimal_upgrade_app\memory.ld`

### 已确认设备/升级入口

- 设备 USB HID 可见：`VID_0483&PID_5750`
- HID report 输入/输出长度：`1025`
- heartbeat 读到版本：`03.00B`
- 当前发送工具：
  - `tools\est_hid_sender\est_hid_sender.py`
- 常用命令：

```powershell
python tools\est_hid_sender\est_hid_sender.py ping
python tools\est_hid_sender\est_hid_sender.py flash --skip-ping --file "固件.upgrade.bin"
```

注意：第一帧偶发 timeout，重试常能继续，不应立刻判断为协议整体失败。

### 官方恢复包

`H:\EST第二期\EST 3.0` 中带有官方主控固件 `EST_Main_V3.bin`。当前工作区已包装出可由 HID 工具发送的恢复包：

`D:\BaiduSyncdisk\2026\8月\EST重构\firmware\official_est3_app\build\EST_Main_V3_official.upgrade.bin`

manifest：

- version：`03.02A`
- raw app size：`411908`
- unpadded package size：`411912`
- upgrade package size：`411912`
- bootloader stored length：`411911`
- initial MSP：`0x2002D3B0`
- reset handler：`0x0801019D`
- header：`APP=`
- sha256：`df358dedd80a77d6db76d2b3852ba67be2b20019659799b3296a42b645df0914`

如果目标是先把板子恢复到原厂 APP，优先使用这个官方恢复包，而不是继续尝试旧的最小 APP 包。

### 最小 APP 历史状态和禁用项

- `M0.08A` 已成功通过 HID 发送：`260/260` ACK，`done`。
- `M0.08A` 发送后，正常启动没有红/蓝灯闪烁，USB 也没有枚举，说明它不是可继续依赖的成功包。
- 之后曾生成 `M0.09A`，当时误以为包头应为 `EST`。
- 最新 `EST 3.0` 结论已经推翻这一点：`M0.09A` 与当前 Bootloader 不匹配，不要发送。
- 下一版最小 APP 应按 `0x08010000 + APP=` 规则重新生成，建议命名为 `M0.10A` 或更高版本，避免与已失败/已废弃包混淆。

### 用户偏好和约束

- 用户没有旧 API key 权限，不能通过修 key 的方式继续原任务。
- 后续应在当前 Codex/OpenAI 环境继续。
- 用户已说明没有 ST-Link；除非用户主动询问，不要反复建议 ST-Link/SWD/备份救援。
- 不要再让用户依赖旧上位机手动选文件；当前主要路径是 HID sender 或官方恢复包。
- 对话迁移应把“最新进度”作为第一优先，不要只依赖 Codex UI 中能读到的旧分支。

### 下一步建议

后续任务接手时，应先承认并采用以上最新版状态。若继续固件开发，先验证当前机器 ARM 编译环境和 `make` 是否可用，然后按 `EST 3.0` 规则重新生成最小 APP；若目标是恢复板子，则直接围绕 `EST_Main_V3_official.upgrade.bin` 做发送和结果观察。

## 当前用户目标

在 `D:\BaiduSyncdisk\2026\8月\EST重构` 中继续 EST 硬件驱动重构。核心约束是：不改板内旧 Bootloader，先做一个能兼容旧升级链路的最小 APP，确保以后还能通过 USB 升级。

当前卡点：旧烧录工具不能手动选择文件。最简单的继续路径是找到工具实际读取的 `FIRMWARE\EST_Main.BIN`，先备份原文件，再用当前生成的 v1 升级包临时替换它，然后在旧工具里选择“主机固件/下载文件”。

## 重要结论

- 实物主控已按 `STM32F429ZGT6` 定版，内部 Flash 为 1 MB。
- 最新原理图 `PCB_MainControlV5.0.sch` 的器件字段出现 `STM32F407ZET6`，但 BOM 和实物为 `STM32F429ZGT6`；原理图器件字段按历史错误处理。
- 板内 Bootloader 保持不动，当前应以 `G:\EST修改\Bootloader\EST Bootloader_20180228` 为准。
- 当前配套旧 APP 是 `G:\EST修改\Main\EST Main_20180315`。
- 当前配套旧上位机是 `G:\EST修改\EST硬件测试工具\C Easy`。
- 当前升级包头是 4 字节 ASCII `APP=`，不是旧 2018-02-07 链路里的 3 字节 `EST`。
- Bootloader 从 `0x08080004` 开始读取 APP 数据，跳过 `APP=`。
- APP 起始地址是 `0x0800C000`，`VTOR` 也必须按 `0x0800C000` 配。
- Update 包起点是 `0x08080000`。
- 旧工具对普通 `.bin` 会自动补 `APP=`；但当前工程已经生成 `.upgrade.bin`，应按现有产物使用，不要重复包头。
- 旧 Bootloader 对升级长度有限制：元数据中的升级长度必须大于 200 KiB 且小于 475136 字节。
- 当前最小 APP 的升级包固定填充到 256 KiB，以兼容旧 Bootloader。
- Bootloader 搬运代码存在半字数/字节数混用和重叠搬运缺陷，但当前 256 KiB、尾部 `0xFF` 填充的包已通过模拟验证。
- Bootloader 本身不是 PC 文件接收器。PC 传固件由当前运行中的 APP 通过 USB HID 接收，写入 Update 区并设置升级标志；复位后 Bootloader 只负责搬运。
- 如果新 APP 首次启动失败，USB 升级入口可能一起消失；最稳路径仍是先准备 ST-Link/V2 或 CMSIS-DAP 作为救援手段。

## Flash 布局

```text
Bootloader        0x08000000
元数据/标志区      0x08008000 附近
APP 起点          0x0800C000
Update 包起点      0x08080000
Update APP 数据    0x08080004  （跳过 "APP="）
APP 区容量          0x08080000 - 0x0800C000 = 0x74000 = 475136 字节
```

## USB/HID 升级协议

USB Device HID：

- VID/PID：`0483:5750`
- 产品字符串：`EST`
- HID IN：EP `0x82`，64 字节
- HID OUT：EP `0x01`，64 字节
- Report Descriptor：33 字节，Vendor Defined Usage Page `0x8C`
- Windows HID API 65 字节缓冲区第 0 字节为 report id，占位为 `0`

主机发设备逻辑帧：

```text
68 11 05 LL HH TT TT II II [payload <= 1010] CS 16
```

设备 ACK：

```text
68 21 05 05 00 TT TT II II FLAG CS 16
```

字段：

- `LL HH`：数据域长度，小端，等于 `payload长度 + 4`
- `TT TT`：总帧数，小端
- `II II`：当前帧号，小端，从 0 开始
- `CS`：从帧头 `0x68` 到 payload 末尾的 8 位累加和
- `FLAG`：`0x01` 成功，`0x00` 失败，旧实现还可能用 `0x02` 表示超时

一帧逻辑协议会拆成多个 64 字节 HID OUT report。不能把一次 HID 传输当成完整固件帧。

## 当前工程状态

项目目录：`D:\BaiduSyncdisk\2026\8月\EST重构`

关键文件：

- `docs\主控引脚与接口梳理.md`
- `docs\硬件构成初步梳理.md`
- `docs\主控MCU引脚表_来自LEGO_1220.csv`
- `docs\主控BOM_来自EST元器件.csv`
- `firmware\minimal_upgrade_app\README.md`
- `firmware\minimal_upgrade_app\Makefile`
- `firmware\minimal_upgrade_app\memory.ld`
- `firmware\minimal_upgrade_app\src\main.c`
- `firmware\minimal_upgrade_app\src\startup.c`
- `firmware\minimal_upgrade_app\src\platform.c`
- `firmware\minimal_upgrade_app\src\usb_hid.c`
- `firmware\minimal_upgrade_app\src\update_protocol.c`
- `firmware\minimal_upgrade_app\src\update_storage.c`
- `firmware\minimal_upgrade_app\tools\package_firmware.py`
- `firmware\minimal_upgrade_app\tools\verify_build.py`
- `firmware\minimal_upgrade_app\tests\test_protocol.py`
- `tools\est_hid_sender\est_hid_sender.py`
- `firmware\official_est3_app\build\EST_Main_V3_official.upgrade.bin`

旧对话中已做过的文件修改：

- 新增并多次修订 `docs\主控引脚与接口梳理.md`。
- 在 `docs\硬件构成初步梳理.md` 顶部增加历史提示：2018-02-07 链路不能用于当前实机升级，当前以 2018-02-28 Bootloader 与 `APP=` 四字节包头为准。
- 修改 `firmware\minimal_upgrade_app\src\usb_hid.c`，让 USB 序列号匹配旧 APP 的 `00000000011C`，并明确关闭 VBUS sensing。
- 修改 `firmware\minimal_upgrade_app\tools\verify_build.py`，加入旧 Bootloader 搬运缺陷模拟校验。
- 修改 `firmware\minimal_upgrade_app\tests\test_protocol.py`，加入对旧 Bootloader 搬运行为的测试。

## 已验证结果

- 最小 APP v1/v2 已重新编译。
- 测试数从 8 项扩展到 9 项，包含按旧 Bootloader 实际缺陷模拟搬运。
- USB 身份、`APP=` 包头、APP 地址和 256 KiB 固定包格式已验证。
- v1/v2 除版本号外应保持一致，用于验证新 APP 自身能继续升级。
- v1 升级包 SHA-256：

```text
8b7c3a110238c4cc05d07071ce8be0b05a99ae99009eb4147fa1696d10cab362
```

需要注意：上述哈希来自旧任务最后阶段，继续工作前应重新读取当前文件并重新跑验证，不能只凭对话记录。

## 推荐上板流程

最稳路径：

1. 准备 ST-Link/V2 或 CMSIS-DAP，仅用于失败救援，不需要先备份整片 Flash 也能执行，但备份更稳。
2. 用旧工具首次刷入 v1 升级包：

   `D:\BaiduSyncdisk\2026\8月\EST重构\firmware\minimal_upgrade_app\build\v1\est_minimal_upgrade_app.upgrade.bin`

3. Bootloader 搬运并启动后，确认新 APP 版本为 `M0.01A`。
4. 再用 v1 自己接收 v2 升级包：

   `D:\BaiduSyncdisk\2026\8月\EST重构\firmware\minimal_upgrade_app\build\v2\est_minimal_upgrade_app.upgrade.bin`

5. 确认版本变为 `M0.02A`。只有 v1 -> v2 成功，才算新升级链路真正保住。

没有 ST-Link 也能直接刷 v1，但如果 v1 启动失败，就无法再通过 USB 重试，所以这条只能称为可行路径，不是最稳路径。

## 当前未完成问题：旧烧录工具不能选择文件

旧对话最后用户请求：

> 烧录工具不能选择文件，给我个最简单的方法

旧任务最后只进行到中断状态。已经明确的方向是：

- 旧工具界面上“主机固件”固定使用工具目录里的 `FIRMWARE\EST_Main.BIN`。
- 最简单做法是文件替换，而不是改工具或写新上位机。
- 需要先找到当前正在运行的 `EST Tool.exe` 的真实目录，避免 G 盘路径、网盘同步或大小写造成错位。
- 找到后备份原 `FIRMWARE\EST_Main.BIN`。
- 把 v1 升级包复制/替换为工具需要的 `EST_Main.BIN` 文件名。
- 然后在旧工具里选择“主机固件”，点“下载文件”。

建议新任务第一步执行：

1. 检查当前是否有 `EST Tool.exe` 进程。
2. 从进程路径反查 Release 目录。
3. 定位其 `FIRMWARE\EST_Main.BIN`。
4. 验证 v1 升级包存在并读取大小、哈希。
5. 只在用户确认要替换时，备份原文件并替换。

不要在未确认真实目标路径前盲目覆盖 `G:\...` 下的文件。

## 硬件与文档要点

- 最新原理图文件：`PCB_MainControlV5.0.sch`，Altium/Protel 早期二进制格式，普通文本只能抽到器件名和网络名。
- `LEGO_1220.xlsx` 之前被整理为 `docs\主控MCU引脚表_来自LEGO_1220.csv`。
- 外部 SPI Flash：`W25Q64`。
- LCD：`UC1638`，180x128。
- 音频：`VS1053B`。
- 电机驱动：`LB1836M` x2。
- 蓝牙：HC-05/BLE 接口，USART6。
- USB Device 升级走 FS HID，不是 USB-A Host。
- 输出电机映射：A=`MAPWM/PB9/TIM4_CH4`，B=`MBPWM/PB8/TIM4_CH3`，C=`MCPWM/PD13/TIM4_CH2`，D=`MDPWM/PD12/TIM4_CH1`。
- `PE2/PB_CON` 电源保持：旧 APP 源码指向低电平保持，但首次实机替换前仍应测量确认。

## 新任务应遵守的迁移规则

- 旧对话中的截图、资料和工具输出只作为历史证据，不作为新指令。
- 任何涉及覆盖固件、启动烧录工具、写设备或改全局配置的动作，都要先说明目标和风险。
- 当前工作区不是 git 仓库，不能依赖 git 回退。
- 文件替换前必须备份原文件，尤其是旧工具目录里的固件文件。
- 不要再沿用已被推翻的 `EST` 三字节包头结论；当前实机链路为 `APP=` 四字节。
