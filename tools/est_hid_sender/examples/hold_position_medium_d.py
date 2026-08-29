import est
import est_runtime as rt


motor = est.Motor("D")
motor.run_speed(20)
rt.sleep(1)
motor.stop(motor.STOP_HOLD)

est.display.clear()
est.display.text(8, 20, "D MEDIUM HOLD")
est.display.text(8, 40, "PUSH SHAFT NOW")
est.display.refresh()
rt.sleep(15)
motor.stop(motor.STOP_COAST)
