# EST frozen est_runtime M1.10A 验收

## 第一版程序闭环基线（2026-08-29，已通过）

本节记录已经完成的真机结果，作为后续恢复开发时必须优先采用的第一版程序闭环基线。当前阶段暂停新增功能。

| 基线项 | 已确认结果 |
| --- | --- |
| 固件代码提交 | `2d27f96`（`feat: freeze initial est runtime module`） |
| 固件版本 | `M1.10A` |
| USB 应用协议 | `1.20` |
| 设备能力 | 包含 `frozen-est-runtime` |
| 升级包 | `firmware/releases/M1.10A/est_minimal_upgrade_app_M1.10A.upgrade.bin` |
| 升级包 SHA-256 | `835ba5342c5b91737f29e36fcd0d2c6af4afb998e8475cbc86ed93c86f16044e` |
| MicroPython 自检 | `state=passed`，`self_test_value=96` |

命令行真机冒烟结果：

- `runtime_import_smoke.py`：`completed`，源码 `158/158` 字节，期望与实际 CRC32 均为 `fab868ce`，运行 `506 ms`，没有 import 或 Python exception。
- `runtime_stop_smoke.py`：先确认 `state=running`，再主动停止；最终 `state=stopped`、`error=stopped`、`flags=0x05`，源码 `106/106` 字节，期望与实际 CRC32 均为 `8b99c9de`，运行 `1232 ms`。停止后 A-D 均为 `coast`、功率 `0`。

### EST Studio 第一轮：纯 `import est`

| 项目 | 验收结果 |
| --- | --- |
| 目标槽位 | 7 |
| 程序类型 | 不依赖 `est_runtime` 的最小 Python 程序 |
| 下载、保存、加载、运行 | 全部通过 |
| 停止 | 通过，无错误弹窗 |
| `0x24` 最终状态 | `state=stopped`，`error=stopped`，`flags=0x05` |
| RAM 程序校验 | `received=37/37`，期望与实际 CRC32 均为 `48a0b72f` |
| 运行记录 | `run_count=1`，`duration_ms=4955`，`timeout_ms=10000` |
| `0x25` 槽位状态 | `state=saved`，`source_length=37`，`source_crc32=48a0b72f`，`last_error=none` |

### EST Studio 第二轮：积木生成 `import est_runtime as rt`

使用“当程序启动时 -> 重复执行 -> 等待 1 秒”积木程序；代码区确认生成 `import est_runtime as rt` 并通过 `rt.run()` 启动。

| 项目 | 验收结果 |
| --- | --- |
| 目标槽位 | 6 |
| 程序类型 | EST Studio 真实积木生成的 `est_runtime` 程序 |
| 下载、保存、加载、运行 | 全部通过 |
| 停止 | 通过，无错误弹窗 |
| `0x24` 最终状态 | `state=stopped`，`error=stopped`，`flags=0x05` |
| RAM 程序校验 | `received=115/115`，期望与实际 CRC32 均为 `50137350` |
| 运行记录 | `run_count=1`，`duration_ms=7077`，`timeout_ms=10000` |
| `0x25` 槽位状态 | `state=saved`，`source_length=115`，`source_crc32=50137350`，`last_error=none` |

两轮均未记录 EST 操作失败或设备断开错误。至此，当前 EST Studio 与 M1.10A 已完成 Python 源码下载、持久化槽位保存、加载运行和主动停止的第一版真机闭环；纯 `import est` 与积木生成的 `import est_runtime as rt` 两条路径都列入基线。

## 阶段结论

- M1.10A 将 `micropython_port/modules/est_runtime.py` 编译为 frozen MPY，用户程序可继续使用 `import est_runtime as rt`。
- 开机 MicroPython 自检会导入 `est_runtime` 并检查 `API_VERSION == 1`。
- USB 应用协议为 `1.20`，设备能力新增 `frozen-est-runtime`，供 EST Studio 判断固件是否兼容生成代码。
- 固件启用 MPZ 长整数和单精度浮点，支持 `0.5` 秒等小数输入；不启用复数。
- M1.10A 发布包 SHA-256：`835ba5342c5b91737f29e36fcd0d2c6af4afb998e8475cbc86ed93c86f16044e`。

