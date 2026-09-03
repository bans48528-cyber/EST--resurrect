import est_runtime as rt


@rt.on_start
def stack_1():
    rt.drive_set_pair("B", "C")
    rt.line_follow_init()

    rt.line_follow_dual_power_step(0, 0, 60, 60, 0, 0)
    rt.sleep(0.8)
    rt.line_follow_dual_power_step(10, 0, 60, 60, 0, 0)
    rt.sleep(0.8)
    rt.line_follow_dual_power_step(25, 0, 60, 60, 0, 0)
    rt.sleep(0.8)
    rt.line_follow_dual_power_step(16, 0, 60, 60, 0, 0)
    rt.sleep(0.8)
    rt.line_follow_dual_power_step(5, 0, 60, 60, 0, 0)
    rt.sleep(0.8)
    rt.drive_stop()


rt.run()
