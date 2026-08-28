import est

motor = est.Motor("D")
assert motor.type() == motor.TYPE_MEDIUM

motor.reset_angle()
started = est.millis()
motor.run_time(800, speed=25)
while motor.state() == motor.STATE_TIMED:
    pass
forward_elapsed = est.millis() - started
forward_angle = motor.angle()
assert 750 <= forward_elapsed <= 900
assert forward_angle > 30
assert motor.stop_mode() == motor.STOP_COAST

started = est.millis()
motor.run_time(800, speed=-25, stop=motor.STOP_BRAKE)
while motor.state() == motor.STATE_TIMED:
    pass
reverse_elapsed = est.millis() - started
assert 750 <= reverse_elapsed <= 900
assert motor.angle() < forward_angle
assert motor.stop_mode() == motor.STOP_BRAKE
assert motor.power() == 0

motor.stop()
est._program_result(forward_elapsed * 1000 + reverse_elapsed)
