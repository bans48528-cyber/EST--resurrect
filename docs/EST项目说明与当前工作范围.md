# EST 项目说明与当前工作范围

日期：2026-08-21

本文用于说明当前 EST 主控重构项目的真实状态、工作目标和边界。若本文与早期迁移记录或旧分支对话冲突，应优先采用本文和当前工作区文件；尤其应采用 `docs/EST_Bootloader_对话迁移上下文_2026-08-21.md` 最前面的“最新迁移覆盖段（2026-08-21，必须优先采用）”。

## 1. 项目目标

本项目是在不修改板内 Bootloader 的前提下，重构 EST 主控下位机固件。

当前阶段的第一目标不是完整恢复所有业务功能，而是先保住升级链路：

1. 确认当前板子的 Bootloader、Flash 布局和 USB HID 升级协议。
2. 准备一个能通过当前 Bootloader 正确搬运并启动的 APP。
3. 确保新 APP 启动后仍能通过 USB HID 接收下一版升级包。
4. 在升级闭环可靠后，再逐步重构硬件驱动和业务功能。

换句话说，当前工作的验收标准是“APP 能启动，并且能继续升级”，不是“所有硬件功能一次性完成”。

## 2. 当前权威资料

当前板子应以用户提供的以下资料为准：

```text
H:\EST第二期\EST 3.0
```

已确认：

- 板上 Bootloader 心跳版本为 `03.00B`。
- 该版本与 `EST 3.0` 资料中的 Bootloader 版本匹配。
- 当前升级包头为 4 字节 ASCII `APP=`。
- 当前 APP 起点为 `0x08010000`。
- Update 包起点为 `0x08080000`。

旧迁移文档正文中关于 2018 旧 Bootloader、`0x0800C000` 作为 APP 起点、以及 `EST` 三字节包头的判断，均只作为历史过程保留，不再作为当前执行依据。

## 3. 当前 Flash / 升级布局

当前采用的关键地址：

| 地址 | 用途 |
| --- | --- |
| `0x08000000` | Bootloader 区 |
| `0x08010000` | APP 起始地址 |
| `0x08080000` | Update 包写入起点 |
| `0x08080004` | Bootloader 跳过 `APP=` 后读取 APP 数据的位置 |
| `0x0800C000` | 升级长度低 32 位 |
| `0x0800C008` | 升级长度高 32 位 |
| `0x0800C010` | 升级 pending 状态 |

注意：`0x0800C000` 当前不是 APP 起点，而是升级元数据区域的一部分。

## 4. USB HID 升级入口

当前板子 USB HID 升级入口已确认可见：

| 项 | 当前值 |
| --- | --- |
| VID/PID | `0483:5750` |
| Bootloader 心跳版本 | `03.00B` |
| HID report 输入/输出长度 | `1025` |
| Device 控制器 | `USB OTG HS + USB3300 ULPI` |
| HID report 有效长度 | `1024` 字节；Windows API 另含 1 字节 Report ID 占位 |
| 当前发送工具 | `tools/est_hid_sender/est_hid_sender.py` |

常用命令：

```powershell
python tools\est_hid_sender\est_hid_sender.py ping
python tools\est_hid_sender\est_hid_sender.py verify --file "固件.upgrade.bin"
python tools\est_hid_sender\est_hid_sender.py flash --file "固件.upgrade.bin"
```

发送器对第 0 包使用 15 秒 ACK 超时，对其余包使用 4 秒 ACK 超时；发送前默认校验 manifest 并显示当前/目标版本，每次升级保存日志。

## 5. 当前已有产物

### 官方恢复包

当前工作区已经从 `EST 3.0` 官方 APP 包装出恢复包：

```text
firmware\official_est3_app\build\EST_Main_V3_official.upgrade.bin
```

manifest 信息：

| 字段 | 值 |
| --- | --- |
| version | `03.02A` |
| raw app size | `411908` |
| unpadded package size | `411912` |
| upgrade package size | `411912` |
| bootloader stored length | `411911` |
| initial MSP | `0x2002D3B0` |
| reset handler | `0x0801019D` |
| header | `APP=` |
| sha256 | `df358dedd80a77d6db76d2b3852ba67be2b20019659799b3296a42b645df0914` |

如果当前目标是让板子恢复到官方 APP，应优先围绕这个恢复包操作。

### 最小 APP 工程

当前最小 APP 工程位于：

```text
firmware\minimal_upgrade_app
```

该工程目标是实现最小可升级 APP，内容包括：

- 启动后保持电源控制脚。
- 清理 Bootloader 遗留中断和外设状态。
- 配置 APP 向量表和系统时钟。
- 枚举 USB HID。
- 支持心跳和版本返回。
- 接收升级包并写入 Update 区。
- 写入升级长度和 pending 标志。
- 复位交给 Bootloader 搬运。

当前工程文件已按 `0x08010000` APP 起点更新，关键文件包括：

