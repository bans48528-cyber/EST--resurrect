import est

expected_types = {
    "A": est.Motor.TYPE_LARGE,
    "B": est.Motor.TYPE_NONE,
    "C": est.Motor.TYPE_LARGE,
    "D": est.Motor.TYPE_MEDIUM,
}

for port in ("A", "B", "C", "D"):
    motor = est.Motor(port)
    status = motor.status()
    assert motor.port() == port
    assert len(status) == 8
    assert motor.error() == 0
    assert motor.type() == expected_types[port]
    assert motor.state() == motor.STATE_IDLE
    assert motor.stop_mode() in (motor.STOP_COAST, motor.STOP_BRAKE)
    assert motor.power() == 0
    assert motor.target_speed() == 0
    assert motor.speed() == 0
    assert isinstance(motor.angle(), int)
    motor.stop(motor.STOP_BRAKE)
    assert motor.stop_mode() == motor.STOP_BRAKE
    motor.stop()
    assert motor.stop_mode() == motor.STOP_COAST

try:
    est.Motor("E")
    assert False
except ValueError:
    pass

try:
    est.Motor("A").stop(2)
    assert False
except ValueError:
    pass

est._program_result(4)
