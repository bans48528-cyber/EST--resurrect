import est
import est_runtime as rt


@rt.on_start
async def stack_1():
    sensor = rt.color(1)
    for cycle in range(10):
        est._program_result(cycle * 3 + 1)
        sensor.reflection()
        await rt.sleep(0.1)
        est._program_result(cycle * 3 + 2)
        sensor.ambient()
        await rt.sleep(0.1)
        est._program_result(cycle * 3 + 3)
        sensor.color()
        await rt.sleep(0.1)
    est._program_result(99)


rt.run()
