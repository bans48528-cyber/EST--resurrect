# EST 与 Pybricks 全项目对比及修改清单

日期：2026-08-31

## 1. 文档目的

本文记录 EST 固件、`est_runtime.py`、EST Studio 代码生成器、HID 协议、程序存储、设备界面和测试发布链与 Pybricks 的差异，并给出修改优先级。

Pybricks 是设计参考，不是要求 EST 逐项复制。EST 已确定的产品边界、硬件限制和用户交互优先于接口形式上的一致。

## 2. 当前工作区快照

本次检查以工作区真实文件为准：

- 固件仓库 HEAD：`16085d3 fix: stabilize sensor first reads and mode switching`
- 当前源码协议：`1.26`
- 当前能力位：bit 0 至 bit 25 已使用
- 当前未提交候选包：`M1.22C`
- 候选包原始 APP：278,988 bytes
- 候选升级包：327,680 bytes
- 固定升级包内剩余空间：48,688 bytes
- 候选包 SHA-256：`268b24835a7e1512088306271cf721fe67b0462b02786b531d490dd2ef1eeaec`
- MicroPython heap：48 KiB
- MicroPython stack limit：32 KiB
- 单个 Python 源码上限：8,192 bytes
- 程序槽位：8 个
- 程序名称：最多 31 UTF-8 bytes

`M1.22C` 是当前工作区候选结果，不应在未完成验收和提交前写成正式发布基线。正式基线应始终由 Git 提交、发布 manifest 和验收记录共同确定。

检查时已存在以下未提交成果，本文不修改它们：

- `firmware/minimal_upgrade_app/micropython_port/mpconfigport.h`
- `firmware/minimal_upgrade_app/tests/test_protocol.py`
- `tools/est_hid_sender/examples/micropython_min_max_smoke.py`
- `tools/est_hid_sender/examples/single_reflection_pid_line_follow.py`

## 3. 总体判断

EST 已经具备可用产品骨架：旧 Bootloader 兼容升级、MicroPython 程序闭环、8 槽位持久化、菜单系统、三语言、显示资源、电机闭环、零速、HOLD、堵转检测、基本协作调度和基础事件帽均已落地。

当前最需要补的不是继续增加积木数量，而是收紧以下基础契约：

1. 生成器只负责表达程序，不再注入通用参数 helper。
2. 所有电机命令必须经过同一套 runtime 资源所有权管理。
3. Python 异常、行号和 `print()` 输出必须能传回 EST Studio。
4. Studio 只能展示当前固件真正支持的积木，或在运行前明确阻止。
5. 版本、协议、能力和文档必须建立单一事实来源。
6. 传感器读取应逐步从同步阻塞转为可协作等待。

这六项比继续扩展音频、广播、更多事件帽或高级机器人运动更优先。

## 4. 本次已确认的代码膨胀问题

### 4.1 现象

几个简单电机积木会生成一大段 `_est_speed()` 和 `_est_speed_magnitude()`：

```python
def _est_speed(value):
    value = int(value)
    if value > 100:
        return 100
    if value < -100:
        return -100
    return value
```

根因位于 EST Studio：

- `openblock-desktop/src/renderer/est-blocks/python-generator.js`
- `ensureSpeedHelpers()` 会把限幅函数加入每个使用速度积木的程序。

这不是积木业务本身复杂，而是把动态输入限幅放错了层级。

### 4.2 Pybricks 的做法

Pybricks 普通积木尽量直接映射成对象方法，例如 `motor.run()`、`motor.run_angle()`、`drive_base.straight()`。设备配置、参数语义和运行时错误由 API 层负责。只有多任务、多个启动入口等确实需要调度的结构，才生成额外的 `async def`、`multitask()` 或 `run_task()` 代码。

### 4.3 EST 推荐改法

静态常量可以由 Studio 在编辑期提示或折叠；动态值必须由 `est_runtime.py` 或原生 `est` 模块统一处理：

- Studio 生成：`rt.motor_start_speed('A', speed)`
- runtime 处理：类型转换、`-100..100` 限幅、端口规范化和资源接管
- C 层处理：最终硬件安全范围、状态机和错误码

限幅规则只保留一份，避免生成器、runtime 和 C 层行为不一致。

### 4.4 同时发现的高风险问题

当前 `motor_start_speed` 和 `motor_start_power` 直接生成：

```python
rt.motor(port).run_speed(speed)
rt.motor(port).run_power(power)
```

