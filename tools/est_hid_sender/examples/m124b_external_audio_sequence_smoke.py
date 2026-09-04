import est
import est_runtime as rt


@rt.on_start
def stack_1():
    est.display.clear()
    est.display.text_line(1, "Audio 1")
    est.audio.play("Animals/Cat purr", wait=True)
    rt.sleep(0.2)
    est.display.text_line(2, "Audio 2")
    est.audio.play("Expressions/Cheering", wait=True)
    rt.sleep(0.2)
    est.display.text_line(3, "Audio 3")
    est.audio.play("System/Start up", wait=True)
    est.display.text_line(4, "Done")


rt.run()
