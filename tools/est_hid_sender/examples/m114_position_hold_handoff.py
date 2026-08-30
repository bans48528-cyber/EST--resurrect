import est
import est_runtime as rt


def show(line1, line2):
    est.display.clear()
    est.display.text(8, 20, line1)
    est.display.text(8, 40, line2)
    est.display.refresh()


large = est.Motor("A")
medium = est.Motor("D")

show("A RUN ANGLE", "THEN PUSH")
large.run_angle(degrees=360, speed=50, stop=large.STOP_HOLD)
show("A HOLD ACTIVE", "PUSH A NOW")
rt.sleep(4)
large.run_angle(degrees=-360, speed=50, stop=large.STOP_BRAKE)
large.stop(large.STOP_COAST)

show("D RUN ANGLE", "THEN PUSH")
medium.run_angle(degrees=360, speed=50, stop=medium.STOP_HOLD)
show("D HOLD ACTIVE", "PUSH D NOW")
rt.sleep(4)
medium.run_angle(degrees=-360, speed=50, stop=medium.STOP_BRAKE)
medium.stop(medium.STOP_COAST)

show("M1.14A HOLD", "TEST COMPLETE")
rt.sleep(2)
