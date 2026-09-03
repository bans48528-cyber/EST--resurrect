"""Single sustained tone probe for VS1003 PCM tone output."""
import est
import est_runtime as rt

est.display.clear()
est.display.text_line(1, "Single Tone")
est.display.text_line(3, "A4 2 seconds")
est.audio.set_volume(90)
try:
    est.audio.tone(69, -1)
    rt.sleep(2)
    est.audio.stop()
    rt.sleep(0.3)
    est._program_result(12303)
finally:
    est.audio.stop()
