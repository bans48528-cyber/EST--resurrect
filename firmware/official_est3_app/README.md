# EST 3.0 官方 APP 兼容资料

此目录保存可由 Git 追踪的官方恢复包，以及针对网盘中完整旧工程的可重复修正工具。完整 `EST 3.0` 历史工程体积较大，按仓库规则不直接提交。

## W25Q256 高地址修正

实物确认外部 Flash 为 32 MiB 的 W25Q256JV。设备测试确认可用的方法是：发送 `0xB7` 临时进入 4 字节地址模式，继续使用原来的 `0x03/0x02/0x20` 读、写、擦除指令并发送 4 个地址字节，操作完成后用 `0xE9` 恢复 3 字节模式。

先只检查旧工程是否需要修正：

```powershell
python firmware/official_est3_app/tools/apply_w25q256_fix.py `
  --project-root "EST 3.0/EST Main_20190621/Project"
```

确认输出 `status=needs-apply` 后实际应用：

```powershell
python firmware/official_est3_app/tools/apply_w25q256_fix.py `
  --project-root "EST 3.0/EST Main_20190621/Project" --apply
```

工具同时把 FATFS 的前 28 MiB 修正为 `7168` 个 4 KiB 扇区。工具保留原文件的 UTF-8 或 GB18030 编码，可以重复执行；再次执行应显示 `status=already-applied`。

## 构建环境

官方 `Brick.uvprojx` 记录的原始编译器是 Keil ARMCC `V5.06 update 4 (build 422)`，目标芯片为 `STM32F429ZGTx`。当前电脑没有安装 Keil/ARMCC，因此暂时不能重新链接完整官方 APP；已使用 ARM GCC 对修改后的 `w25qxx.c` 和 `diskio.c` 完成语法编译。

安装兼容的 Keil MDK 后，应打开：

```text
EST 3.0/EST Main_20190621/Project/MDK-ARM/Brick.uvprojx
```

选择目标 `EST V3` 重新构建。生成新 BIN 后，还必须经过升级包封装、A 设备升级和 16 MiB 以上地址的实物读写验收，才能作为正式主机固件发布。