这会绕过 `est_runtime.py` 的 `_begin_motor_command()`、任务 owner 和 generation 机制。多程序堆争用同一电机时，旧任务结束可能误清理后来接管电机的新任务。

建议增加并统一使用：

```python
rt.motor_start_speed(port, speed)
rt.motor_start_power(port, power)
```

这两个接口内部再调用原生 `est.Motor`，并纳入当前命令所有权记录。

## 5. 分层 API 对比与问题清单

### 5.1 Studio 代码生成器

| 当前问题 | Pybricks 方向 | EST 建议 | 风险 |
| --- | --- | --- | --- |
| 速度积木注入整段限幅 helper | 普通块直接调用对象 API | 把动态限幅移入 runtime | 低 |
| 同类参数在不同积木路径处理不一致 | 同一对象方法统一验证 | 速度、功率、端口、方向均调用统一 runtime 入口 | 中 |
| 部分电机块绕过 runtime owner | 设备对象统一管理控制资源 | 禁止生成器直接调用原生连续电机命令 | 高 |
| 已展示的积木会生成 `_unsupported()` API | 不可用设备或 API 在编辑期警告 | 按设备 capability 隐藏、禁用或阻止下载 | 高 |
| 生成代码存在隐藏全局状态 | 参数通常直接出现在方法调用中 | 保留兼容 setter，但新生成代码优先显式参数 | 中 |
| 同时混用方向字符串和有符号速度 | 设备 setup 定义正方向，命令用有符号速度 | 确立唯一规则并保留旧代码适配层 | 中 |
| 单个方法通过单位字符串分派多种动作 | Pybricks 以明确方法表达意图 | 保留积木体验，runtime 内拆为清晰方法 | 低 |

生成代码应该可读、可复制、可调试。普通四个电机积木不应因为限幅而多出十几行样板代码。

### 5.2 frozen `est_runtime.py`

当前 runtime 已承担协作任务、事件注册、电机命令代次和高层传感器包装，这是正确方向，但职责还没有完全闭合。

需要修改：

- 把所有 Studio 生成的电机连续命令纳入 `_motor_commands`。
- 集中实现端口、速度、功率、方向、停止动作和单位规范化。
- 用一个公开契约定义“自动限幅”与“抛 ValueError”的边界。
- 给同步旧程序保留兼容入口，新生成程序使用协作入口。
- 明确每个 API 是否立即返回、可 `await`、是否接管资源、是否取消旧命令。
- 对尚未实现的 API 不仅运行时抛 `NotImplementedError`，还要让 Studio 在下载前识别。

当前明确未实现但 Studio 已有生成路径的接口包括：

- `broadcast`、`on_broadcast`
- `drive_start_dual_speed`、`drive_dual_speed_for`
- `on_color`、`on_touch`、`on_ultrasonic`
- `on_gyro_angle`、`on_ir_proximity`、`on_ir_beacon_button`
- 多种 `wait_*`
- 红外信标方向、距离、按键查询
- 颜色校准与重置

这些接口有两个正确选择：实现后开放，或根据 capability 从 Studio 隐藏。不能继续让用户下载后才看到通用 `E:3`。

### 5.3 原生 MicroPython `est` 模块

原生模块应负责设备对象和低成本硬件操作，不应承担积木生成器特有的文案或隐藏状态。

建议稳定以下对象模型：

- `est.Motor(port)`：角度、速度、功率、状态、堵转、动作命令
- `est.MotorPair(left, right)`：双电机原子命令和完成状态
- `est.DriveBase(...)`：仅在产品需要几何模型时扩展
- 各类型传感器：构造时绑定端口，方法内部完成模式选择和首帧等待
- `est.display`、`est.led`、`est.audio`、`est.buttons`：参数错误给出明确异常

字符串兼容适合放在 frozen runtime。C 模块优先接受紧凑枚举和数值，除非字符串本身就是公开 Python API 的核心可读性需求。

### 5.4 电机 API

EST 已完成的重要能力：

- 速度 0 与 HOLD 分离
- 1..9 低速允许进入闭环
- 运行中更新目标速度
- 角度、圈数、时间动作
- COAST、BRAKE、HOLD
- 单电机和双电机控制
- 堵转查询
- Back、HID STOP、断线和异常统一安全清理

仍需处理：

