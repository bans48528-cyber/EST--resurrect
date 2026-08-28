import est

motor = est.Motor("A")
motor.run_time(5000, speed=30, stop=motor.STOP_BRAKE)
started = est.millis()
while est.millis() - started < 400:
    assert motor.state() == motor.STATE_TIMED
motor.stop()
assert motor.state() == motor.STATE_IDLE
assert motor.stop_mode() == motor.STOP_COAST
assert motor.power() == 0
est._program_result(est.millis() - started)
