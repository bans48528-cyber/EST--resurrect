import est
import est_runtime as rt


condition = [False]
condition_done = [0]
timer_count = [0]
heartbeat = [0]
cooperative_checks = [0]


@rt.on_start
async def heartbeat_stack():
    while True:
        heartbeat[0] += 1
        await rt.sleep(0.01)


@rt.on_start
async def producer_stack():
    await rt.sleep(0.10)
    condition[0] = True
    await rt.sleep(0.15)
    condition[0] = False
    await rt.sleep(0.05)
    condition[0] = True

    await rt.wait_until(
        lambda: condition_done[0] == 2 and timer_count[0] == 2
    )
    result = 1000 + condition_done[0] * 100
    result += timer_count[0] * 10 + cooperative_checks[0]
    est._program_result(result)
    rt.stop("all")


@rt.on_condition(lambda: condition[0])
async def condition_stack():
    before = heartbeat[0]
    await rt.sleep(0.04)
    if heartbeat[0] > before:
        cooperative_checks[0] += 1
    condition_done[0] += 1


@rt.on_timer_gt(0.12)
async def timer_stack():
    timer_count[0] += 1
    if timer_count[0] == 1:
        rt.reset_timer()


rt.run()
