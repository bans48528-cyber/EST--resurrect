# EST 与 Pybricks 对比整改：新对话迁移交接

日期：2026-08-31

## 1. 交接目的

本文件用于把 EST 全项目与 Pybricks 的对比结论迁移到新对话，并明确后续修改顺序、上下位机边界和第一批工作的验收标准。

详细分析见：

- `docs/EST与Pybricks全项目对比及修改清单_2026-08-31.md`

新对话应先读取本文件和详细分析文档，再检查工作区真实状态。旧对话内容只作为历史证据，若与当前文件或源码冲突，以当前工作区为准。

## 2. 当前工作区快照

### 2.1 下位机仓库

路径：

`D:\BaiduSyncdisk\2026\8月\EST重构`

当前状态：

- HEAD：`16085d3 fix: stabilize sensor first reads and mode switching`
- 当前协议：`1.26`
- capability 已使用 bit 0 至 bit 25
- 当前构建候选：`M1.22C`
- 候选原始 APP：278,988 bytes
- 固定升级包：327,680 bytes
- 固定包内剩余：48,688 bytes
- SHA-256：`268b24835a7e1512088306271cf721fe67b0462b02786b531d490dd2ef1eeaec`

`M1.22C` 是未完成正式验收和阶段提交的候选结果，不能直接称为正式发布基线。

检查时已有以下未提交成果，后续禁止 reset、checkout、覆盖或丢弃：

- `firmware/minimal_upgrade_app/micropython_port/mpconfigport.h`
- `firmware/minimal_upgrade_app/tests/test_protocol.py`
- `tools/est_hid_sender/examples/micropython_min_max_smoke.py`
- `tools/est_hid_sender/examples/single_reflection_pid_line_follow.py`
- `docs/EST与Pybricks全项目对比及修改清单_2026-08-31.md`

### 2.2 上位机仓库

路径：

`D:\BaiduSyncdisk\2026\8月\EST STUDIO 开发\openblock-desktop`

当前状态：

- HEAD：`e1036ac Update EST Studio features and localization`
- 当前存在未提交修改：`src/renderer/app.css`

该修改不是本次交接文档工作产生的。后续同样禁止 reset、checkout、覆盖或丢弃，应先读取并判断它与任务是否相关。

上位机和下位机是两个独立仓库。修改跨越 API 或协议时，必须分别测试和提交，不能只修改其中一侧。

## 3. 已确认的直接问题

### 3.1 简单积木生成大段代码

EST Studio 的速度积木会注入：

```python
def _est_speed(value):
    value = int(value)
    if value > 100:
        return 100
    if value < -100:
        return -100
    return value
```

以及 `_est_speed_magnitude()`。

根因是输入限幅被放在代码生成器，而不是 runtime/API 层。普通电机积木因此生成不必要的样板代码，也消耗当前仅 8 KiB 的用户源码空间。

Pybricks 的普通积木主要直接映射到对象 API。只有多任务、多个开始帽等结构需要额外调度代码。

推荐原则：

- Studio：生成简洁、可读的 API 调用；静态常量可在编辑期警告。
- frozen `est_runtime.py`：处理动态类型转换、限幅、端口规范化和兼容语义。
- C 层：执行最终硬件安全检查和状态机控制。

限幅规则只能有一个权威实现，不能在生成器、runtime 和 C 层各写一套不同逻辑。

### 3.2 连续电机命令绕过资源所有权管理

当前 Studio 会生成：

```python
rt.motor(port).run_speed(speed)
rt.motor(port).run_power(power)
```

这两个调用绕过 `est_runtime.py` 的 `_begin_motor_command()`、owner 和 generation 记录。

风险：多程序堆争用同一电机时，旧任务结束或清理可能误停后来接管电机的新任务。

推荐新增统一入口：

```python
rt.motor_start_speed(port, speed)
rt.motor_start_power(port, power)
```

这两个入口应完成参数规范化、资源接管和原生 `est.Motor` 调用。

## 4. 为什么必须分批修改

不建议把所有差距一次完成。涉及的层次包括：

- Studio 积木定义和 Python 生成器
- frozen `est_runtime.py`
- MicroPython 原生 `est` 模块
- 电机与传感器 C 状态机
- HID 协议和 sender
- Studio 输出窗口与设备能力判断
- 程序存储和下载格式
- 自动测试、固件构建和发布基线

同时修改会让回归来源难以定位，也容易出现新 Studio 只能配新固件、旧 Studio 无法继续使用的问题。

正确做法是每批只改变一组明确契约，并保留至少一个版本周期的兼容层。

## 5. 推荐分批路线

### 第一批：生成器与电机 runtime API 收口

目标：解决本次已确认的代码膨胀和电机资源接管问题。

下位机：

