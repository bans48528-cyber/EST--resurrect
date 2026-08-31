import est
import est_runtime as rt


est._program_result(100)

color = rt.color(4)
reflection = color.reflection()
assert 0 <= reflection <= 100
est._program_result(101)

ultrasonic = rt.ultrasonic("3")
distance_cm = ultrasonic.distance("centimeters")
assert distance_cm >= 0
est._program_result(102)

rt.motor_set_speed("A", 30)
rt.motor_start("A", "clockwise")
try:
    rt.sleep(1)
    reflection = color.reflection()
    distance_cm = ultrasonic.distance("centimeters")
    assert 0 <= reflection <= 100
    assert distance_cm >= 0
    rt.sleep(1)
    est._program_result(103)
finally:
    rt.motor_stop("A")

for _ in range(8):
    ambient = color.ambient()
    reflection = color.reflection()
    assert 0 <= ambient <= 100
    assert 0 <= reflection <= 100

est._program_result(122)
