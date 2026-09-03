import est_runtime as rt


@rt.on_start
async def stack_1():
    await rt.wait_until(lambda: rt.color("1").reflection() < 50)
    await rt.sleep(1)


rt.run()
