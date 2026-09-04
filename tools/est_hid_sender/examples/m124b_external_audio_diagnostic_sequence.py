import est
import est_runtime as rt


def mark(code):
    status = est.audio._diagnostics()
    packed = (
        code * 100000
        + status[0] * 10000
        + status[6] * 1000
        + status[8] * 100
        + min(status[10], 99)
    )
    est._program_result(packed)
    est.display.text_line(1, "Step " + str(code))
    est.display.text_line(2, "S/P/F " + str(status[0]) + "/" + str(status[6]) + "/" + str(status[8]))
    est.display.text_line(3, "DREQ " + str(status[10]))


@rt.on_start
def stack_1():
    try:
        est.audio.set_volume(80)
        mark(10)
        est.audio.play("Animals/Cat purr", wait=True)
        mark(11)
        rt.sleep(0.2)
        mark(12)
        est.audio.play("Expressions/Cheering", wait=True)
        mark(13)
        rt.sleep(0.2)
        mark(14)
        est.audio.play("System/Start up", wait=True)
        mark(15)
        est.display.text_line(4, "Done")
    except Exception:
        mark(90)
        raise


rt.run()
