import est
import est_runtime as rt


color = rt.color(4)
ultrasonic = rt.ultrasonic("3")

for sample in range(1200):
    reflection = color.reflection()
    distance_cm = ultrasonic.distance("centimeters")
    assert 0 <= reflection <= 100
    assert distance_cm >= 0
    if sample % 100 == 0:
        est._program_result(sample)
    rt.sleep(0.05)

est._program_result(1200)
