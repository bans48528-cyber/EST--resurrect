# 当前权威硬件资料

## 文件

- `PCB_MainControlV5.0.sch`：Altium/Protel 二进制原理图源文件。
- `PCB_MainControlV30.pdf`：同一设计的 5 页可视原理图。
- `EST_MainControl_V5_pin_map.csv`：按 144 脚封装逐脚整理的当前引脚表，也是自动冲突检查的数据源。

便于人工筛选和查看的 Excel 版本位于 `outputs/est_pin_map_20260822/EST_MainControl_V5_pin_map.xlsx`。

运行自动检查：

```powershell
python firmware\minimal_upgrade_app\tools\check_hardware_pins.py --repo-root .
```

## 完整性

| 文件 | SHA-256 |
| --- | --- |
| `PCB_MainControlV5.0.sch` | `8634622811dfb1aca44f2e901b0c36a8f70829d588d646ddbaa543b26736c953` |
| `PCB_MainControlV30.pdf` | `dcea253cd2aef315e570e2b66a020b4ba0588291334bcb2711699a5d51436079` |

2026-08-22 已核对 SCH、PDF 和实物。LCD 写屏引脚为 `PD14/PG2/PD15`，`M0.25A` 已显示版本号且 USB 心跳正常。`PB13` 是 `ULPI_D6`，不得用作 LCD 时钟。
