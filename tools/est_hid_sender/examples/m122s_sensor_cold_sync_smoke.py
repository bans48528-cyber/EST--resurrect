import est
import est_runtime as rt


@rt.on_start
async def stack_1():
    for cycle in range(3):
        est._program_result(cycle * 10 + 1)
        est.Sensor(1).restart()
        sensor = est.ColorSensor(1)
        est._program_result(cycle * 10 + 2)
        sensor.reflection()
        est._program_result(cycle * 10 + 3)
        await rt.sleep(0.1)
    est._program_result(99)


rt.run()
