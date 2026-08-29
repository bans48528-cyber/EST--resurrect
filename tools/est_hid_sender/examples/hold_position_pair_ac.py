import est
import est_runtime as rt


pair = est.MotorPair("A", "C")


def show(line1, line2=""):
    est.display.clear()
    est.display.text(12, 48, line1)
    if line2:
        est.display.text(12, 64, line2)
    est.display.refresh()


show("A+C timed HOLD", "wait for finish")
pair.run_time(
    1500,
    left_speed=35,
    right_speed=35,
    stop=pair.STOP_HOLD,
    wait=True,
)
show("Timed HOLD active", "push both motors")
rt.sleep(8)

show("A+C angle HOLD", "one rotation")
pair.run_angle(
    360,
    360,
    speed=35,
    stop=pair.STOP_HOLD,
    wait=True,
)
show("Angle HOLD active", "push both motors")
rt.sleep(8)

show("New command", "takes over HOLD")
pair.run_speed(30, 30)
rt.sleep(2)
pair.stop(pair.STOP_COAST)
show("COAST", "motors released")
rt.sleep(4)
