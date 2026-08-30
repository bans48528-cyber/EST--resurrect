import est
import est_runtime as rt


PORT = "A"
observed = [False]


def show(line1, line2):
    est.display.clear()
    est.display.text(4, 8, line1)
    est.display.text(4, 24, line2)
    est.display.refresh()


@rt.on_start
async def run_position_task():
    show("STALL TEST A", "Starts in 5 sec")
    await rt.sleep(5)
    show("HOLD MOTOR A", "Waiting stall")
    await rt.motor_run_for(PORT, "clockwise", 10, "rotations", speed=25)


@rt.on_start
async def monitor_stall():
    initial_clear = not rt.motor_stalled(PORT)
    await rt.sleep(5)
    started_ms = est.millis()
    while not rt.motor_stalled(PORT):
        if ((est.millis() - started_ms) & 0xFFFFFFFF) >= 12000:
            rt.motor_stop(PORT)
            est._program_result(-1)
            return
        await rt.sleep(0.05)

    observed[0] = True
    show("STALL DETECTED", "Stopping safely")
    rt.motor_stop(PORT)
    await rt.sleep(0.1)
    cleared_after_stop = not rt.motor_stalled(PORT)
    result = (1 if initial_clear else 0)
    result += 10 if observed[0] else 0
    result += 100 if cleared_after_stop else 0
    est._program_result(result)


rt.run()
