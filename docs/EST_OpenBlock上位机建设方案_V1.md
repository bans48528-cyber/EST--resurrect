# EST OpenBlock 上位机建设方案 V1

日期：2026-08-26

## 1. 建设目标

以 OpenBlock 桌面端为基础，构建面向 EST 主控的 Windows 上位机，最终形成一套可以完成以下闭环的软件：

1. 拖拽积木编写 EST 程序；
2. 将积木保存为 OpenBlock 项目文件；
3. 在设备实时模式下通过 USB HID 读取状态、控制马达和传感器；
4. 在上传模式下生成目标程序并下载到 EST；
5. 在同一软件中完成设备识别、固件升级、诊断和日志查看。

本项目不以恢复旧版 EST 上位机界面为目标。旧资料用于协议、硬件行为和资源校验；新的用户界面、设备模型和升级流程均建立在可维护的开源工程上。

## 2. 推荐技术路线

采用 OpenBlock 官方桌面端的 Electron 架构，保留其积木编辑能力，并增加 EST 专用设备扩展：

```text
OpenBlock GUI
    │  积木、项目、舞台、语言包
    ▼
EST Blocks / EST VM Extension
    │  EST 积木定义、运行时操作、上传代码生成
    ▼
EST Device Service
    │  设备发现、USB HID、帧组包、状态缓存、错误转换
    ├── 实时控制：0x01、0x0D、0x17～0x21
    └── 固件升级：复用现有 est_hid_sender 的 APP=、分包、ACK、manifest 校验
    ▼
EST 主控 M0.88A 及后续固件
```

推荐从 `openblock-desktop` 派生桌面壳，从 `openblock-gui` 派生界面，从 `openblock-vm` 派生运行时扩展；EST 协议和升级逻辑单独放在 `packages/est-device`，不散落到 GUI 组件中。

## 3. 软件组成

### 3.1 桌面壳

- Electron 主进程负责窗口、文件、升级包选择、日志目录和应用打包；
- renderer 进程负责 OpenBlock GUI 和 EST 专用面板；
- 设备服务通过明确的 IPC API 暴露给 renderer，避免 UI 直接访问 HID；
- Windows 第一目标，后续再评估 macOS/Linux。

### 3.2 EST 设备服务

建议提供以下稳定接口：

```ts
interface EstDeviceService {
  listDevices(): Promise<EstDeviceInfo[]>;
  connect(deviceId: string): Promise<EstDeviceInfo>;
  disconnect(deviceId: string): Promise<void>;
  readSnapshot(deviceId: string): Promise<EstSnapshot>;
  motorPower(deviceId: string, port: number, power: number, stop: StopMode): Promise<void>;
  motorPosition(deviceId: string, port: number, degrees: number, speed: number): Promise<void>;
  motorSpeed(deviceId: string, port: number, speed: number): Promise<void>;
  readSensor(deviceId: string, port: number, mode: number): Promise<EstSensorValue>;
  stopAll(deviceId: string): Promise<void>;
  upgrade(deviceId: string, packagePath: string): Promise<UpgradeProgress>;
}
```

设备服务内部必须保留命令互斥、请求超时、设备断开、忙状态和升级期间禁止普通控制等保护。renderer 只接收结构化状态，不解析原始 HID 字节。

### 3.3 EST 积木扩展

第一版积木按“能完成真实机器人程序”而不是按底层命令逐条暴露：

- 事件：程序开始、按键按下、设备连接/断开；
- 运动：A-D 单电机功率、停止、按角度转动、按圈数转动、定速；
- 底盘：双电机直行、转向值混控、左右独立速度；原地旋转是转向值 `±100` 的结果，不单独引入轮径/轮距/机身角度积木；
- 传感器：触碰、颜色/灰度、超声波、温度、陀螺仪、红外；
- 主控：LED、LCD、背光、声音、电池、延时；
- 程序控制：等待、重复、条件、变量、消息广播。

第一阶段不把升级协议、Flash 诊断和引脚级 ADC 诊断做成教学积木；它们属于“设备维护”页面。

## 4. 两种运行模式

### 实时模式

积木由 OpenBlock VM 解释执行，每个硬件操作调用 EST Device Service。适合调试、传感器观察和低风险交互。运行期间使用快照轮询刷新状态，默认不直接暴露原始 HID 帧。

### 上传模式

积木经过 EST 专用代码生成器转换为目标程序，再由构建服务编译并通过现有升级链路下载。第一版建议先实现“实时模式 + 固件升级”，上传模式等 EST MicroPython/C API 稳定后再冻结，避免先绑定尚未确定的固件执行模型。

## 5. MVP 范围

第一可用版本只做以下功能：

