import est

motor = est.Motor("A")
motor.run_power(20)
started = est.millis()
while est.millis() - started < 400:
    assert motor.power() == 20
raise RuntimeError("motor cleanup test")
