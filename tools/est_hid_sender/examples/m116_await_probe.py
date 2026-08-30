import est
import est_runtime as rt


est._program_result(10)


@rt.on_start
async def task():
    est._program_result(20)
    await rt.yield_once()
    est._program_result(30)


rt.run()
est._program_result(40)
