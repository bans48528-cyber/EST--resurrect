import est
import est_runtime as rt


@rt.on_timer_gt(0.08)
async def timer_stack():
    est._program_result(1171)
    rt.stop("all")


rt.run()
