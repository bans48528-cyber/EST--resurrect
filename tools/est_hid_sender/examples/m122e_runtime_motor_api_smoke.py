import est
import est_runtime as rt


PORT = "B"


@rt.on_start
async def old_owner():
    rt.motor_start_speed(PORT, 20)
    await rt.sleep(0.1)
    rt.motor_start_speed(PORT, 0)
    await rt.sleep(0.1)
    rt.motor_start_speed(PORT, 75)
    await rt.sleep(0.6)


@rt.on_start
async def new_owner():
    await rt.sleep(0.4)
    rt.motor_start_power(PORT, 101)
    await rt.sleep(0.7)
    rt.motor_stop(PORT)
    est._program_result(122)


rt.run()