| 问题 | 建议 | 优先级 |
| --- | --- | --- |
| `speed` 实际是百分比，容易被理解为 deg/s | API 和积木文案明确为 `speed_percent`，兼容旧名称 | P1 |
| `run_power` 与速度路径的范围处理不统一 | 明确 `-100..100` 契约并统一验证 | P0 |
| 时间动作常见上限 600,000 ms | 评估是否需要分段或扩大，超限必须给明确错误 | P1 |
| 角度动作常见上限 3,600° | 按产品需要提高，不能只在运行时给 E:3 | P1 |
| 隐藏的“先设置速度再启动”状态 | 新 API 允许每条动作显式带速度 | P1 |
| 完成、堵转、故障状态接口分散 | 统一 `done()`、`stalled()`、`state()` 语义 | P1 |
| 加减速和控制容差不可配置 | 先提供合理默认，后续按需要开放高级设置 | P2 |
| `board_motor.c` 同时承担硬件、识别、控制、轨迹和配对 | 分阶段拆为硬件、估算、轨迹、控制器、资源管理 | P2 |

Pybricks 还支持齿轮比、正方向、负载/扭矩估算、目标追踪和丰富控制器参数。EST 不必一次照搬。没有可靠电流检测或模型验证前，不应伪造负载/扭矩精度。

### 5.5 双电机和 DriveBase

用户已明确 EST Studio 最终主要使用直行的圈数、角度和秒数，不要求以毫米为核心的移动距离。因此 Pybricks 的轮径、轴距、毫米直行和几何轨迹不是当前缺陷。

当前应优先保证：

- 只允许一组配对任务的产品规则得到明确提示。
- 配对建立、重新配置和运行中接管有确定状态机。
- 双速度 0/50、50/0、0/0 和运行中变速经过同一 runtime 路径。
- 新命令接管后，旧程序堆不能误停新命令。
- 大型与中型电机混配时明确允许、警告或拒绝，不做静默假设。

未来若机器人几何控制成为明确需求，再增加 Pybricks 风格 `DriveBase` 参数，不应为了形式兼容现在就扩大范围。

### 5.6 传感器 API

当前 typed sensor 已朝 Pybricks 的正确语义靠近：用户直接调用 `reflection()` 或 `distance()`，底层负责端口、类型、模式切换和首帧等待；字符串端口和整数端口也已规范化。

仍有一个架构问题：首次读取或模式切换最多可在 C/Python 调用内同步等待约 3 秒。VM hook 能维持 USB STOP、Back、断线和 watchdog，但同一 MicroPython VM 中其他 Python 协作任务此时无法运行。

建议下一阶段采用非阻塞传感器读取状态机：

- C 层维护 requested mode、active mode、generation、fresh frame。
- runtime 提供可轮询或可 `await` 的读取对象。
- 同步旧 API 在内部驱动相同状态机，保持兼容。
- 事件帽只读取缓存和状态，不在调度循环中进行 3 秒阻塞等待。
- 多任务争用同一传感器不同模式时，明确 owner、切换规则和失效代次。

传感器事件帽、`wait_*` 和红外信标应在这套非阻塞基础上实现，不宜继续逐个增加同步特例。

### 5.7 协作调度与事件

EST 当前使用单个 MicroPython VM、自建协作调度器、最多 8 个活动任务和 16 个事件监听器。这符合当前资源条件，也符合“不使用线程或 uasyncio”的既定方向。

与 Pybricks 相比需要继续收紧：

- 所有可能等待的 runtime 方法都必须主动让出执行权。
- 资源接管必须由 runtime/C 层统一，不能依赖生成器自律。
- 异常应标记具体任务、处理器和源码位置。
- `stop("this_stack")`、`stop_other_stacks()`、`stop("all")` 的清理边界要有动态测试。
- 事件 pending、不可重入和任务槽复用要做长时间压力测试。
- 普通同步程序继续可运行；多个启动帽和事件帽生成 `async def`。

不建议改成操作系统线程。当前问题是 API 入口不统一和阻塞点未完全消除，不是线程数量不足。

### 5.8 Python 诊断与输出

这是当前体验差距最大的部分。

现状：

- `mp_hal_stdout_tx_strn()` 丢弃所有输出。
- Python 异常被捕获后只返回失败，没有传送 traceback。
- HID program status 只提供通用状态和错误码。
- Studio 最终常显示通用失败，设备屏幕显示 `E:3`。

Pybricks Code 会在 output window 显示 `print()` 输出和有帮助的错误信息。EST 应优先补齐：