- `firmware\minimal_upgrade_app\include\app_config.h`
- `firmware\minimal_upgrade_app\memory.ld`
- `firmware\minimal_upgrade_app\tools\package_firmware.py`
- `firmware\minimal_upgrade_app\tools\verify_build.py`

## 6. 已失败或废弃的最小 APP 版本

以下版本不要继续发送：

| 版本 | 状态 |
| --- | --- |
| `M0.08A` | 已通过 HID 完整发送，`260/260` ACK 且 `done`，但启动后无红/蓝灯闪烁，USB 未枚举，视为启动失败 |
| `M0.09A` | 基于错误的 `EST` 包头判断生成，已被 `EST 3.0` 结论推翻，废弃 |
| `M0.10A` | 按 `APP= + 0x08010000` 生成并成功发送，但心跳无响应；复盘确认仍误用了 FS/64 字节 HID，且把 `PE2` 拉低，启动失败 |

当前候选版本为 `M0.11A`：已改为 EST 3.0 官方的 HS ULPI/1024 字节 HID，并按官方 `Power_On()` 将 `PE2` 保持为高电平。本机构建、产物校验和 9 项测试均已通过。实机已完成 `260/260` 发送并返回 `done`，但跳转后的首次及重复心跳均无响应，后续写入出现 timeout。当前需要通过物理 USB 断开重连和 LED 状态区分“APP 未运行”与“HS PHY 未产生完整重新枚举”。

候选包：

```text
firmware\minimal_upgrade_app\build\v11_hs_ulpi\est_minimal_upgrade_app.upgrade.bin
```

SHA-256：`45800f830bf2d00910ed3ed80cd66e8b47de3c35969803dd6008ec265a5c987f`

## 7. 我们当前实际工作内容

当前我们的工作不是改 Bootloader，也不是继续依赖旧上位机手动选文件。

当前工作内容明确为：

1. 整理并统一 EST 3.0 资料、当前板子行为和工作区代码。
2. 维护一个可由当前 Bootloader 接收并搬运的升级包格式。
3. 使用 `tools/est_hid_sender` 作为主要发送工具。
4. 在需要恢复板子时，优先使用官方恢复包。
5. 在继续开发时，基于 `minimal_upgrade_app` 生成新版本最小 APP。
6. 逐步验证：发送成功、Bootloader 搬运成功、APP 启动成功、USB 再枚举成功、下一版升级成功。

后续硬件驱动重构应在升级闭环稳定后进行，顺序建议为：

1. GPIO、电源保持、LED、按键。
2. LCD 基础显示。
3. 外部 Flash `W25Q64`。
4. 音频 `VS1053B`。
5. 蓝牙/串口。
6. 4 路输入端口。
7. 4 路电机输出、测速和电流检测。

## 8. 当前下一步可执行路径

根据目标不同，下一步分两条：

### 路径 A：先恢复官方 APP

目标：让板子回到 `EST 3.0` 官方 APP 状态。

执行对象：

```text
firmware\official_est3_app\build\EST_Main_V3_official.upgrade.bin
```

建议步骤：

1. 先执行 `ping`，确认 Bootloader 仍返回 `03.00B`。
2. 使用 HID sender 发送官方恢复包。
3. 等待发送完成和设备复位。
4. 观察 USB 是否重新枚举，确认 APP 是否恢复。

### 路径 B：继续开发最小 APP

目标：生成新的最小可升级 APP，替代已失败的 `M0.08A` 和废弃的 `M0.09A`。

建议步骤：

1. 检查当前 ARM GCC 和 make 环境是否可用。
2. 确认 `minimal_upgrade_app` 中 APP 起点、包头、升级元数据地址均为 EST 3.0 规则。
3. 使用已生成并验证的 `M0.11A` 升级包。
4. 发送前检查 manifest、包头、向量表和 reset handler。
5. 发送后观察 LED、USB 枚举和心跳。
6. 若 `M0.11A` 能启动，再生成只改变版本号的下一版，验证自升级闭环。

## 9. 明确不做的事

当前阶段不做以下事项：

- 不修改板内 Bootloader。
- 不再把 `0x0800C000` 当作 APP 起点。
- 不再使用 `EST` 三字节包头作为当前升级包格式。
- 不再发送 `M0.09A`。
- 不再发送已确认失败的 `M0.10A`。
- 不把 2018 旧 Bootloader 当作当前权威资料。
- 不优先依赖旧上位机手动选择固件文件。
- 不在升级闭环未恢复前展开大规模业务驱动重构。

## 10. 关键判断

当前项目的关键判断是：

```text
先恢复或重建“可继续升级的 APP”，再重构完整硬件功能。
```

只要新 APP 启动后 USB 不枚举，就说明升级链路没有保住；即使 HID 发送过程显示 ACK 全部成功，也不能算最终成功。真正成功必须同时满足：

1. 固件包发送完成。
2. Bootloader 搬运后 APP 能启动。
3. APP 能重新枚举 USB HID。
4. APP 能响应心跳。
5. APP 能继续接收下一版升级包。
