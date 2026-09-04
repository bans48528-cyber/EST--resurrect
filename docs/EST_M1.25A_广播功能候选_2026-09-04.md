# EST M1.25A 广播功能候选

## 目标

在现有单 MicroPython VM 和协作调度器中实现本地广播，不使用蓝牙、网络、操作系统线程或 uasyncio。

支持三类积木：

- 当接收到消息
- 广播消息
- 广播消息并等待

消息范围固定为 `message_1` 至 `message_8`，对应 Studio 中的消息1至消息8。

## 运行语义

- `rt.on_broadcast(message)` 注册接收程序堆。
- `rt.broadcast(message, wait=False)` 触发匹配的接收程序堆后立即继续。
- `await rt.broadcast(message, wait=True)` 等待本次匹配的接收程序堆完成。
- 接收帽不占活动任务槽，触发后才占用任务槽。
- 最多 16 个事件帽、8 个同时活动的程序堆。
- 同一个接收帽不可重入；运行期间重复触发最多保留一次待执行请求。
- 程序堆结束仍按 owner/generation 释放自己拥有的电机，不影响后来接管电机的程序堆。
- Back、HID STOP、断线和未处理异常继续执行全局安全停止。

Pybricks 没有 EV3 Classroom 风格的本地广播帽。本实现只参考其单 VM `async/await` 协作调度思路，广播本身是 EST runtime 内部事件。

## 版本与能力

- 固件版本：M1.25A
- 协议：1.28
- 能力：`runtime-broadcast`
- 能力位：bit 28
- Studio 最低固件门控：M1.25A

## 自动验证

- frozen runtime 和固件测试：337 项通过。
- HID 工具测试：138 项通过。
- Studio 协议、积木和代码生成测试通过。
- 133 个生成程序通过 Python AST 语法验证。
- ARM GCC `-Werror` 构建通过。

广播专项覆盖：消息1至消息8、无效消息、普通广播、广播并等待、同步及异步接收程序堆、防重入、单次 pending、任务槽复用和同一接收帽自广播不死锁。

## 构建产物

- 原始 APP：299680 字节。
- 未填充升级数据：299684 字节。
- 固定升级包：327680 字节。
- 剩余 APP 空间：27996 字节。
- 相比 M1.24O 原始 APP 增加：936 字节。
- 升级包：`firmware/minimal_upgrade_app/build/m125a_runtime_broadcast/est_minimal_upgrade_app.upgrade.bin`
- SHA-256：`607806953dea2a7b62513ab795ef0df0d3565e558f0a2461a45df7855186bbc7`

## 真机冒烟程序

`tools/est_hid_sender/examples/m125a_broadcast_smoke.py`

预期屏幕依次出现：

1. `BROADCAST TEST`
2. `MSG1 START`
3. 一秒后 `MSG1 END`
4. 之后才出现 `WAIT COMPLETE`
5. 最后出现 `MSG8 RECEIVED`

其中第 3 项先于第 4 项，验证“广播并等待”；第 5 项验证消息8和普通广播。

## 真机验收结果

2026-09-04 在 M1.24O 设备上升级 M1.25A 后确认：

- 设备状态为 M1.25A、协议 1.28，并上报 `runtime-broadcast`。
- 屏幕依次出现 `MSG1 START`、`MSG1 END`、`WAIT COMPLETE`、`MSG8 RECEIVED`。
- “广播并等待”顺序正确，消息8可正常触发。
- 运行期间没有 E3；HID STOP 后恢复默认界面。
- A-D 电机均安全回到 COAST、功率 0。
