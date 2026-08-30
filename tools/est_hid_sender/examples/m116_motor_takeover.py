import est_runtime as rt


@rt.on_start
async def old_owner():
    rt.motor_set_speed("A", 20)
    rt.motor_start("A", "clockwise")
    await rt.sleep(0.6)
    rt.stop("this_stack")


@rt.on_start
async def new_owner():
    await rt.sleep(0.2)
    rt.motor_set_speed("A", 75)
    rt.motor_start("A", "clockwise")
    await rt.sleep(1.2)
    rt.motor_stop("A")


rt.run()
