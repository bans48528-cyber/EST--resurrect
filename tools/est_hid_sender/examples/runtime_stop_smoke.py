import est_runtime as rt


@rt.on_start
def stack_1():
    while True:
        rt.yield_once()


rt.run()