1. 固件侧有界 stdout/stderr 环形缓冲。
2. 捕获异常类型、消息、源码行号和有限 traceback。
3. HID 增加分页读取日志或诊断记录的命令。
4. Studio 显示用户可理解的错误，并定位到生成代码或积木。
5. 日志溢出时丢弃旧数据或限流，不能占满 RAM。
6. STOP 和断线仍优先于日志传输。

验收标准不应再是“只看到 E:3”，而应至少能看到：异常类型、消息、程序行号和最近几条 `print()`。

### 5.9 程序存储与 Python 能力

当前 8 槽位、A/B bank、CRC 和命名程序机制是 EST 的优势。主要限制是：

- 单源码上限只有 8 KiB，生成器 helper 会进一步挤占空间。
- 文件导入目前只支持 frozen module，没有用户 companion file。
- REPL 被关闭。
- 程序名最多 31 UTF-8 bytes，中文和葡萄牙语的可见字符数会明显少于 31。

Pybricks 支持项目导入文件随主程序一起下载，并提供终端/输出能力。EST 建议顺序：

1. 先去掉生成代码膨胀并测量真实程序规模。
2. 再评估把源码上限提高到 12 或 16 KiB 对 RAM 的影响。
3. 需要模块化时，设计简单的多文件打包或编译后 MPY，不引入外部资源包体系。
4. REPL 仅作为开发功能评估，不作为普通用户必需功能。

### 5.10 显示、菜单、语言和音频

EST 的本机 180x128 菜单、程序槽位、端口页、遥控页、马达页、设置、传输弹窗和三语言是产品差异化能力，不需要模仿 Pybricks 的无屏或小屏交互。

后续需要收口：

- 明确程序名可显示字符集和缺字 fallback，避免空白或方框。
- 区分 UI 字体、用户程序字体和图片资源的容量预算。
- 所有页面使用同一文本测量和裁剪函数。
- 音频接口的当前验收状态需要重新建立权威记录，不能继续依赖旧 README。
- 背光、音量、语言和最近程序已经持久化，应纳入自动掉电恢复测试。

### 5.11 电池与安全

当前电量百分比来自两节锂电池电压曲线并带采样平均。短期按现有曲线继续使用，但应保留以下验证项：

- 空载和 80% 至 100% 电机负载下的电压下陷。
- 低电量时是否需要禁止高功率输出或安全关机。
- 不同电池批次、温度和老化后的百分比误差。
- 电压 ADC 标定与真实万用表读数对照。

这是产品安全和准确度工作，不应混入普通 API 补丁。

### 5.12 HID 协议、版本和能力位

当前 device status 中 capability 是单个 32-bit 字段，已经用到 bit 25，只剩 6 位。继续按每个功能消耗一位，很快会遇到上限。

建议在下一个需要协议升级的阶段增加：

- capability word count 或 capability pages；或
- 向后兼容的 TLV 扩展状态；
- 每项 API 以 capability group 表达，避免每个小功能单独占一位。

版本比较也存在问题。当前正则把版本末尾字母纳入 family，因此 `M1.22B` 与 `M1.22C` 被判为不可比较，往往需要 `--force`。建议把版本拆为：

- 产品 family：`M`
- major：`1`
- minor：`22`
- patch/revision：`B`、`C`
- 可选 build metadata

设备显示字符串可以继续保持 6 字节，但 host manifest 应使用结构化版本字段。

### 5.13 测试、CI 和发布

当前仓库有 18 个固件侧测试文件、3 个 HID 工具测试文件、ARM `-Werror` 构建和发布包校验，但根仓库没有自己的 GitHub Actions。MicroPython 子目录中的 CI 不能替代 EST 项目 CI。

建议建立最小 CI：

- frozen runtime 单测
- 完整固件单测
- HID sender 单测
- EST Studio 生成器与协议测试
- ARM `-Werror` 构建
- 固件尺寸预算检查
- manifest、固定包长和 SHA-256 校验
- 生成代码语法检查
- 文档中的版本/协议/能力与源码一致性检查

中期增加确定性主机仿真：

- 电机速度、位置、HOLD、堵转和接管轨迹
- 传感器模式切换、STALE、拔插和错误恢复
- 8 任务、16 事件的长时间槽位复用
- STOP、断线、异常和重启的所有清理路径

实机测试仍然必要，但不应承担所有回归责任。

### 5.14 文档和基线管理

当前文档明显漂移：根 README 仍描述较早版本，`docs/EST项目说明与当前工作范围.md` 也落后于当前协议和候选固件。

建议新增一个机器可读的单一基线文件，例如：

