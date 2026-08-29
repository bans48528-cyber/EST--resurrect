import est_runtime as rt


@rt.on_start
def stack_1():
    rt.motor_set_speed("A", 20)
    rt.motor_start("A", "clockwise")
    rt.sleep(2)
    rt.motor_set_speed("A", 75)
    rt.motor_start("A", "clockwise")
    rt.sleep(2)
    rt.motor_stop("A")


rt.run()
