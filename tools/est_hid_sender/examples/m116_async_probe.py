import est
import est_runtime as rt


est._program_result(1)


@rt.on_start
async def task():
    est._program_result(2)


rt.run()
est._program_result(3)
