"""Two phrases of Twinkle, Twinkle using the device's existing Flash MP3s."""
import est
import est_runtime as rt

notes = (
    ("C4", 450), ("C4", 450), ("G4", 450), ("G4", 450),
    ("A4", 450), ("A4", 450), ("G4", 900),
    ("F4", 450), ("F4", 450), ("E4", 450), ("E4", 450),
    ("D4", 450), ("D4", 450), ("C4", 900),
)

est.display.clear()
est.display.text_line(1, "Internal Piano")
est.display.text_line(3, "Twinkle, Twinkle")
est.audio.set_volume(85)
try:
    rt.sleep(1.2)
    index = 0
    for note, duration in notes:
        est._program_result(100 + index)
        est.audio.play("Piano/" + note)
        rt.sleep(duration / 1000)
        audio_status = est.audio._diagnostics()
        est._program_result(audio_status[0] * 10000000 +
                            audio_status[6] * 1000000 + audio_status[2])
        if audio_status[0] != 1:
            est._program_result(audio_status[8] * 1000000 +
                                audio_status[7] * 65536 + audio_status[5])
            raise RuntimeError("piano playback stopped early")
        est.audio.stop()
        rt.sleep(0.03)
        index += 1
    est._program_result(12301)
finally:
    est.audio.stop()
