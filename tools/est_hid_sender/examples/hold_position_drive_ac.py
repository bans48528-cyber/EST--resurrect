import est
import est_runtime as rt


drive = est.DriveBase("A", "C")


def show(line1, line2=""):
    est.display.clear()
    est.display.text(12, 48, line1)
    if line2:
        est.display.text(12, 64, line2)
    est.display.refresh()


show("DriveBase timed", "then HOLD")
drive.straight_time(
    1500,
    speed=35,
    stop=drive.STOP_HOLD,
    wait=True,
)
show("Drive HOLD active", "push A and C")
rt.sleep(8)

show("Drive takeover", "angle command")
drive.straight_angle(
    360,
    speed=35,
    stop=drive.STOP_HOLD,
    wait=True,
)
show("Angle HOLD active", "push A and C")
rt.sleep(8)

drive.stop(drive.STOP_COAST)
show("Drive COAST", "motors released")
rt.sleep(4)