1. OpenBlock 桌面端可以启动、创建、保存和打开项目；
2. 识别 `VID=0483、PID=5750` 的 EST HID 设备；
3. 显示固件版本、协议版本、电池档位、四路马达和四路输入状态；
4. 提供连接、断开、急停和设备状态刷新；
5. 提供四路马达功率控制、停止、测速读取；
6. 提供已验收传感器的读取积木或实时面板；
7. 提供固件包离线校验和升级进度、失败原因、日志路径；
8. 在设备未连接、设备忙、协议错误和超时情况下给出中文可操作提示。

MVP 暂不承诺：

- 完整恢复旧版所有积木外观和行为；
- 音频播放；
- 一次性实现 C/MicroPython 上传编译；
- 蓝牙支持；
- 兼容所有历史 EST 固件。最低兼容基线保留为 M0.72A / 协议 1.5，当前完整功能目标为 M0.93A / 协议 1.13；新增能力必须按设备功能位启用。M0.90A 定量转向已经完成锂电实机验收；M0.91A-M0.93A 的传感器、界面、电池和系统服务层重构都不改变上位机协议。

## 6. 目录建议

```text
est-openblock-desktop/
├── apps/desktop/              # Electron 主进程和 renderer 入口
├── packages/est-device/       # HID、协议、状态机、升级、日志
├── packages/est-blocks/       # EST 积木定义和中文文案
├── packages/est-vm/           # 实时模式 runtime extension
├── packages/est-codegen/      # 后续上传模式代码生成
├── packages/est-ui/           # 设备面板、诊断面板、升级面板
├── firmware/                  # 构建产物索引，不复制全部固件源码
├── tests/                     # 协议、设备模拟器、端到端测试
└── docs/
```

当前仓库中的 Python `tools/est_hid_sender` 不直接搬进 renderer。建议先将其协议规则以测试向量形式固定，再用 TypeScript 重写设备服务；在 TypeScript 版本通过同一组向量前，不删除 Python 工具。

## 7. 分阶段实施

### 阶段 A：设备服务原型

- 固定 TypeScript 协议类型和 EST 设备状态模型；
- 实现 HID 发现、打开、读写、1024 字节 report 拆装；
- 迁移 `ping`、`device-status`、马达功率和急停；
- 用模拟 HID 测试替代实物依赖；
- 继续保留 Python 工具作为升级救援工具。

### 阶段 B：OpenBlock 接入

- 引入 EST 设备菜单和连接状态；
- 加入 EST 积木分类；
- 将马达、按键和传感器积木接入实时模式；
- 保存/打开项目并验证断开重连。

### 阶段 C：维护中心

- 固件版本和 manifest 检查；
- 升级进度、ACK 统计和日志查看；
- 设备诊断、马达类型刷新和传感器模式配置；
- 急停、升级前强制停机、升级后自动重新枚举。

### 阶段 D：上传模式

- 先冻结 `est_*` C API 和 MicroPython API；
- 决定固件端执行模型；
- 实现代码生成、构建、包签名/manifest 和下载；
- 用同一套积木样例对比实时模式和上传模式结果。

## 8. 首批验收标准

### 设备通信

- Windows 插入 EST 后 3 秒内显示设备和固件版本；
- 连续读取快照 10 分钟无未处理异常；
- 设备断开后 UI 在 1 秒内显示断开，重连后能恢复；
- 任何急停路径都停止 A-D 输出；
- 设备忙时不并发发送互斥命令。

### 积木运行

- “开始 → A 口正转 30% → 等待 1 秒 → 停止”能稳定执行；
- 角度/圈数控制使用协议 `0x1B`，不由上位机自行模拟闭环；
- 传感器读数能进入条件和变量；
- 程序停止、窗口关闭和设备断开均触发安全停机。

### 固件升级

- 继续兼容 `APP=` 包头、当前 1024 字节 HID report、逐包 ACK 和 manifest；
- 升级包错误时在发送前拒绝；
- 升级中禁止普通控制；
- 260/260 帧升级完成后能等待设备重新枚举并显示新版本；
- 失败日志至少包含设备、目标版本、帧序号、错误类型和时间。

## 9. 当前应立即冻结的决定

1. MVP 只支持 Windows 和 USB HID；
2. M0.72A / 协议 1.5 是最低兼容基线，M0.93A / 协议 1.13 是当前完整功能目标；
3. 实时模式先于上传模式；
4. 上位机只依赖公开 EST 应用协议，不依赖 Bootloader 内部搬运细节；
5. 设备服务拥有唯一的 HID 访问权，GUI 和 VM 不直接打开 HID；
6. 所有危险动作必须有统一的 `stopAll()` 和窗口关闭清理路径。

## 10. 参考资料

- `docs/EST_USB应用协议_V1.md`
- `docs/EST_C_API与MicroPython下一步工作清单.md`
- `docs/EST基础硬件验收清单_2026-08-25.md`
- `tools/est_hid_sender/`
- OpenBlock 官方仓库：`openblockcc/openblock-desktop`、`openblockcc/openblock-gui`、`openblockcc/openblock-vm`
