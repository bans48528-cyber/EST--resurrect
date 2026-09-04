import est
import est_runtime as rt


@rt.on_start
def stack_1():
    est.audio.set_volume(80)
    rt.sleep(1.5)

    est.display.text_line(1, "Click")
    est.audio.play("System/Click", wait=True)
    rt.sleep(0.5)

    est.display.text_line(1, "Connect")
    est.audio.play("System/Connect", wait=True)
    rt.sleep(0.5)

    est.display.text_line(1, "Download")
    est.audio.play("System/Download", wait=True)
    rt.sleep(0.5)

    est.display.text_line(1, "Start")
    est.audio.play("Information/Start", wait=True)
    rt.sleep(0.5)

    est.display.text_line(2, "Done")
    est._program_result(12404)


rt.run()
