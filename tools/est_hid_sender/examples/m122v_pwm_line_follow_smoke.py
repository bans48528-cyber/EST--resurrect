import est_runtime as rt


@rt.on_start
def stack_1():
    rt.drive_set_pair("B", "C")
    rt.line_follow_init()
    rt.drive_start_dual_power(20, 20)
    rt.sleep(0.3)
    rt.line_follow_dual_power_step(60, 40, 30, 30, 1, 0)
    rt.sleep(0.3)
    rt.line_follow_dual_power_step(40, 60, 30, 30, 1, 0)
    rt.sleep(0.3)
    rt.drive_stop()


rt.run()
