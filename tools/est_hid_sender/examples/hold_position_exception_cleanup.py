import est
import est_runtime as rt


motor = est.Motor("A")
motor.run_speed(30)
rt.sleep(1)
motor.stop(motor.STOP_HOLD)
rt.sleep(2)
raise RuntimeError("intentional HOLD cleanup test")
