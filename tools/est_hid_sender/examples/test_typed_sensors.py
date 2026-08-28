import est

sensor_classes = (
    est.SoundSensor,
    est.TemperatureSensor,
    est.TouchSensor,
    est.ColorSensor,
    est.UltrasonicSensor,
    est.GyroSensor,
    est.InfraredSensor,
)
assert len(sensor_classes) == 7

infrared = est.InfraredSensor(1)
assert infrared.port() == 1
est._program_result(10)
proximity = infrared.proximity()
assert 0 <= proximity <= 100
est._program_result(11)
beacon = infrared.beacon()
assert len(beacon) == 2
assert -255 <= beacon[0] <= 255
assert 0 <= beacon[1] <= 255
est._program_result(12)
remote = infrared.remote()
assert 0 <= remote <= 255

sound = est.SoundSensor(2)
est._program_result(20)
assert sound.db() >= 0
try:
    sound.set_mode(1)
    assert False
except RuntimeError:
    pass
est._program_result(21)
sound.restart()
assert sound.db() >= 0

gyro = est.GyroSensor(3)
est._program_result(30)
assert isinstance(gyro.angle(), int)
est._program_result(31)
assert isinstance(gyro.speed(), int)
est._program_result(32)
gyro.reset_angle()
est._program_result(33)
zeroed_angle = gyro.angle()
assert -5 <= zeroed_angle <= 5
est._program_result(34)
assert isinstance(gyro.speed(), int)

ultrasonic = est.UltrasonicSensor(4)
est._program_result(40)
assert ultrasonic.distance_mm() >= 0
est._program_result(41)
assert ultrasonic.inches_tenths() >= 0
est._program_result(42)
assert ultrasonic.presence() in (True, False)

try:
    est.ColorSensor(1)
    assert False
except RuntimeError:
    pass

est._program_result(60)
generic_remote = est.Sensor(1).read_mode(est.InfraredSensor.MODE_REMOTE)
assert 0 <= generic_remote <= 255
est._program_result(7)
