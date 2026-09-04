import est
import est_runtime as rt


@rt.on_start
def stack_1():
    try:
        est.audio.set_volume(80)
        est.display.text_line(1, "Audio boot")
        est.audio.play("Animals/Cat purr", wait=True)
        status = est.audio._diagnostics()
        est._program_result(1000000 + status[5])
        est.display.text_line(2, "OK")
    except Exception:
        status = est.audio._diagnostics()
        est._program_result(
            status[0] * 10000000
            + status[6] * 1000000
            + status[8] * 100000
            + (status[5] & 0xFFFF)
        )
        est.display.text_line(2, "ERR")
        est.display.text_line(3, "P/F " + str(status[6]) + "/" + str(status[8]))
        est.display.text_line(4, "M " + str(status[5]))
        raise


rt.run()