1. 在 frozen `est_runtime.py` 增加 `motor_start_speed(port, speed)`。
2. 增加 `motor_start_power(port, power)`。
3. 两个入口都使用 `_begin_motor_command()` 和 generation 所有权。
4. runtime 统一处理端口、整数转换和输入范围。
5. 明确速度、功率是自动限幅还是越界抛异常。
6. 保留旧 `rt.motor(...).run_speed/run_power`，不立即删除。
7. 不改变现有零速、1..9 低速、运行中改速、HOLD 和安全停止语义。

上位机：

1. 删除 `ensureSpeedHelpers()`、`_est_speed()` 和 `_est_speed_magnitude()` 生成逻辑。
2. `motor_start_speed` 改为生成 `rt.motor_start_speed(...)`。
3. `motor_start_power` 改为生成 `rt.motor_start_power(...)`。
4. 其他速度积木直接传递表达式，由 runtime 统一处理。
5. 更新 Python 生成器测试，确认普通积木不再注入 helper。

建议兼容顺序：

1. 先在下位机加入新 API，并运行自动测试和 ARM 构建。
2. 实机验证新 API，同时确认旧生成程序仍可运行。
3. 再修改 EST Studio 生成器。
4. 用新 Studio 分别测试新固件和兼容提示。
5. 两个仓库分别提交阶段 commit。

第一批通常不需要升级 HID 协议。若 Studio 需要识别新 runtime API，优先使用固件版本或新增 runtime API version 字段，不要为每个小方法继续消耗一个 capability bit。

第一批验收：

- 四个简单电机积木不再生成速度 helper。
- 动态输入超过范围时行为一致。
- 速度 0、1、5、9 和正负 100 正常。
- 运行中 20 -> 0 -> 75 不退出程序。
- 两个任务争用同一电机时新任务接管。
- 旧任务结束不误停新任务。
- `stop("this_stack")`、`stop("all")`、Back、HID STOP 和断线清理正确。
- 旧 Studio 生成的程序仍能在兼容期运行。

### 第二批：Studio capability 门控

目标：用户不能下载已知会调用 `_unsupported()` 的积木程序。

当前 Studio 已有生成路径、但 runtime 仍未实现的主要接口：

- `broadcast`、`on_broadcast`
- `drive_start_dual_speed`、`drive_dual_speed_for`
- `on_color`、`on_touch`、`on_ultrasonic`
- `on_gyro_angle`、`on_ir_proximity`、`on_ir_beacon_button`
- 多种传感器 `wait_*`
- IR 信标方向、距离和按键方法
- 颜色校准与重置

下位机：

- 维护准确的版本、协议和能力声明。
- 不为未实现功能上报 capability。

上位机：

- 根据 capability 隐藏、禁用或在下载前阻止。
- 提示具体缺少的固件能力，不能只显示“运行失败”。

### 第三批：Python 输出和异常诊断

目标：结束设备只显示 `E:3`、Studio 只显示通用失败的状态。

下位机：

- 增加有界 stdout/stderr 环形缓冲。
- 捕获异常类型、消息、源码行号和有限 traceback。
- HID 提供分页或分块读取诊断信息的命令。
- 日志不能阻塞 STOP、断线和电机安全清理。

上位机：

- 增加输出窗口。
- 显示 `print()`、异常类型、消息和行号。
- 尽可能把生成代码行映射回积木。

这一批涉及 HID 契约，建议升级协议小版本，并同步更新 sender、Studio 和协议测试。

### 第四批：非阻塞传感器与完整事件协作

目标：首次读取和模式切换不再阻塞其他 Python 程序堆。

下位机：

- 保留 requested mode、active mode、generation 和 fresh frame 状态机。
- 提供可轮询或可 `await` 的读取过程。
- 同步旧 API 驱动同一状态机，保持兼容。
- 处理多任务模式争用、拔插、STALE 和超时。

上位机：

- 传感器等待和事件处理器生成 `async def`。
- 协作调用生成 `await`。
- 只有固件声明支持时开放对应帽子和等待积木。

### 第五批：程序规模、多文件和存储

当前限制：

- 单 Python 源码最大 8,192 bytes。
- 8 个持久化槽位。
- 程序名最多 31 UTF-8 bytes。
- 用户程序不能导入一起下载的 companion file。

建议先去掉生成器膨胀，再测量实际项目大小。确有需要时：

- 下位机评估 12 或 16 KiB 源码缓冲的 RAM 影响。
- 协议支持简单多文件清单或编译后 MPY。
- Studio 负责项目文件打包、依赖扫描、容量预检和一起下载。

不要重新引入外部 Flash 图片资源包、`.estpkg` 或二次资源传输方案。

### 第六批：协议、版本、CI 和正式基线

需要解决：