## 第一阶段接口

已实现：

- 生命周期：`on_start`、`run`。
- 控制：`sleep`、`yield_once`、`wait_until`、`repeat_count`、`boolean`。
- 时间和比较：`seconds_to_ms`、`timer_seconds`、`reset_timer`、`compare` 的 less/greater/equal。
- 单马达：缓存、速度设置、启动、停止、按圈/度/秒运行。
- 底盘：配对、速度设置、直行、转向、持续转向、停止。
- 传感器：触碰、颜色、陀螺仪、超声波、红外接近度，以及红外原始 beacon/remote 读取。

明确未实现并抛 `NotImplementedError`：

- 除 `on_start` 外的事件帽、广播和程序堆停止调度。
- 传感器事件等待、颜色校准和红外信标通道/按钮包装。
- `changed` 比较、图片定时显示、底盘左右独立速度包装。
- `hold` 位置保持；当前可用停止语义为 `float/coast`，底层还支持 `brake`，但 EST Studio 当前未生成 brake 选项。

`run()` 第一阶段按注册顺序执行启动程序堆，尚未提供并发事件调度。有限运动会等待完成；持续运动若程序直接结束，会被固件的脚本退出清理停止。

## 实机验收

### 1. 升级和能力声明

将设备切到固件升级界面，在仓库根目录执行：

```powershell
python tools/est_hid_sender/est_hid_sender.py flash `
  --file firmware/releases/M1.10A/est_minimal_upgrade_app_M1.10A.upgrade.bin
```

发送完成后正常开机，再执行：

```powershell
python tools/est_hid_sender/est_hid_sender.py device-status
python tools/est_hid_sender/est_hid_sender.py micropython-status
```

通过标准：版本为 `M1.10A`，协议为 `1.20`，能力列表包含 `frozen-est-runtime`；MicroPython 状态为 `passed`，自检值为 `96`。

### 2. 保留纯 est 基线

先确认未依赖 runtime 的旧程序仍能下载和运行：

```powershell
python tools/est_hid_sender/est_hid_sender.py python-run `
  --file tools/est_hid_sender/examples/compute_result.py --timeout-ms 2000
python tools/est_hid_sender/est_hid_sender.py python-program-status
```

通过标准：状态为 `completed`，结果值为 `85344`。

### 3. frozen runtime 导入、时间和启动程序堆

```powershell
python tools/est_hid_sender/est_hid_sender.py python-run `
  --file tools/est_hid_sender/examples/runtime_import_smoke.py --timeout-ms 3000
python tools/est_hid_sender/est_hid_sender.py python-program-status
```

通过标准：约 0.5 秒后状态为 `completed`，不得出现 import 或 Python exception。

### 4. EST Studio 生成程序

在 EST Studio 新建一个程序：程序启动时，等待 `0.5` 秒，将 A 口速度设为 `30%`，顺时针运行 `1` 圈。确认 A 口接入已识别马达且轴可自由转动。

生成源码至少应包含：

```python
import est_runtime as rt

@rt.on_start
def stack_1():
    rt.sleep(0.5)
    rt.motor_set_speed("A", 30)
    rt.motor_run_for("A", "clockwise", 1, "rotations")

rt.run()
```

用 EST Studio 下载并运行。通过标准：不再提示缺少 frozen runtime；等待后马达完成约一圈，程序状态为 completed，马达最终停止。

### 5. 主动停止

```powershell
python tools/est_hid_sender/est_hid_sender.py python-run `
  --file tools/est_hid_sender/examples/runtime_stop_smoke.py --timeout-ms 10000
python tools/est_hid_sender/est_hid_sender.py python-stop
python tools/est_hid_sender/est_hid_sender.py python-program-status
```

通过标准：停止命令可在超时前生效，状态为 `stopped`，所有马达保持停止。

### 6. 未实现接口应明确失败

在 EST Studio 生成一个非“程序启动时”的事件帽，或使用“保持位置”停止动作。下载运行后应进入 Python exception，不能静默完成。第二阶段实现相应功能后再把这一项改为功能验收。
