import est
import est_runtime as rt


color = rt.color(4)

for sample in range(200):
    reflection = color.reflection()
    assert 0 <= reflection <= 100
    if sample % 20 == 0:
        est._program_result(sample)
    rt.sleep(0.05)

est._program_result(122)
