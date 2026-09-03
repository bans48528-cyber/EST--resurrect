"""Piano playback smoke for the legacy EST piano stream window."""
import est

est.display.clear()
est.display.text_line(1, "Piano legacy")
est.display.text_line(3, "No per-note stop")
est.audio.set_volume(85)

try:
    for note in ("C4", "E4", "G4", "C5"):
        est.audio.play("Piano/" + note, wait=True)
    est._program_result(12311)
finally:
    est.audio.stop()
