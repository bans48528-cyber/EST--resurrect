# EST 3.0 重构与 Bootloader 兼容工程

本仓库保存 EST 3.0 主控重构中可复现、可审计的关键文件。大体量历史 GUI、图片和完整旧工程通过网盘同步，不进入 Git。

## 当前状态

- 当前实机版本：`M0.19A`。
- 已验证完整闭环：`M0.18A` 接收 260 包升级文件，设备冷启动后由官方 Bootloader 搬运，`M0.19A` 成功启动并返回心跳。
- `M0.19A` 启动表现：屏幕显示更新 `0-100`，随后约 2 秒红蓝交替，最后红灯熄灭、蓝灯慢闪。
- `M0.18A -> M0.19A` 已验证 APP 自升级的下一跳能力，保留官方“最终 ACK 后拉低 PE2 关机，等待人工重新开机”的语义。

## 关键目录

| 路径 | 内容 |
| --- | --- |
| `firmware/minimal_upgrade_app` | STM32F429 最小 APP 源码、构建脚本、验证器和测试 |
| `firmware/releases` | 已实机验证的 M0.16A/M0.17A/M0.18A/M0.19A 升级包和清单 |
| `firmware/official_est3_app` | 官方 EST 3.0 APP 恢复包 |
| `tools/est_hid_sender` | Windows HID 心跳和升级发送器 |
| `references/official_est3` | 与升级、跳转、电源和 LCD 相关的官方源码证据 |
| `docs` | 引脚、协议结论、项目范围、调试进度和迁移说明 |

新电脑恢复步骤见 `docs/新电脑开发环境与迁移清单.md`。开始调试前先读 `docs/EST最小APP调试进度_2026-08-21.md`。
