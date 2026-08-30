import est

expected_types = (
    est.Sensor.TYPE_INFRARED,
    est.Sensor.TYPE_SOUND,
    est.Sensor.TYPE_GYRO,
    est.Sensor.TYPE_ULTRASONIC,
)
for port in range(1, 5):
    sensor = est.Sensor(port)
    assert sensor.port() == port
    assert sensor.type() == expected_types[port - 1]
    assert sensor.state() == est.Sensor.STATE_STREAMING
    assert sensor.valid()
    sensor.value()
    assert len(sensor.status()) == 7

assert est.battery.valid()
assert 0 <= est.battery.percent() <= 100
assert 0 <= est.battery.level() <= est.battery.LEVEL_MAX
assert est.battery.low() in (True, False)
assert len(est.battery.status()) == 7

button_mask = est.buttons.value()
assert 0 <= button_mask <= 63
for button in (
    est.buttons.BACK,
    est.buttons.LEFT,
    est.buttons.UP,
    est.buttons.DOWN,
    est.buttons.RIGHT,
    est.buttons.CONFIRM,
):
    assert est.buttons.pressed(button) == ((button_mask & button) != 0)

try:
    est.Sensor(0)
    assert False
except ValueError:
    pass

est._program_result(6)
