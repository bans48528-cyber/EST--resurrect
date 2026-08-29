import est
import est_runtime as rt


motor = est.Motor("A")
motor.run_speed(30)
rt.sleep(1)
motor.stop(motor.STOP_HOLD)

est.display.clear()
est.display.text(12, 48, "HOLD active")
est.display.text(12, 64, "waiting for STOP")
est.display.refresh()

while True:
    rt.sleep(1)
