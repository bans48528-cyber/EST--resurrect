"""Simple piano MP3 smoke test for the M1.23J playback baseline."""
import est
import est_runtime as rt

est.display.clear()
est.display.text_line(1, "Piano baseline")
est.audio.set_volume(85)

try:
    for note in ("C4", "E4", "G4", "C5"):
        est.audio.play("Piano/" + note)
        rt.sleep(0.65)
        est.audio.stop()
        rt.sleep(0.08)
    est._program_result(12304)
finally:
    est.audio.stop()
