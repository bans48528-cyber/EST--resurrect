import est
import est_runtime as rt


progress = [0, 0]


@rt.on_start
async def stack_a():
    for _ in range(5):
        progress[0] += 1
        await rt.sleep(0.1)


@rt.on_start
async def stack_b():
    for _ in range(7):
        progress[1] += 1
        await rt.yield_once()


@rt.on_start
async def observer():
    await rt.wait_until(lambda: progress[0] == 5 and progress[1] == 7)


rt.run()
est._program_result(progress[0] * 100 + progress[1])
