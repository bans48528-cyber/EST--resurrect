import est
import est_runtime as rt


@rt.on_start
def stack_1():
    est.display.text_line(1, "EST")
    est.display.text_line(12, "LINE 12")
    est._program_result(122)


rt.run()
