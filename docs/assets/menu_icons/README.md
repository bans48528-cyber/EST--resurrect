# EST Menu Icons

The supplied SVG files are mapped to the main menu as follows:

| Main action | Source artwork | Generated bitmap |
| --- | --- | --- |
| Recent program | `source/quick_run.svg` | `quick_run_32.png` |
| Programs | `source/programs.svg` | `programs_32.png` |
| Ports | `source/ports.svg` | `ports_32.png` |
| Infrared remote | `source/infrared_remote.svg` | `infrared_remote_32.png` |
| Motor control | `source/motor_control.svg` | `motor_control_32.png` |
| Settings | `source/settings.svg` | `settings_32.png` |

Generated icons use a 32 x 32 canvas with a 28 x 28 centered drawing area. Pixels are thresholded to 1-bit monochrome. Each firmware bitmap is row-major, MSB first, with four bytes per row and 128 bytes per icon. This matches `board_lcd_draw_bitmap()`.

- `est_menu_icons_32x32.h` and `.c` currently contain the four implemented firmware arrays.
- The infrared remote and motor control bitmaps are wired into the interactive web preview first. Add them to the firmware tables when those control pages are implemented.
- `menu_icons_preview.png` is a quick visual check of the generated pixels.

Selection highlighting should be drawn by the UI through background inversion; a second copy of each icon is not required.
