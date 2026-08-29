import est_runtime as rt


@rt.on_start
def stack_1():
    for speed in (1, 5, 9):
        rt.motor_set_speed("A", speed)
        rt.motor_start("A", "clockwise")
        rt.sleep(2)
    rt.motor_stop("A")


rt.run()
