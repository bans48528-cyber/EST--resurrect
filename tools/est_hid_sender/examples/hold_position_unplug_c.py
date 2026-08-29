import est
import est_runtime as rt


motor = est.Motor("C")

motor.run_time(1200, speed=30, stop=motor.STOP_HOLD)
while motor.state() == motor.STATE_TIMED:
    rt.yield_once()
est.display.clear()
est.display.text(12, 48, "C HOLD active")
est.display.text(12, 64, "unplug motor C")
est.display.refresh()
rt.sleep(15)
