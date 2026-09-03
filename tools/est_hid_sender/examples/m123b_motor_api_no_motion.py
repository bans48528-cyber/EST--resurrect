"""Exercise runtime paths with a fake motor; never apply physical motor power."""
import est
import est_runtime as rt


class FakeMotor:
    STATE_TIMED = 1
    STATE_POSITION = 2
    STATE_FAULT = 3

    def run_speed(self, value):
        pass

    def run_power(self, value):
        pass

    def stop(self, mode):
        pass

    def run_time(self, duration, speed=0, stop=0):
        pass

    def run_angle(self, angle, speed=0, stop=0):
        pass

    def state(self):
        return 0


rt._motors["A"] = FakeMotor()
step = 1
try:
    rt.motor_set_speed("A", 50)
    step = 2
    rt.motor_start("A", "clockwise")
    step = 3
    rt.motor_start_speed("A", 75)
    step = 4
    rt.motor_start_power("A", 30)
    step = 5
    rt.motor_stop("A")
    step = 6
    rt.motor_run_for("A", "clockwise", 1, "seconds")
    step = 7
    rt.motor_run_for("A", "clockwise", 1, "rotations")
    est._program_result(12310)
except Exception as error:
    est._program_result(-step)
    est.display.clear()
    est.display.text_line(1, "API CHECK " + str(step))
    est.display.text_line(3, str(error))
    rt.sleep(2)

