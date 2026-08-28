import est

motor = est.Motor("A")
motor.run_time(5000, speed=30, stop=motor.STOP_BRAKE)
started = est.millis()
while est.millis() - started < 400:
    assert motor.state() == motor.STATE_TIMED
raise RuntimeError("timed motor cleanup test")
