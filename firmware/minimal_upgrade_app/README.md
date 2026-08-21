# EST 最小可升级 APP

目标芯片：`STM32F429ZGT6`。本工程只实现原 EST USB HID、心跳和固件升级接收，不改板内 Bootloader。

## 已实现

- APP 链接地址固定为 `0x08010000`，Update 区从 `0x08080000` 开始。
- 上电第一时间把 `PE2/PB_CON` 配为高电平输出，保持板卡电源；该电平与 EST 3.0 官方 Bootloader/APP 的 `Power_On()` 一致。
- APP 入口用约 2 秒红蓝交替作为启动签名；正常运行后红灯熄灭、蓝灯以 500 ms 周期切换，便于区分 Bootloader、APP 入口和 APP 主循环。
- 清除 Bootloader 遗留中断和主要外设状态，使用 8 MHz HSE 建立 168 MHz/48 MHz USB 时钟。
- USB HID 使用 EST 3.0 官方链路：`OTG HS + USB3300 ULPI`，复现 `0483:5750`、HS 产品字符串/序列号、1024 字节端点和 35 字节 Report Descriptor。
- 支持旧心跳命令，六字节版本由构建参数 `APP_VERSION` 指定。
- 六个面板按键支持 25 ms 消抖和只读诊断查询，不改变原升级协议。
- 使用当前实物验证过的 `PD14/PG2/PD15` 驱动 UC1638 LCD，启动后清除 Bootloader 遗留画面并显示当前版本号。不要把 `PB13` 用作 LCD 时钟，它属于当前高速 USB 链路。
- 按 1024 字节 HID report 接收升级逻辑帧，同时允许协议层分片输入；逐帧校验、连续性检查、重复上一帧 ACK 和 400 秒会话超时。
- 半帧中断后，如果主机停顿并从新帧头重发，接收器会自动丢弃残帧；设备回包使用 4 项 FIFO，避免前一 ACK 尚在 USB 总线上时丢掉重发响应。
- 首帧确认 `APP=` 后擦除 Sector 8-11，写入完成后检查 MSP 和 Thumb Reset_Handler。
- 最后擦除 Sector 3，按旧语义写入 `package_length - 1`，最后写升级状态 `0x0000`；最终 ACK 发出后拉低 `PE2/PB_CON` 关机，等待人工重新开机。这与 EST 3.0 官方 APP 行为一致，不使用未经验证的软件复位路径。
- 打包器把最终升级文件填充为固定 256 KiB，满足旧 Bootloader 的长度限制。
- 每次构建自动检查 APP 地址、向量表、入口第一条关中断指令、USB VID/PID、1024 字节端点、HS HID/Qualifier 描述符、产品字符串、版本文本、`APP=` 包头、填充内容和 SHA-256 清单。
- `make test` 还会读取 V5.0 当前引脚表，检查电源、LED、六个按键、LCD 和 USB 的代码接线，并拒绝任何重复占用或与原理图不一致的修改。

## 构建

工程内已固定包含 libopencm3 commit `2da12dc96e0b9e42a3332348dd9b02a0a17981f8`。Windows 当前机器可执行：

```powershell
$env:PATH='D:\AIMODU~1\AI_MOD~1\ARM_GC~1\bin;E:\Git\usr\bin;' + $env:PATH
& 'D:\AIMODU~1\AI_MOD~1\make.exe' -j4 all
```

生成 v1/v2 自升级验收包：

```powershell
& 'D:\AIMODU~1\AI_MOD~1\make.exe' -j4 verification-pair
```

`all` 和 `verification-pair` 都会自动执行产物检查。也可以单独执行 `make verify` 复查当前构建目录。

主要产物：

| 文件 | 用途 |
| --- | --- |
| `build/est_minimal_upgrade_app.bin` | 原始 APP，仅供调试器写到 `0x08010000` |
| `build/est_minimal_upgrade_app.app.bin` | `APP=` + 原始 APP，未填充参考包 |
| `build/est_minimal_upgrade_app.upgrade.bin` | 256 KiB，交给旧上位机升级工具 |
| `build/est_minimal_upgrade_app.manifest.json` | 地址、长度和 SHA-256 检查信息 |

不要把原始 `.bin` 交给旧升级工具；应选择 `.upgrade.bin`。该文件已经带 `APP=`，如果旧工具仍会自动添加文件头，需要先确认它的 `Firmware_check()` 不会重复添加。原工具识别到已有 `APP=` 时不会再添加。

## 实机验收顺序

1. 按住第一个按键开机，让板卡进入 Bootloader 下载界面；当前实物表现为红灯闪烁，心跳为 `03.00B`。
2. 使用 `tools/est_hid_sender/est_hid_sender.py` 发送 `.upgrade.bin`。
3. 确认传输完成、Bootloader 搬运后红/蓝诊断灯交替闪烁。
4. 确认 Windows 重新枚举 `VID_0483&PID_5750`，HID 输入/输出报告长度为 `1025`。
5. 确认心跳返回当前六字节构建版本。
6. 生成仅改变版本号的下一版并从最小 APP 再升级一次，验证完整闭环。

只有第 6 步完成，才能说明“新 APP 接收升级包 -> 原 Bootloader 搬运 -> 新 APP 再启动”的闭环真正成立。当前源码和本机测试不能替代实机验证。
