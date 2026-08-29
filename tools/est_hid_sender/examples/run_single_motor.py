import est

motor = est.Motor("A")
assert motor.type() == motor.TYPE_LARGE

motor.reset_angle()
motor.run_power(20)
started = est.millis()
while est.millis() - started < 400:
    assert motor.state() == motor.STATE_POWER
    assert motor.power() == 20
motor.stop()
assert motor.state() == motor.STATE_IDLE
assert motor.power() == 0

motor.run_speed(20)
maximum_speed = 0
maximum_gc_pause = 0
for _ in range(6):
    pause = est.force_gc()
    if pause > maximum_gc_pause:
        maximum_gc_pause = pause
    started = est.millis()
    while est.millis() - started < 200:
        speed = motor.speed()
        if speed > maximum_speed:
            maximum_speed = speed
motor.stop()
assert maximum_speed >= 10
assert maximum_gc_pause > 0
assert motor.state() == motor.STATE_IDLE

before = motor.angle()
motor.run_angle(degrees=90, speed=30)
started = est.millis()
while motor.state() == motor.STATE_POSITION:
    assert est.millis() - started < 5000
after = motor.angle()
assert 75 <= after - before <= 110
assert motor.state() == motor.STATE_IDLE
assert motor.power() == 0

motor.run_speed(0)
assert motor.state() == motor.STATE_SPEED
assert motor.target_speed() == 0
assert motor.power() == 0
motor.stop()

try:
    est.Motor("B").run_speed(20)
    assert False
except RuntimeError:
    pass

try:
    motor.run_angle(90, stop=motor.STOP_BRAKE)
    assert False
except RuntimeError:
    pass

est._program_result(maximum_speed * 1000 + after - before)
