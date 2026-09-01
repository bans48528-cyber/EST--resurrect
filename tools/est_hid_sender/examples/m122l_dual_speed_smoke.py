import est_runtime as rt


@rt.on_start
async def stack_1():
    rt.drive_set_pair("B", "C")
    rt.drive_set_stop_action("brake")

    rt.display_text_line(1, "1 Continuous 50/25")
    rt.drive_start_dual_speed(50, 25)
    await rt.sleep(2)
    rt.drive_start_dual_speed(-40, -20)
    await rt.sleep(2)
    rt.drive_stop()
    await rt.sleep(1)

    rt.display_text_line(1, "2 Timed 50/-30")
    await rt.drive_dual_speed_for(50, -30, 2, "seconds")
    await rt.sleep(1)

    rt.display_text_line(1, "3 Rotation 50/25")
    await rt.drive_dual_speed_for(50, 25, 1, "rotations")
    await rt.sleep(1)

    rt.display_text_line(1, "4 Left zero")
    await rt.drive_dual_speed_for(0, 50, 0.5, "rotations")
    await rt.sleep(1)

    rt.display_text_line(1, "Dual speed OK")


rt.run()
