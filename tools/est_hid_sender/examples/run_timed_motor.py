import est

motor = est.Motor("A")
assert motor.type() == motor.TYPE_LARGE

motor.reset_angle()
started = est.millis()
next_gc = started
gc_count = 0
motor.run_time(duration_ms=1200, speed=30)
while motor.state() == motor.STATE_TIMED:
    now = est.millis()
    if now >= next_gc:
        assert est.force_gc() > 0
        gc_count += 1
        next_gc += 100
forward_elapsed = est.millis() - started
forward_angle = motor.angle()
assert 1150 <= forward_elapsed <= 1300
assert forward_angle > 30
assert motor.state() == motor.STATE_IDLE
assert motor.stop_mode() == motor.STOP_COAST
assert motor.power() == 0

started = est.millis()
motor.run_time(duration_ms=1000, speed=-30, stop=motor.STOP_BRAKE)
while motor.state() == motor.STATE_TIMED:
    pass
reverse_elapsed = est.millis() - started
assert 950 <= reverse_elapsed <= 1100
assert motor.angle() < forward_angle
assert motor.state() == motor.STATE_IDLE
assert motor.stop_mode() == motor.STOP_BRAKE
assert motor.power() == 0

for duration, speed in ((0, 30), (100, 0)):
    try:
        motor.run_time(duration_ms=duration, speed=speed)
        assert False
    except ValueError:
        pass

try:
    motor.run_time(100, stop=2)
    assert False
except RuntimeError:
    pass

try:
    est.Motor("B").run_time(100, speed=30)
    assert False
except RuntimeError:
    pass

motor.stop()
est._program_result(
    forward_elapsed * 10000 + reverse_elapsed + gc_count
)
