import est_runtime as rt


@rt.on_start
def stack_1():
    rt.sleep(12)


rt.run()
