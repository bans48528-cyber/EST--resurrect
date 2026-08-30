import est_runtime as rt


@rt.on_start
async def task():
    await rt.yield_once()
    rt.stop("all")


rt.run()
