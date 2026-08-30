import est
import est_runtime as rt


progress = [0, 0, 0]


@rt.on_start
async def stopped_stack():
    progress[0] = 1
    rt.stop("this_stack")
    progress[0] = 99


@rt.on_start
async def peer_stack():
    progress[1] = 1
    await rt.yield_once()
    progress[1] = 2


@rt.on_start
async def controller():
    await rt.yield_once()
    rt.stop_other_stacks()
    progress[2] = 3


rt.run()
est._program_result(progress[0] * 100 + progress[1] * 10 + progress[2])
