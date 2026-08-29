import est


def wait_ms(duration_ms):
    started = est.millis()
    while est.millis() - started < duration_ms:
        pass


motor = est.Motor("A")
pair = est.MotorPair("A", "C")

assert motor.type() == motor.TYPE_LARGE
assert est.Motor("C").type() == motor.TYPE_LARGE

# A running motor must accept 20 -> 0 -> 75 without restarting the program.
motor.run_speed(20)
wait_ms(500)
motor.run_speed(0)
assert motor.state() == motor.STATE_SPEED
assert motor.target_speed() == 0
wait_ms(500)
motor.run_speed(75)
assert motor.state() == motor.STATE_SPEED
assert motor.target_speed() == 75
wait_ms(700)
motor.stop()
est._program_result(110)

# Low closed-loop speeds are accepted as-is instead of being raised to 10.
for speed in (1, 5, 9):
    motor.run_speed(speed)
    assert motor.state() == motor.STATE_SPEED
    assert motor.target_speed() == speed
    wait_ms(300)
    motor.stop()
est._program_result(120)

# A timed zero-speed operation still consumes the complete requested time.
started = est.millis()
motor.run_time(duration_ms=1000, speed=0, stop=motor.STOP_COAST)
assert motor.state() == motor.STATE_TIMED
while motor.state() == motor.STATE_TIMED:
    pass
elapsed = est.millis() - started
assert elapsed >= 1000
est._program_result(130)

# Paired control keeps running when either or both requested speeds are zero.
pair.run_speed(0, 50)
wait_ms(500)
assert pair.running()
pair.run_speed(50, 0)
wait_ms(500)
assert pair.running()
pair.run_speed(0, 0)
wait_ms(500)
assert pair.running()
pair.run_speed(0, 50)
wait_ms(500)
assert pair.running()
pair.run_speed(50, 50)
wait_ms(700)
assert pair.running()
pair.stop(pair.STOP_COAST)
assert not pair.running()
est._program_result(140)

# A nonzero angle at zero speed must wait until the user sends STOP.
motor.run_angle(degrees=90, speed=0, stop=motor.STOP_COAST)
assert motor.state() == motor.STATE_POSITION
wait_ms(700)
assert motor.state() == motor.STATE_POSITION
assert motor.target_speed() == 0
est._program_result(150)
while True:
    assert motor.state() == motor.STATE_POSITION
    pass
