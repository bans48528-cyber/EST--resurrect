import est_runtime as rt
import est


@rt.on_start
async def stack_1():
    await rt.wait_until(
        lambda: rt.compare(rt.color("1").reflection(), "less", 50)
    )
    est.display.image("Eyes/Neutral")
    est.display.refresh()
    await rt.sleep(1)


rt.run()
