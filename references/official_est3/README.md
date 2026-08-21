# 官方 EST 3.0 关键源码证据

本目录从用户提供的 `H:\EST第二期\EST 3.0` 中提取，只保留当前 Bootloader 兼容工作需要的文件。完整官方工程由网盘保存。

## bootloader

- `C_Protocol.c`：`APP=` 检查、固件分包接收、升级区写入和最终 `iap_Func()` 调用。
- `iap.c` / `iap.h`：元数据地址、升级区到 APP 区搬运、`0x5555/0x0000` 状态和 APP 跳转。
- `flash*`：STM32 Flash 读写和扇区定义。
- `main.c`：冷启动时的 `App_Update_Check()`、按键判定和 APP 跳转顺序。
- `timer3.c`：Bootloader 直接接收固件后延迟搬运并跳转的 RAM 标志流程。
- `power*` / `led*`：电源保持和诊断灯引脚依据。

## app

- `IAP/iap.c` 的副本为本目录 `app/iap.c`：官方 APP 写完升级元数据后等待 1 秒并调用 `Power_Off()`；`NVIC_SystemReset()` 被注释。
- `C_Protocol.c`：官方 APP 的固件下载处理入口。
- `main.c`：官方 APP 初始化顺序。
- `power*`：`PE2/PB_CON` 电源保持语义。
- `uc1638c*`：LCD 控制器代码；其改板引脚与当前原理图资料存在冲突，未经复核不得直接移植。

当前权威运行参数仍以仓库 `docs` 和 EST 3.0 Bootloader 源码共同确认的结论为准。