```json
{
  "firmware": "M1.22C",
  "protocol": "1.26",
  "commit": "...",
  "package_sha256": "...",
  "accepted": false
}
```

README、发布说明、Studio 兼容表和升级工具从该文件生成或校验。只有实机验收完成后才把 `accepted` 置为 true 并建立阶段提交。

## 6. 推荐优先级

### P0：停止继续扩大不稳定 API 面

1. 移除生成器速度 helper，把限幅归入 runtime。
2. 新增统一 `motor_start_speed/power`，修复绕过 owner/generation 的路径。
3. 让 Studio 按 capability 隐藏或阻止所有 `_unsupported` 积木。
4. 增加 Python traceback、行号和有界 `print()` 输出通道。
5. 建立版本、协议、能力、包 SHA 和验收状态的单一事实来源。
6. 修正 host 版本比较，使 B/C 修订可正常排序。

P0 验收：普通积木生成代码简洁；并发资源接管不误停；不支持积木无法下载；Python 错误不再只有 E:3；发布信息不再互相矛盾。

### P1：提高程序规模和协作可靠性

1. 传感器非阻塞状态机和协作读取。
2. 完成事件帽和 `wait_*` 前先解决模式冲突与 pending 语义。
3. 明确速度、功率、方向、停止动作、单位和超限契约。
4. 评估提高 8 KiB 源码限制，并支持简单多文件导入。
5. 增加 EST 项目 CI 和确定性状态机测试。
6. 扩展 capability 表达方式，避免 32-bit 耗尽。

### P2：控制品质和高级能力

1. 拆分 `board_motor.c` 的硬件、估算、轨迹和控制职责。
2. 提供可选加减速、容差和高级 motor state API。
3. 按明确产品需求再增加广播、传感器事件、IR 信标和高级 DriveBase。
4. 完成电池负载标定、低电量策略、音频权威验收和字体 fallback。
5. 评估更新包签名、反回滚和旧 Bootloader 带来的产品化风险。

## 7. 建议版本切分

不要在当前候选未验收时预先占用准确版本号。建议按功能性质切分：

- 当前候选之后的缺陷补丁：生成器限幅归位、电机 owner 路径统一、版本比较修正，不升级协议。
- 下一协议小版本：stdout/traceback 诊断通道和 capability 扩展。
- 后续功能版本：非阻塞传感器与完整事件帽。
- 更长期控制版本：电机控制分层和高级配置。

每个版本先完成自动测试和实机验收，再提交 Git 并更新正式基线。

## 8. 不建议照搬 Pybricks 的内容

- 不为了兼容形式强制加入毫米直行和轮距模型，除非 EST 产品明确需要。
- 不引入操作系统线程或 uasyncio；继续使用单 VM 协作调度。
- 不支持多组 MotorPair 同时配置；当前产品只允许一组。
- 不恢复外部 Flash 资源包、`.estpkg` 或二次资源传输。
- 不在本阶段改变 Bootloader 地址、APP 起点或固定升级链路。
- 不直接复制 Pybricks 或 LEGO GPL 源码；只参考公开语义和控制结构。
- 不为尚未验证的硬件能力提供虚假的扭矩、电流或负载精度。

## 9. 推荐立即执行顺序

1. 先修生成器限幅和连续电机命令入口，这是本次问题的直接根因。
2. 同一阶段增加 capability 门控，停止继续生成已知不支持 API。
3. 随后做 Python 错误和输出通道，让后续问题能被准确定位。
4. 建立机器可读基线和 CI，固定发布纪律。
5. 再做非阻塞传感器和剩余事件帽。
6. 最后按真实用户需求扩展高级 Motor、DriveBase、音频和广播。

## 10. Pybricks 参考资料

- 生成代码与多任务理念：https://pybricks.com/learn/intro/story-mission/
- Pybricks Code、output window 与错误提示：https://pybricks.com/learn/getting-started/pybricks-environment/
- 设备 setup、命名和正方向：https://pybricks.com/learn/making-programs/basic-blocks/
- Motor API：https://docs.pybricks.com/en/latest/nxtdevices/motor.html
- DriveBase API：https://docs.pybricks.com/en/v4.0.0/robotics.html
- Stop 语义：https://docs.pybricks.com/en/stable/parameters/stop.html
- 多文件导入示例：https://pybricks.com/project/spike-hub-menu/
- Pybricks 源码与测试：https://github.com/pybricks/pybricks-micropython

以上资料用于对照 API 责任、资源接管、协作调度、错误诊断和测试方法，不作为直接复制实现的依据。