- 32-bit capability 已使用到 bit 25，只剩 6 位。
- 当前版本比较把末尾字母当作 family，`M1.22B` 与 `M1.22C` 无法正常排序。
- 根 README、项目说明和当前源码版本存在漂移。
- 根仓库没有 EST 自己的 GitHub Actions。

建议：

- 增加 capability pages、多 word 或向后兼容 TLV。
- 把版本拆为 family、major、minor、revision。
- 新增机器可读正式基线文件，记录版本、协议、commit、SHA 和验收状态。
- CI 自动运行固件单测、runtime 单测、HID 单测、Studio 生成器测试、ARM `-Werror` 构建、尺寸预算和 manifest 校验。

## 6. 上下位机修改对应表

| 项目 | 下位机 | 上位机 |
| --- | --- | --- |
| 输入限幅 | runtime 统一转换和范围契约 | 删除生成 helper，直接传表达式 |
| 电机连续命令 | 新 runtime 入口和资源 owner | 生成新入口调用 |
| 不支持功能 | 准确 capability | 隐藏、禁用或阻止下载 |
| Python 错误 | 缓冲并通过 HID 上报 | 输出窗口和错误定位 |
| 传感器协作 | 非阻塞状态机 | `async def` 和 `await` 生成 |
| 多文件程序 | 存储和传输协议 | 项目打包和容量预检 |
| 版本能力 | 结构化协议字段 | 解析、比较和兼容提示 |
| 控制器内部调优 | 主要在 C 层 | 通常无需修改 |
| 电池、字体、菜单 | 主要在固件 | 仅在公开契约变化时修改 |

## 7. 暂时不要照搬 Pybricks 的内容

- 不为了形式兼容强制采用毫米距离和轮径/轴距 DriveBase。
- 不引入操作系统线程或 uasyncio。
- 不支持多组 MotorPair；当前产品只允许一组。
- 不改变旧 Bootloader、APP 起点或固定升级链路。
- 不恢复外部资源包和二次资源上传。
- 不直接复制 Pybricks 或 LEGO GPL 源码，只参考公开语义和架构。
- 没有硬件测量依据时，不提供虚假的扭矩、电流或负载精度。

## 8. 新对话建议先做什么

新对话应只执行“第一批：生成器与电机 runtime API 收口”，不要同时开始诊断协议、非阻塞传感器和多文件系统。

开始前必须：

1. 阅读本文件和详细对比文档。
2. 分别检查两个 Git 仓库状态。
3. 保留下位机全部未提交成果。
4. 阅读 `est_runtime.py` 的 owner/generation 实现。
5. 阅读 Studio `python-generator.js` 的速度 helper 和电机生成路径。
6. 先列出兼容方案和测试矩阵，再改代码。
7. 不自动升级设备，不自动推送 Git，等待用户确认。

## 9. 可直接用于新对话的任务说明

```text
请先读取并理解：

D:\BaiduSyncdisk\2026\8月\EST重构\docs\EST_Pybricks对比整改_新对话迁移交接_2026-08-31.md
D:\BaiduSyncdisk\2026\8月\EST重构\docs\EST与Pybricks全项目对比及修改清单_2026-08-31.md

然后检查两个真实工作区：

下位机：D:\BaiduSyncdisk\2026\8月\EST重构
上位机：D:\BaiduSyncdisk\2026\8月\EST STUDIO 开发\openblock-desktop

当前存在未提交成果，不要 reset、checkout、覆盖或丢弃。先只实施交接文档中的“第一批：生成器与电机 runtime API 收口”：把速度/功率动态限幅移入 frozen est_runtime.py，增加纳入 owner/generation 管理的 motor_start_speed 和 motor_start_power，并让 EST Studio 删除 _est_speed helper、改为生成简洁的新 API 调用。保留旧接口兼容，不改变零速、低速、运行中改速、HOLD 和安全停止语义。

先报告检查结果、修改范围、兼容方案和测试矩阵，再开始改代码。完成后运行下位机 runtime/固件/HID 测试、ARM -Werror 构建，以及上位机生成器和协议测试。不要自动升级设备，不要自动推送 Git。
```

## 10. 参考资料

- Pybricks 生成代码与多任务理念：https://pybricks.com/learn/intro/story-mission/
- Pybricks Code 和 output window：https://pybricks.com/learn/getting-started/pybricks-environment/
- Pybricks Motor API：https://docs.pybricks.com/en/latest/nxtdevices/motor.html
- Pybricks DriveBase API：https://docs.pybricks.com/en/v4.0.0/robotics.html
- Pybricks Stop 语义：https://docs.pybricks.com/en/stable/parameters/stop.html
- Pybricks 多文件导入示例：https://pybricks.com/project/spike-hub-menu/

这些资料用于参考 API 责任、资源接管和代码生成方式，不作为复制源码的依据。
