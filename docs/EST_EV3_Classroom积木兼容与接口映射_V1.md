# EST EV3 Classroom 积木兼容与接口映射 V1

日期：2026-08-26

> 文档状态：117 种积木已确认并完成 MicroPython 映射；另有 11 种安装包源码候选，未计入默认面板。M0.88A 已实机验证首个 EV3 持续转向混控路径。

## 快速导航

按工作内容阅读：

- **整理或实现积木界面**：先读第2、3章，再查第4-14章；
- **实现上位机实时运行**：重点读第3、8、9、16章；
- **实现MicroPython代码生成**：直接查第17章完整映射；
- **核对EV3本地资源**：查第2.1节；
- **安排后续验证**：查第18、19章。

分类索引：

| 分类 | 数量 | 界面、参数和语义 | MicroPython映射 |
|---|---:|---|---|
| 电机 | 11 | [第4章](#4-电机积木) | [第17.1节](#171-电机11块) |
| 移动 | 11 | [第5章](#5-移动积木) | [第17.2节](#172-移动11块) |
| 显示 | 6 | [第6章](#6-显示积木) | [第17.3节](#173-显示6块) |
| 声音 | 6 | [第7章](#7-声音积木) | [第17.4节](#174-声音6块) |
| 事件 | 13 | [第8章](#8-事件积木) | [第17.5节](#175-事件13块) |
| 控制 | 9 | [第9章](#9-控制积木) | [第17.6节](#176-控制9块) |
| 传感器 | 34 | [第10-11章](#10-传感器公共菜单和范围) | [第17.7节](#177-传感器34块) |
| 运算符 | 16 | [第12章](#12-运算符积木) | [第17.8节](#178-运算符16块) |
| 变量和列表 | 9 | [第13章](#13-变量和列表积木) | [第17.9节](#179-变量和列表9块) |
| 我的模块 | 2 | [第14章](#14-我的模块) | [第17.10节](#1710-我的模块2块) |

核心数据流：

```text
中文界面积木
    ↓ stable ID
项目 AST / 存档
    ├─ EST VM → Device Service → USB HID → EST
    └─ 代码生成器 → MicroPython → est / est_runtime → est_* C API
```

## 1. 文档目的

本文记录 EV3 Classroom 中文界面中已经核实的积木、菜单、参数范围和执行语义，作为以下工作的共同输入：

1. EST OpenBlock 上位机的积木定义与中文文案；
2. EST VM 实时运行模式的 opcode 实现；
3. EST MicroPython 代码生成器；
4. `est_*` C 服务 API 的能力覆盖检查；
5. EV3 Classroom 兼容性测试和回归测试。

本文不是旧 EV3 固件或旧 EST 软件的反编译说明。界面名称用于兼容和用户迁移，内部实现必须使用稳定 ID，不能用中文显示文字作为协议或代码生成标识。

相关文档：

- `docs/EST_OpenBlock上位机建设方案_V1.md`
- `docs/EST_C_API与MicroPython下一步工作清单.md`
- `docs/EST_USB应用协议_V1.md`

## 2. 核对口径和当前结论

核对基线为本机 EV3 Classroom 1.5.2 简体中文界面、软件内置 `zh-CN` 资源和用户提供的界面截图。

采用以下证据规则：

- 截图中出现：确认当前界面可用；
- 用户明确确认：确认可用；
- 只在安装包共享源码中出现：标记为“候选”，不计入默认面板；
- 截图中没有出现：不能单独据此断定不存在，可能是滚动、裁切或渲染不完整；
- 创建变量、列表或模块的按钮不是程序积木，不计入积木数量。

当前确认 117 种积木形态：

| 分类 | 已确认数量 |
|---|---:|
| 电机 | 11 |
| 移动 | 11 |
| 显示 | 6 |
| 声音 | 6 |
| 事件 | 13 |
| 控制 | 9 |
| 传感器 | 34 |
| 运算符 | 16 |
| 变量和列表 | 9 |
| 我的模块 | 2 |
| 合计 | 117 |

### 2.1 本机 EV3 Classroom 安装与资源地址

为避免反复阅读超长绝对路径，本文使用以下路径别名：

```text
INSTALL_ROOT = C:\Program Files\WindowsApps\LEGOEducation.EV3ClassroomLEGOEducation_1.5.2.0_x64__by3p0hsm2jzfy
PACKAGE_ROOT = C:\Users\64264\AppData\Local\Packages\LEGOEducation.EV3ClassroomLEGOEducation_by3p0hsm2jzfy
CACHE_ROOT   = PACKAGE_ROOT\LocalCache\Roaming\MINDSTORMS_EDU\CefSharp\Cache
```

当前本机安装信息：

| 项目 | 当前值 |
|---|---|
| Appx 包名 | `LEGOEducation.EV3ClassroomLEGOEducation` |
| 版本 | `1.5.2.0` |
| Package Family Name | `LEGOEducation.EV3ClassroomLEGOEducation_by3p0hsm2jzfy` |
| 安装目录 | `INSTALL_ROOT` |
| 主程序 | `INSTALL_ROOT\EV3 Classroom-win-1.5.2.3740.exe` |
| Appx 清单 | `INSTALL_ROOT\AppxManifest.xml` |
| 用户数据根目录 | `PACKAGE_ROOT` |
| CefSharp 缓存目录 | `CACHE_ROOT` |

对积木兼容开发最有价值的当前缓存文件：

| 文件 | 当前大小 | SHA-256 | 已确认内容 |
|---|---:|---|---|
| `CACHE_ROOT\Cache\f_000002` | 9,527,493 字节 | `DDBCA145D25F3A5AD7B22A4FC18495B70C29DCB427CAB460D0FAA386394C1E3D` | EV3 专用中文积木文字、菜单、内置帮助和参数说明 |
| `CACHE_ROOT\Cache\f_000003` | 14,708,637 字节 | `9A88D0CC27BFEE7B3580CE8216F6FA1C3E6E9B9ADE9F511109CABC16075140E3` | 工具箱 XML、Scratch 中文资源、积木类、opcode、默认值和编译实现 |

可移植路径写法：

```text
%LOCALAPPDATA%\Packages\LEGOEducation.EV3ClassroomLEGOEducation_by3p0hsm2jzfy\LocalCache\Roaming\MINDSTORMS_EDU\CefSharp\Cache
```

注意事项：

- `WindowsApps` 安装目录带版本号，应用升级后路径会变化；
- `f_000002`、`f_000003` 是 Chromium/CefSharp 缓存文件名，清理缓存、重装或重新下载资源后可能变化；
- 后续开发不能把这两个缓存文件名硬编码进工具，应按内容特征重新搜索；
- 缓存中包含其他 LEGO 产品和通用 Scratch 的共享代码，不能把搜索到的所有积木自动视为 EV3 默认界面积木；
- 用户截图和实际项目文件仍是判断积木是否可达的重要证据。

重新定位当前安装和资源的 PowerShell 命令：

```powershell
$pkg = Get-AppxPackage -Name 'LEGOEducation.EV3ClassroomLEGOEducation'
$pkg | Select-Object Name, Version, InstallLocation, PackageFamilyName

$cache = Join-Path $env:LOCALAPPDATA (
    'Packages\' + $pkg.PackageFamilyName +
    '\LocalCache\Roaming\MINDSTORMS_EDU\CefSharp\Cache'
)

# EV3 专用中文积木文字和菜单
rg -a -l '"zh-CN":\{motor:' $cache

# Scratch 中文工具箱、积木定义和编译代码
rg -a -l 'ScratchMsgs\.locales\["zh-cn"\]' $cache
```

### 2.2 表格阅读约定

| 术语 | 含义 |
|---|---|
| 稳定 ID | 项目 AST、VM 和代码生成器使用的永久积木身份 |
| 中文界面形态 | 当前简体中文界面实际显示或经本地资源补全的文字 |
| 命令积木 | 执行动作，可上下连接 |
| 报告积木 | 圆形数值/文本输出，可嵌入参数框 |
| 布尔积木 | 六边形真假输出，可嵌入条件框 |
| 事件积木 | 帽形入口，触发一个独立程序堆 |
| 控制积木 | 包含程序体或改变程序堆运行状态 |
| 候选块 | 安装包源码存在，但当前界面尚未确认可达，不进入默认面板 |

第4-14章回答“界面上是什么、参数是什么、行为是什么”；第17章回答“生成MicroPython时具体输出什么”。同一个稳定 ID 会在这两部分各出现一次。

## 3. 公共参数和实现约定

| 参数 | EV3 Classroom 语义 | EST 实现约定 |
|---|---|---|
| 输出端口 | `A`、`B`、`C`、`D` | 使用稳定端口枚举，不直接传显示文字 |
| 输入端口 | `1`、`2`、`3`、`4` | 使用稳定端口枚举 |
| 功率 | `-100～100%` | C 层开环 PWM；再次校验范围 |
| 速度 | `-100～100%` | C 层闭环目标；负数表示反向 |
| 电机数量单位 | 圈、度、秒 | 圈乘 `360` 转为度；秒乘 `1000` 转为毫秒 |
| 停止方式 | 保持位置、惯性滑行 `(float)` | 映射到 EST 保持/滑行；主动刹车可作为 EST 扩展，不冒充 EV3 选项 |
| 比较 | 小于 `(<)`、大于 `(>)`、等于 `(=)` | 在 VM 或 MicroPython 层执行，不下沉为传感器 C API |
| 事件比较 | 小于、大于、等于、变化超过 | 需要保存上次采样值并做边沿判断 |
| 数值框 | 可放常量、变量或报告积木 | AST 保存表达式；C API 仍校验最终值 |
| 布尔框 | 六边形条件表达式 | AST 和 MicroPython 均使用布尔值 |

默认状态：

- 单电机默认速度 `75%`，默认停止方式为保持位置；
- 移动默认速度 `50%`，默认电机对为 `B/C`；
- 颜色传感器默认端口 `3`，触摸默认 `1`，陀螺仪默认 `2`，超声波和红外默认 `4`；
- 输入积木允许表达式，但生成器不得依赖界面已经完成范围检查。

## 4. 电机积木

| 稳定 ID | 中文界面形态 | 参数与语义 | EST / MicroPython 映射建议 |
|---|---|---|---|
| `motor_run_for` | `[A] [顺时针] 运行 [1] [圈]` | A-D；顺/逆时针；圈/度/秒；使用端口预设速度并等待完成 | `Motor.run_angle()` 或 `run_time()`，`wait=True` |
| `motor_start` | `[A] [顺时针] 启动电机` | 使用预设速度，持续到新命令或程序停止 | `Motor.run_speed()`，方向决定速度符号 |
| `motor_stop` | `[A] 停止电机` | 使用端口预设停止方式 | `Motor.stop()` |
| `motor_set_speed` | `[A] 将速度设置为 [75] %` | -100～100；影响不自带速度参数的运行块 | 保存 `motor_speed[port]` 状态 |
| `motor_set_stop_action` | `[A] 将电机设置为在停止处 [保持位置]` | 保持位置/惯性滑行 | 保存 `motor_stop[port]` 状态 |
| `motor_run_for_speed` | `[A] 以 [75] % 的速度运行 [1] [圈]` | -100～100；负数反向；圈/度/秒；等待完成 | `run_angle()`/`run_time()`，`wait=True` |
| `motor_start_speed` | `[A] 以 [75] % 的速度启动电机` | 闭环速度，持续运行 | `Motor.run_speed()` |
| `motor_start_power` | `[A] 以 [100] % 的功率启动电机` | 开环功率，不保证实际转速 | `Motor.run_power()` |
| `motor_reset_degrees` | `[A] 重置运转度数` | 将累计度数归零 | `Motor.reset_angle()` |
| `motor_degrees` | `[A] 运转度数` | 有符号累计度数；顺时针增加、逆时针减少 | `Motor.angle()` |
| `motor_speed` | `[A] 速度` | 当前实际速度百分比，不是目标速度 | `Motor.speed()` |

实现备注：

- 电机重新连接、程序启动或执行复位块后，兼容层应把运转度数基准置零；
- 单次角度需遵守 EST C API 当前 `-3600～3600°` 限制，超长动作由上层拆分或返回明确错误；
- 功率和速度必须保持为两个不同接口。

## 5. 移动积木

| 稳定 ID | 中文界面形态 | 参数与语义 | EST / MicroPython 映射建议 |
|---|---|---|---|
| `drive_move_for` | `向 [前] 移动 [1] [圈]` | 前/后；圈/度/秒；使用移动预设速度并等待完成 | `MotorPair` 同步角度或时间运行 |
| `drive_steer_for` | `向 [前:0] 移动 [1] [圈]` | 转向 -100～100；0 直行；使用预设速度 | 统一调用共享 `drive_steer()`，不得在多个层各写一套混控公式 |
| `drive_start_steer` | `开始向 [前:0] 移动` | 按转向值持续移动 | 非阻塞 `DriveBase.steer()` |
| `drive_stop` | `停止运动` | 按预设停止方式同时停止电机对 | `DriveBase.stop()` |
| `drive_set_speed` | `将移动速度设置为 [50] %` | -100～100；影响不自带速度的移动块 | 保存 `drive_speed` 状态 |
| `drive_set_pair` | `将运转电机设置为 [B] 和 [C]` | 左右端口 A-D；默认 B/C；要求同类型电机 | `DriveBase(left, right, ...)` |
| `drive_set_stop_action` | `将运转电机设置为停止时 [保持位置]` | 保持位置/惯性滑行 | 保存 `drive_stop` 状态 |
| `drive_steer_for_speed` | `以 [50] % 的速度向 [前:0] 移动 [1] [圈]` | 指定速度、转向和数量，等待完成 | `drive_steer(..., wait=True)` |
| `drive_dual_speed_for` | `以 [50] [50] % 的速度移动 [1] [圈]` | 左右速度各 -100～100，圈/度/秒 | `MotorPair.run(...)` |
| `drive_start_steer_speed` | `以 [50] % 的速度开始向 [前:0] 移动` | 指定速度和转向，持续运行 | 非阻塞 `drive_steer()` |
| `drive_start_dual_speed` | `以 [50] [50] % 的速度开始移动` | 左右速度独立，持续运行 | 非阻塞 `MotorPair.run()` |

EV3 Classroom 1.5.2 缓存中的 `TravelGuideBuilder.steeringToSpeeds()` 已给出转向混控公式。设转向值为 `s`、移动速度为 `v`：当 `|s| < 100` 时，左右原始速度为 `100+s`、`100-s`；当 `|s| = 100` 时为 `s`、`-s`。两路原始速度分别限制到 `-100～100` 后，再乘以 `v/100`。因此 `s=0` 为直行，`s=+50` 为左轮全速/右轮半速，`s=-50` 相反，`s=±100` 为两轮反向原地旋转。上位机实时模式、MicroPython 生成代码和 C 服务层必须复用这一实现及同一组测试向量。

## 6. 显示积木

| 稳定 ID | 中文界面形态 | 参数与语义 | EST / MicroPython 映射建议 |
|---|---|---|---|
| `display_image_for` | `显示 [Eyes / Neutral] [2] 秒` | 显示资源并等待指定秒数 | `Display.image()` 后程序延时 |
| `display_image` | `显示 [Eyes / Neutral]` | 显示资源，不等待 | `Display.image()` |
| `display_text_line` | `在第 [1] 行写入 [EV3]` | 可见行约 1-12；内容保持到覆盖或清屏 | `Display.text_line()` |
| `display_text_xy` | `使用字体 [常规黑色] 在 [1], [1] 处写入 [EV3]` | EV3 可见区 X=0-177、Y=0-127 | `Display.text()` |
| `display_clear` | `清除显示` | 清空屏幕 | `Display.clear()` |
| `display_status_light` | `将状态灯设置为 [绿色]` | 见状态灯菜单 | `LED.set()`，映射策略待冻结 |

字体菜单：常规黑色、粗体黑色、大号黑色、常规白色、粗体白色、大号白色。

状态灯菜单：关闭、绿色、红色、橙色、绿色闪烁、红色闪烁、橙色闪烁。

兼容说明：EV3 可见区为 `178×128`，当前 EST C API 规划为 `180×128`。兼容模式建议保留左上角 `178×128` 坐标，不缩放；EST 原生积木可另行使用完整宽度。EST 红/蓝灯如何映射 EV3 绿/红/橙及闪烁模式尚未冻结。

## 7. 声音积木

| 稳定 ID | 中文界面形态 | 参数与语义 | EST / MicroPython 映射建议 |
|---|---|---|---|
| `sound_play_wait` | `播放声音 [Communication / Hello] 直到完成` | 播放资源并等待结束 | `Audio.play(name, wait=True)` |
| `sound_play` | `开始播放声音 [Communication / Hello]` | 非阻塞播放 | `Audio.play(name, wait=False)` |
| `sound_beep_for` | `播放警笛声 [60] [0.2] 秒` | MIDI 音符和秒数，等待结束 | `Audio.tone(note, duration_ms, wait=True)` |
| `sound_beep` | `开始播放警笛声 [60]` | 持续音调，直到新声音命令 | `Audio.tone(note)` |
| `sound_stop_all` | `停止所有声音` | 停止当前声音和音调 | `Audio.stop()` |
| `sound_set_volume` | `将音量设置为 [100] %` | 0-100 | `Audio.volume()` |

声音菜单使用 `分类 / 名称`，例如 `Communication / Hello`。代码生成器和项目文件应保存稳定资源 ID，同时保存显示名称，不能只保存本地化字符串。

## 8. 事件积木

所有事件帽积木都应按边沿触发，不能在条件持续成立时每个调度周期重复启动同一程序堆。实时 VM 和 MicroPython runtime 需要一致的事件队列、重入和停止规则。

| 稳定 ID | 中文界面形态 | 参数与触发语义 |
|---|---|---|
| `event_program_start` | `当程序启动时` | 程序开始时触发一次 |
| `event_color` | `[3] 当颜色为 [红色]` | 端口1-4；指定颜色或颜色已改变 |
| `event_touch` | `[1] 当 [被按压]` | 被按压/被松开 |
| `event_ultrasonic` | `[4] 当距离 [小于 (<)] [15] [厘米]` | 比较或变化事件；厘米/英寸 |
| `event_ir_proximity` | `[4] 当近程 [小于 (<)] [15] %` | 0-100 |
| `event_ir_beacon_button` | `[4] 当信标 [1] [左上按钮被按压] 时` | 信道1-4；信标按键事件 |
| `event_gyro_angle` | `[2] 当角度 [小于 (<)] [45]° 时` | 有符号角度；比较或变化事件 |
| `event_brick_button` | `当 [中] 按钮 [被按压]` | 无/左/中/右/上/下；按压/松开 |
| `event_condition` | `当 [条件]` | 条件从假变真时触发 |
| `event_broadcast_received` | `当接收到 [消息1]` | 收到同名广播时触发 |
| `event_broadcast` | `广播 [消息1]` | 发出消息，不等待接收程序结束 |
| `event_broadcast_wait` | `广播 [消息1] 并等待` | 等待本次接收程序全部结束 |
| `event_timer` | `当计时器 > [10]` | 计时器首次越过阈值时触发 |

比较事件菜单：小于 `(<)`、大于 `(>)`、等于 `(=)`、变化超过。

颜色事件菜单：无色、黑色、蓝色、绿色、黄色、红色、白色、棕色、已改变。

信标事件菜单：左上按钮被按压、左下按钮被按压、未按压左按钮、右上按钮被按压、右下按钮被按压、未按压右按钮、信标处于活动状态。

## 9. 控制积木

| 稳定 ID | 中文界面形态 | 参数与语义 | 代码生成建议 |
|---|---|---|---|
| `control_wait_seconds` | `等待 [1] 秒` | 程序延时，建议非负 | `sleep_ms(round(seconds * 1000))` |
| `control_wait_until` | `等待 [条件]` | 挂起当前程序堆，直到条件成立 | 轮询时必须让出调度器 |
| `control_repeat` | `重复执行 [10] 次` | 次数按非负整数处理 | `for` 循环 |
| `control_forever` | `重复执行` | 无限循环 | `while True`，循环内让出调度器 |
| `control_repeat_until` | `重复执行直到 [条件]` | 条件成立时结束 | `while not condition` |
| `control_if` | `如果 [条件] 那么` | 单分支 | `if` |
| `control_if_else` | `如果 [条件] 那么 ... 否则 ...` | 双分支 | `if/else` |
| `control_stop_other_stacks` | `停止其它程序堆` | 停止当前堆以外的运行堆 | runtime 调度器操作 |
| `control_stop` | `停止 [并退出程序]` | 所有程序堆/此程序堆/其它程序堆/并退出程序 | 使用内部停止异常或调度器状态，不生成普通 `return` 冒充全部语义 |

程序停止、窗口关闭、Python 异常和设备断开必须最终调用 C 层安全停止入口，不能只停止脚本调度。

## 10. 传感器公共菜单和范围

| 项目 | 选项或范围 |
|---|---|
| 颜色编号 | 0无色、1黑、2蓝、3绿、4黄、5红、6白、7棕 |
| 光强 | 0-100% |
| 超声波距离 | 0-255厘米或0-100英寸；EST C API 内部统一毫米 |
| 红外近程 | 0-100 |
| 红外信标朝向 | -25～25 |
| 红外信道 | 1-4 |
| 主控按钮值 | 0无、1左、2中、3右、4上、5下 |
| 信标按钮代码 | 0无键；1左上；2左下；3右上；4右下；5-8组合；9信标；10左双键；11右双键 |

## 11. 传感器积木

### 11.1 主控按钮

| 稳定 ID | 中文界面形态 | 语义与映射 |
|---|---|---|
| `sensor_brick_button_value` | `按钮` | 返回当前按钮代码；`Buttons.value()` |
| `sensor_brick_button_pressed` | `[中] 按钮是否被按压？` | 返回布尔值；`Buttons.pressed(button)` |
| `sensor_wait_brick_button` | `等待直到 [中] 按钮 [被按压]` | 等待按压/松开边沿 |

### 11.2 颜色传感器

| 稳定 ID | 中文界面形态 | 语义与映射 |
|---|---|---|
| `sensor_color_calibrate_reflection` | `将反射光线强度从 [最小值] 校准至 [值]` | 最小值/最大值；目标标度0-100；界面没有端口参数，EST 如何选择校准对象待实机行为测试 |
| `sensor_color_reset_calibration` | `重置反射光线强度校准` | 重置校准；端口选择语义同上 |
| `sensor_color_reflection` | `[3] 反射光线强度` | 0-100；`ColorSensor.reflection()` |
| `sensor_color_reflection_compare` | `[3] 反射光线强度是否 [小于 (<)] [50]%？` | VM/MicroPython 比较，不下沉 C API |
| `sensor_color_ambient` | `[3] 环境光强度` | 0-100；`ColorSensor.ambient()` |
| `sensor_color_ambient_compare` | `[3] 环境光强度是否 [小于 (<)] [50]%？` | VM/MicroPython 比较 |
| `sensor_color_value` | `[3] 颜色` | 返回0-7颜色编号；`ColorSensor.color()` |
| `sensor_color_is` | `[3] 颜色是否为 [红色]？` | 颜色枚举比较 |
| `sensor_wait_color` | `[3] 等待直到颜色为 [红色]` | 等待指定颜色或颜色改变 |

### 11.3 触摸传感器

| 稳定 ID | 中文界面形态 | 语义与映射 |
|---|---|---|
| `sensor_touch_pressed` | `[1] 是否被按压？` | `TouchSensor.pressed()` |
| `sensor_wait_touch` | `[1] 等待直到 [被按压]` | 被按压/被松开 |

### 11.4 超声波传感器

| 稳定 ID | 中文界面形态 | 语义与映射 |
|---|---|---|
| `sensor_ultrasonic_distance` | `[4] 距离，单位为 [厘米]` | C API毫米值在上层转换为厘米/英寸 |
| `sensor_ultrasonic_compare` | `[4] 距离是否 [小于 (<)] [15] [厘米]？` | VM/MicroPython比较 |
| `sensor_wait_ultrasonic` | `[4] 等待直到距离 [小于 (<)] [15] [厘米]` | 支持小于/大于/等于/变化超过 |

### 11.5 红外传感器和信标

| 稳定 ID | 中文界面形态 | 语义与映射 |
|---|---|---|
| `sensor_ir_proximity` | `[4] 近程` | 返回0-100 |
| `sensor_ir_proximity_compare` | `[4] 近程是否 [小于 (<)] [15]%？` | VM/MicroPython比较 |
| `sensor_wait_ir_proximity` | `[4] 等待直到近程 [小于 (<)] [15]%` | 支持比较和变化事件 |
| `sensor_ir_beacon_heading` | `[4] 前往信标 [1]` | 返回朝向 -25～25 |
| `sensor_ir_beacon_proximity` | `[4] 信标 [1] 近程` | 返回0-100 |
| `sensor_ir_beacon_buttons` | `[4] 按压信标 [1] 按钮` | 返回0-11按钮代码；中文显示较生硬但保持兼容 |
| `sensor_ir_beacon_button_pressed` | `[4] 信标 [1] [无按钮] 是否被按压？` | 无按钮/左上/左下/右上/右下/信标按钮 |
| `sensor_wait_ir_beacon_button` | `[4] 等待直到信标 [1] [左上按钮被按压]` | 等待信标按钮事件 |
| `sensor_ir_beacon_active` | `[4] 信标 [1] 是否处于活动状态？` | 返回布尔值 |
| `sensor_ir_beacon_active_compare` | `[4] 信标 [1] 是否 [朝向] [小于 (<)] [值]？` | 朝向或近程与阈值比较 |

### 11.6 陀螺仪

| 稳定 ID | 中文界面形态 | 语义与映射 |
|---|---|---|
| `sensor_gyro_angle` | `[2] 角度` | 有符号累计角度；`GyroSensor.angle()` |
| `sensor_gyro_rate` | `[2] 角速度` | 有符号角速度；`GyroSensor.speed()` |
| `sensor_gyro_reset` | `[2] 重置角度` | 当前方向设为0 |
| `sensor_gyro_compare` | `[2] 角度是否 [小于 (<)] [45]°？` | VM/MicroPython比较 |
| `sensor_wait_gyro` | `[2] 等待直到角度 [小于 (<)] [45]°` | 支持比较和变化超过 |

### 11.7 计时器

| 稳定 ID | 中文界面形态 | 语义与映射 |
|---|---|---|
| `sensor_timer` | `计时器` | 返回启动或复位后的秒数 |
| `sensor_timer_reset` | `重置计数器` | 实际重置计时器；保留界面原文，内部 ID 使用 timer |

## 12. 运算符积木

| 稳定 ID | 中文界面形态 | 参数与语义 |
|---|---|---|
| `operator_random` | `在 [1] 和 [10] 之间取随机数` | 两端均为整数时返回包含端点的整数，否则可返回小数 |
| `operator_add` | `[ ] + [ ]` | 加法 |
| `operator_subtract` | `[ ] - [ ]` | 减法 |
| `operator_multiply` | `[ ] * [ ]` | 乘法 |
| `operator_divide` | `[ ] / [ ]` | 除数为0时按当前EV3编译语义返回0 |
| `operator_less_than` | `[ ] < [100]` | 小于，返回布尔值 |
| `operator_equals` | `[ ] = [100]` | 等于，返回布尔值 |
| `operator_greater_than` | `[ ] > [100]` | 大于，返回布尔值 |
| `operator_and` | `[条件] 与 [条件]` | 逻辑与 |
| `operator_or` | `[条件] 或 [条件]` | 逻辑或 |
| `operator_not` | `[条件] 不成立` | 逻辑非 |
| `operator_join` | `连接 [apple] 和 [banana]` | 文本连接 |
| `operator_length` | `[apple] 的字符数` | 返回字符数 |
| `operator_mod` | `[ ] 除以 [ ] 的余数` | 余数；除数为0的兼容结果需要测试冻结 |
| `operator_round` | `四舍五入 [ ]` | 四舍五入到整数 |
| `operator_math` | `[绝对值] [ ]` | 一个积木，下拉菜单选择数学函数 |

数学函数菜单：绝对值、向下取整、向上取整、平方根、`sin`、`cos`、`tan`、`asin`、`acos`、`atan`、`ln`、`log`、`e ^`、`10 ^`。三角函数按 Scratch/EV3 兼容语义使用角度制。

## 13. 变量和列表积木

用户已确认以下9种积木均存在。`建立一个变量`和`建立一个列表`是创建按钮，不计入积木数量。

| 稳定 ID | 中文界面形态 | 语义与代码生成 |
|---|---|---|
| `data_variable` | `[变量名]` | 读取变量；Python局部或运行时变量 |
| `data_set_variable` | `将 [变量] 设为 [值]` | 赋值 |
| `data_change_variable` | `将 [变量] 增加 [值]` | 按Scratch数值转换规则相加 |
| `data_list` | `[列表名]` | 返回列表内容 |
| `data_list_add` | `将 [项目] 加入 [列表]` | 追加项目 |
| `data_list_delete_all` | `删除 [列表] 的全部项目` | 清空列表 |
| `data_list_replace` | `将 [列表] 的第 [N] 项替换为 [项目]` | UI索引从1开始；越界不执行 |
| `data_list_item` | `[列表] 的第 [N] 项` | UI索引从1开始；生成Python时转换为 `N-1`；越界返回空值 |
| `data_list_length` | `[列表] 的项目数` | Python `len(list)` |

项目文件必须使用变量和列表的内部唯一 ID，显示名称允许重命名。不能仅按名称绑定，否则重命名会破坏积木引用。

## 14. 我的模块

分类名称为“我的模块”，创建按钮为“制作新的积木”。创建界面支持：

- 添加输入：数字或文本；
- 添加输入：布尔；
- 添加标签：固定显示文字；
- 删除当前选中的输入或标签；
- 保存或取消。

创建后有两种积木形态：

| 稳定 ID | 中文界面形态 | 语义与代码生成 |
|---|---|---|
| `procedure_definition` | `定义 [模块名称] [参数...]` | 定义无返回值过程；生成 Python `def` |
| `procedure_call` | `[模块名称] [参数...]` | 调用过程；参数按定义顺序传入 |

模块名称、参数和标签必须在项目 AST 中使用稳定 ID。显示名称可编辑；数字或文本参数为圆形输入，布尔参数为六边形输入。我的模块没有报告值，不能生成带返回值的 Python 函数语义。

## 15. 源码存在但界面未确认的候选块

以下块不计入已确认的117种默认积木。实现 EST V1 默认面板时不得自动加入；如后续界面或项目文件证明确实可用，再转入正式清单。

### 15.1 Scratch 通用声音候选8块

- `播放声音 [声音] 等待播完`
- `播放声音 [声音]`
- `将 [音调/左右平衡] 音效增加 [值]`
- `将 [音调/左右平衡] 音效设为 [值]`
- `清除音效`
- `将音量增加 [值]`
- `将音量设为 [值]%`
- `音量`

### 15.2 运算候选3块

- `[文本] 的第 [N] 个字符`
- `[文本] 包含 [文本]？`
- `[值] 介于 [下限] 和 [上限] 之间`

## 16. 上位机和 MicroPython 落地规则

### 16.1 稳定 ID 与 MicroPython 函数名的关系

稳定 ID 不是 MicroPython 函数名。稳定 ID 是积木在项目 AST、VM 分发、代码生成、测试和版本迁移中的永久身份，例如 `motor_run_for`。MicroPython 函数名是该积木在某个目标后端生成的实现，例如 `Motor.run_angle()`。

两者关系通常是一对一、一对多或不生成函数：

| 稳定 ID 示例 | MicroPython 输出示例 | 关系说明 |
|---|---|---|
| `motor_start_power` | `Motor("A").run_power(50)` | 通常一对一映射到类方法 |
| `motor_run_for` + 单位“圈/度” | `Motor("A").run_angle(...)` | 同一稳定 ID 根据单位选择参数换算 |
| `motor_run_for` + 单位“秒” | `Motor("A").run_time(...)` | 同一稳定 ID 可映射到另一个函数 |
| `control_if` | Python `if` 语句 | 生成语法结构，不调用函数 |
| `operator_add` | Python `+` 运算符 | 生成表达式，不调用函数 |
| `event_program_start` | runtime 入口或事件注册代码 | 需要调度器，不是普通函数调用 |
| `data_set_variable` | Python 赋值语句 | 生成变量操作 |

必须保持以下边界：

- 项目文件保存稳定 ID，不保存 MicroPython 方法名；
- 稳定 ID 在显示语言变化、MicroPython API 重命名或后端切换后仍保持不变；
- MicroPython 代码生成器维护“稳定 ID -> 生成规则”的映射表；
- 实时 VM 维护“稳定 ID -> Device Service 操作”的映射表；
- C/MicroPython API 名称需要单独冻结，不能反过来决定项目 AST 格式；
- 一个积木需要多个底层调用时，仍只保留一个稳定 ID。

### 16.2 AST 与 opcode

- 使用本文稳定 ID 或其版本化等价物作为 AST opcode；
- 中文、英文和其他语言只是显示资源，不参与协议判断；
- 菜单值保存枚举 ID，例如 `clockwise`、`degrees`、`hold`，不保存“顺时针”等文字；
- 项目文件必须记录 schema 版本，并为未来字段迁移提供升级函数；
- 资源保存稳定资源 ID、类型和显示名称。

### 16.3 实时模式

- OpenBlock VM 调用唯一的 EST Device Service，不直接访问 HID；
- 运动完成等待必须读取 C 控制状态，不能由 JavaScript 定时器模拟角度闭环；
- 传感器比较和普通流程在 VM 中处理；
- 停止程序、设备断开和窗口关闭统一调用 `stopAll()`；
- 事件轮询必须有采样周期、防抖、边沿检测和重复触发保护。

### 16.4 MicroPython 生成模式

- 电机、底盘、传感器、显示和声音调用 `est` 模块；
- 圈、秒、厘米和英寸在生成层转换为 C API 约定单位；
- `wait=True` 轮询 C 状态，不能用 Python `sleep` 代替电机内部计时和闭环；
- 控制、变量、列表、比较和我的模块优先生成普通 Python；
- 事件帽积木需要轻量调度器，不能简单生成多个无法并发的顶层死循环；
- Python 异常、停止和软重启路径必须进入 C 层安全清理。

### 16.5 M0.98A 已冻结的只读底层契约

- `est.Sensor(port)` 接受 `1..4`，提供 `port()`、`type()`、`state()`、`mode()`、`value_format()`、`valid()`、`error()`、`value()` 和 `status()`；`value()` 使用 `est_sensor` 已统一的有符号数和单位，未连接、同步中、陈旧或无有效值时抛出明确异常。
- `Sensor.status()` 固定返回 `(error, type, state, mode, value_format, valid, value)`；传感器类型和状态常量由 `est.Sensor.TYPE_*`、`est.Sensor.STATE_*` 提供。
- `est.buttons.value()` 固定返回六位按键掩码；`BACK/LEFT/UP/DOWN/RIGHT/CENTER` 分别为 `1/2/4/8/16/32`，`pressed(button)` 只接受一个单键常量。
- `est.battery.status()` 固定返回 `(result, valid, level, percent, low, adc_raw, sample_mv)`；普通积木优先使用 `valid()`、`level()`、`percent()` 和 `low()`。
- 上述接口是类型便捷类和 `est_runtime` 的底层依据；代码生成器仍按第 17 章生成 `rt.color(port)` 等高层调用，不直接展开状态元组。

### 16.6 M0.99A 已冻结的马达状态与停止契约

- `est.Motor(port)` 接受字符串 `"A".."D"`，提供 `port()`、`type()`、`state()`、`stop_mode()`、`power()`、`target_speed()`、`speed()`、`angle()`、`error()`、`status()` 和 `stop()`。
- `Motor.status()` 固定返回 `(error, type, state, stop_mode, power, target_speed, speed, angle)`；类型、状态和停止方式常量由 `est.Motor.TYPE_*`、`STATE_*`、`STOP_COAST` 和 `STOP_BRAKE` 提供。
- `stop()` 默认自由滑行，`stop(STOP_BRAKE)` 主动刹车；`HOLD` 尚未支持，不得静默降级。脚本启动前、正常结束、异常、超时和主动停止仍由 C 层统一停止全部马达。
- M0.99A 尚未开放任何启动马达的方法；代码生成器在下一阶段运行 API 冻结前，不得生成 `run_power`、`run_speed`、`run_time` 或 `run_angle` 调用。

## 17. 全部积木的 MicroPython 代码生成映射

本节定义建议的 V1 代码生成契约，当前尚不表示这些 `est`/`est_runtime` 接口已经全部实现。建议生成程序统一使用：

```python
import est
import est_runtime as rt
```

约定：

- `est` 是稳定的硬件 API，包装 `est_*` C 服务层；
- `est_runtime` 是随生成程序提供的 EV3/Scratch 兼容运行时，负责状态、事件、类型转换、1基列表和停止程序堆；
- `<port>`、`<value>`、`<condition>` 等表示由子积木生成的表达式；
- `stack_N`、`var_ID`、`list_ID`、`proc_ID` 使用项目内部唯一 ID 生成，不使用可能重复的显示名称；
- 表中的代码模板是生成目标，不是积木的稳定 ID。

### 17.1 电机：11块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `motor_run_for` | `rt.motor_run_for(<port>, <direction>, <value>, <unit>)` |
| `motor_start` | `rt.motor_start(<port>, <direction>)` |
| `motor_stop` | `rt.motor_stop(<port>)` |
| `motor_set_speed` | `rt.motor_set_speed(<port>, <speed>)` |
| `motor_set_stop_action` | `rt.motor_set_stop_action(<port>, <stop_action>)` |
| `motor_run_for_speed` | `rt.motor_run_for(<port>, None, <value>, <unit>, speed=<speed>)` |
| `motor_start_speed` | `rt.motor(<port>).run_speed(<speed>)` |
| `motor_start_power` | `rt.motor(<port>).run_power(<power>)` |
| `motor_reset_degrees` | `rt.motor(<port>).reset_angle()` |
| `motor_degrees` | `rt.motor(<port>).angle()` |
| `motor_speed` | `rt.motor(<port>).speed()` |

`rt.motor_run_for()` 根据单位调用 `Motor.run_angle()` 或 `Motor.run_time()`；圈先乘360，秒先乘1000。未显式传速度时读取 `rt.motor_set_speed()` 保存的端口状态。

### 17.2 移动：11块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `drive_move_for` | `rt.drive_move_for(<direction>, <value>, <unit>)` |
| `drive_steer_for` | `rt.drive_steer_for(<steering>, <value>, <unit>)` |
| `drive_start_steer` | `rt.drive_start_steer(<steering>)` |
| `drive_stop` | `rt.drive_stop()` |
| `drive_set_speed` | `rt.drive_set_speed(<speed>)` |
| `drive_set_pair` | `rt.drive_set_pair(<left_port>, <right_port>)` |
| `drive_set_stop_action` | `rt.drive_set_stop_action(<stop_action>)` |
| `drive_steer_for_speed` | `rt.drive_steer_for(<steering>, <value>, <unit>, speed=<speed>)` |
| `drive_dual_speed_for` | `rt.drive_dual_speed_for(<left_speed>, <right_speed>, <value>, <unit>)` |
| `drive_start_steer_speed` | `rt.drive_start_steer(<steering>, speed=<speed>)` |
| `drive_start_dual_speed` | `rt.drive_start_dual_speed(<left_speed>, <right_speed>)` |

移动 helper 必须调用 C 层双电机同步能力。上位机实时模式和生成模式必须复用同一套转向混控测试向量。

### 17.3 显示：6块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `display_image_for` | `rt.display_image_for(<asset_id>, <seconds>)` |
| `display_image` | `est.display.image(<asset_id>)` |
| `display_text_line` | `est.display.text_line(<line>, <text>)` |
| `display_text_xy` | `est.display.text(<x>, <y>, <text>, font=<font>)` |
| `display_clear` | `est.display.clear()` |
| `display_status_light` | `est.led.set(<status_mode>)` |

`rt.display_image_for()` 显示图片后只延迟当前程序堆；资源 ID 在构建阶段解析为设备资源路径或资源表编号。

### 17.4 声音：6块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `sound_play_wait` | `est.audio.play(<asset_id>, wait=True)` |
| `sound_play` | `est.audio.play(<asset_id>, wait=False)` |
| `sound_beep_for` | `est.audio.tone(<midi_note>, rt.seconds_to_ms(<seconds>), wait=True)` |
| `sound_beep` | `est.audio.tone(<midi_note>)` |
| `sound_stop_all` | `est.audio.stop()` |
| `sound_set_volume` | `est.audio.set_volume(<volume>)` |

### 17.5 事件：13块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `event_program_start` | `@rt.on_start` 后定义 `def stack_N(): <body>` |
| `event_color` | `@rt.on_color(<port>, <color_event>)` 后定义程序堆函数 |
| `event_touch` | `@rt.on_touch(<port>, <touch_event>)` 后定义程序堆函数 |
| `event_ultrasonic` | `@rt.on_ultrasonic(<port>, <event>, <value>, <unit>)` |
| `event_ir_proximity` | `@rt.on_ir_proximity(<port>, <event>, <value>)` |
| `event_ir_beacon_button` | `@rt.on_ir_beacon_button(<port>, <channel>, <event>)` |
| `event_gyro_angle` | `@rt.on_gyro_angle(<port>, <event>, <value>)` |
| `event_brick_button` | `@rt.on_brick_button(<button>, <event>)` |
| `event_condition` | `@rt.on_condition(lambda: <condition>)` |
| `event_broadcast_received` | `@rt.on_broadcast(<message_id>)` 后定义程序堆函数 |
| `event_broadcast` | `rt.broadcast(<message_id>, wait=False)` |
| `event_broadcast_wait` | `rt.broadcast(<message_id>, wait=True)` |
| `event_timer` | `@rt.on_timer_gt(<seconds>)` 后定义程序堆函数 |

所有装饰器在导入阶段注册程序堆，最后由生成器输出 `rt.run()` 启动调度器。事件回调不能在中断上下文直接执行硬件阻塞操作。

### 17.6 控制：9块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `control_wait_seconds` | `rt.sleep(<seconds>)` |
| `control_wait_until` | `rt.wait_until(lambda: <condition>)` |
| `control_repeat` | `for _ in range(rt.repeat_count(<count>)): <body>` |
| `control_forever` | `while True: <body>; rt.yield_once()` |
| `control_repeat_until` | `while not rt.boolean(<condition>): <body>; rt.yield_once()` |
| `control_if` | `if rt.boolean(<condition>): <body>` |
| `control_if_else` | `if rt.boolean(<condition>): <then_body> else: <else_body>` |
| `control_stop_other_stacks` | `rt.stop_other_stacks()` |
| `control_stop` | `rt.stop(<stop_scope>)` |

代码生成器必须按 Python 缩进输出循环和分支。`rt.stop()` 通过调度器内部异常/状态退出对应程序堆，并在“退出程序”路径调用安全停机。

### 17.7 传感器：34块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `sensor_brick_button_value` | `est.buttons.value()` |
| `sensor_brick_button_pressed` | `est.buttons.pressed(<button>)` |
| `sensor_wait_brick_button` | `rt.wait_brick_button(<button>, <event>)` |
| `sensor_color_calibrate_reflection` | `rt.color_calibrate(<calibrate_option>, <value>)` |
| `sensor_color_reset_calibration` | `rt.color_reset_calibration()` |
| `sensor_color_reflection` | `rt.color(<port>).reflection()` |
| `sensor_color_reflection_compare` | `rt.compare(rt.color(<port>).reflection(), <comparator>, <value>)` |
| `sensor_color_ambient` | `rt.color(<port>).ambient()` |
| `sensor_color_ambient_compare` | `rt.compare(rt.color(<port>).ambient(), <comparator>, <value>)` |
| `sensor_color_value` | `rt.color(<port>).color()` |
| `sensor_color_is` | `rt.color(<port>).color() == <color_id>` |
| `sensor_wait_color` | `rt.wait_color(<port>, <color_event>)` |
| `sensor_touch_pressed` | `rt.touch(<port>).pressed()` |
| `sensor_wait_touch` | `rt.wait_touch(<port>, <touch_event>)` |
| `sensor_ultrasonic_distance` | `rt.ultrasonic(<port>).distance(<unit>)` |
| `sensor_ultrasonic_compare` | `rt.compare(rt.ultrasonic(<port>).distance(<unit>), <comparator>, <value>)` |
| `sensor_wait_ultrasonic` | `rt.wait_ultrasonic(<port>, <event>, <value>, <unit>)` |
| `sensor_ir_proximity` | `rt.infrared(<port>).proximity()` |
| `sensor_ir_proximity_compare` | `rt.compare(rt.infrared(<port>).proximity(), <comparator>, <value>)` |
| `sensor_wait_ir_proximity` | `rt.wait_ir_proximity(<port>, <event>, <value>)` |
| `sensor_ir_beacon_heading` | `rt.infrared(<port>).beacon_heading(<channel>)` |
| `sensor_ir_beacon_proximity` | `rt.infrared(<port>).beacon_proximity(<channel>)` |
| `sensor_ir_beacon_buttons` | `rt.infrared(<port>).beacon_buttons(<channel>)` |
| `sensor_ir_beacon_button_pressed` | `rt.infrared(<port>).beacon_button_pressed(<channel>, <button>)` |
| `sensor_wait_ir_beacon_button` | `rt.wait_ir_beacon_button(<port>, <channel>, <event>)` |
| `sensor_ir_beacon_active` | `rt.infrared(<port>).beacon_active(<channel>)` |
| `sensor_ir_beacon_active_compare` | `rt.ir_beacon_compare(<port>, <channel>, <heading_or_proximity>, <comparator>, <value>)` |
| `sensor_gyro_angle` | `rt.gyro(<port>).angle()` |
| `sensor_gyro_rate` | `rt.gyro(<port>).speed()` |
| `sensor_gyro_reset` | `rt.gyro(<port>).reset_angle()` |
| `sensor_gyro_compare` | `rt.compare(rt.gyro(<port>).angle(), <comparator>, <value>)` |
| `sensor_wait_gyro` | `rt.wait_gyro(<port>, <event>, <value>)` |
| `sensor_timer` | `rt.timer_seconds()` |
| `sensor_timer_reset` | `rt.reset_timer()` |

颜色校准积木没有端口参数，因此 `rt.color_calibrate()` 和 `rt.color_reset_calibration()` 的目标选择规则必须等实机行为测试后冻结，不能由代码生成器自行猜测。

### 17.8 运算符：16块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `operator_random` | `rt.random_between(<from>, <to>)` |
| `operator_add` | `rt.number(<a>) + rt.number(<b>)` |
| `operator_subtract` | `rt.number(<a>) - rt.number(<b>)` |
| `operator_multiply` | `rt.number(<a>) * rt.number(<b>)` |
| `operator_divide` | `rt.safe_divide(<a>, <b>)` |
| `operator_less_than` | `rt.scratch_less_than(<a>, <b>)` |
| `operator_equals` | `rt.scratch_equals(<a>, <b>)` |
| `operator_greater_than` | `rt.scratch_greater_than(<a>, <b>)` |
| `operator_and` | `rt.boolean(<a>) and rt.boolean(<b>)` |
| `operator_or` | `rt.boolean(<a>) or rt.boolean(<b>)` |
| `operator_not` | `not rt.boolean(<value>)` |
| `operator_join` | `rt.text(<a>) + rt.text(<b>)` |
| `operator_length` | `len(rt.text(<value>))` |
| `operator_mod` | `rt.scratch_mod(<a>, <b>)` |
| `operator_round` | `rt.scratch_round(<value>)` |
| `operator_math` | `rt.math_operation(<operation>, <value>)` |

这里不直接使用 Python 的隐式类型转换、`round()` 和 `%` 作为完整实现，因为负数、文本转数字、除零和 `.5` 舍入行为必须与 EV3/Scratch 兼容。

### 17.9 变量和列表：9块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `data_variable` | `var_ID` |
| `data_set_variable` | `var_ID = <value>` |
| `data_change_variable` | `var_ID = rt.number(var_ID) + rt.number(<value>)` |
| `data_list` | `list_ID` |
| `data_list_add` | `list_ID.append(<item>)` |
| `data_list_delete_all` | `list_ID.clear()` |
| `data_list_replace` | `rt.list_replace(list_ID, <one_based_index>, <item>)` |
| `data_list_item` | `rt.list_item(list_ID, <one_based_index>)` |
| `data_list_length` | `len(list_ID)` |

`rt.list_item()` 越界返回空字符串，`rt.list_replace()` 越界不执行，以保持已确认的 EV3 列表语义。变量和列表的 Python 标识符必须从内部 ID 生成并处理关键字冲突。

### 17.10 我的模块：2块

| 稳定 ID | MicroPython 生成模板 |
|---|---|
| `procedure_definition` | `def proc_ID(<number_or_text_args>, <boolean_args>): <body>` |
| `procedure_call` | `proc_ID(<arguments>)` |

固定标签只影响积木显示，不生成 Python 参数。模块没有返回值；调用前由生成器按依赖顺序输出定义。

### 17.11 `est_runtime` 最小能力清单

为支持以上全部映射，MicroPython 固件或用户程序资源中至少需要以下兼容能力：

- 电机和移动预设状态、端口对象缓存、单位换算和等待完成；
- 颜色、触摸、超声波、红外和陀螺仪对象缓存；
- 事件注册、采样、边沿检测、广播、程序堆调度和停止作用域；
- Scratch 数字、文本、布尔、比较、除法、余数、舍入和随机数语义；
- 1基列表索引及越界行为；
- 计时器、可让出调度器的等待和全局安全停机；
- 资源 ID 到设备声音/图片的解析。

这些 helper 必须有主机侧单元测试。上位机实时 VM 可以使用 TypeScript 实现同名语义，但必须复用同一组输入输出测试向量。

## 18. 建议的首批兼容测试

1. 电机：预设速度、显式速度和功率三种运行方式不能混淆；
2. 电机：圈、度、秒三种单位与正负方向一致；
3. 移动：实时模式与生成模式对同一转向值产生相同左右轮目标；
4. 显示：行文字、坐标文字、图片和清屏结果一致；
5. 声音：等待播放与后台播放的程序顺序不同；
6. 事件：条件持续为真时只按边沿触发，不重复创建程序堆；
7. 广播：普通广播不等待，“广播并等待”必须等待接收程序结束；
8. 传感器：颜色、距离、红外和按钮的枚举与单位转换一致；
9. 列表：1基索引、越界读取和越界替换行为一致；
10. 停止：所有停止选项、Python异常、USB断开和窗口关闭均能安全停机。

## 19. 尚待冻结的问题

1. 无端口颜色校准块在多颜色传感器场景中的目标选择规则；
2. EST 红/蓝状态灯对 EV3 绿/红/橙及闪烁模式的映射；
3. EV3 内置图片、声音资源清单及 EST 资源包格式；
4. 运算候选3块和通用声音候选8块是否存在可达界面或历史项目兼容需求；
5. 事件调度器的最大并发程序堆、队列长度和重入规则。
