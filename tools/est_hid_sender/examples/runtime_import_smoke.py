import est_runtime as rt


@rt.on_start
def stack_1():
    rt.sleep(0.5)
    rt.wait_until(lambda: rt.compare(rt.timer_seconds(), "greater", 0.4))


rt.run()
