"""Twinkle phrase using one continuous PCM tone stream."""
import est
import est_runtime as rt

notes = (
    (60, 450), (60, 450), (67, 450), (67, 450),
    (69, 450), (69, 450), (67, 900),
    (65, 450), (65, 450), (64, 450), (64, 450),
    (62, 450), (62, 450), (60, 900),
)

est.display.clear()
est.display.text_line(1, "Continuous Tone")
est.display.text_line(3, "No MP3 per note")
est.audio.set_volume(75)
try:
    rt.sleep(1)
    index = 0
    for note, duration in notes:
        est._program_result(200 + index)
        est.audio.tone(note, -1)
        rt.sleep(duration / 1000)
        index += 1
    est.audio.stop()
    rt.sleep(0.2)
    est._program_result(12302)
finally:
    est.audio.stop()
