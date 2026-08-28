import est

motor = est.Motor("D")
assert motor.type() == motor.TYPE_MEDIUM

motor.reset_angle()
motor.run_speed(20)
started = est.millis()
maximum_speed = 0
while est.millis() - started < 800:
    speed = motor.speed()
    if speed > maximum_speed:
        maximum_speed = speed
motor.stop()
assert maximum_speed >= 10

before = motor.angle()
motor.run_angle(degrees=-90, speed=30)
started = est.millis()
while motor.state() == motor.STATE_POSITION:
    assert est.millis() - started < 5000
after = motor.angle()
assert -110 <= after - before <= -75
assert motor.state() == motor.STATE_IDLE
assert motor.power() == 0

est._program_result(maximum_speed * 1000 + before - after)
