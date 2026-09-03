"""Muted MP3 decoder check. Does not start any motor."""
import est
import est_runtime as rt

est.audio.set_volume(0)
clock_ok = False
mp3_seen = False
sample_rate_ok = False
muted = False
try:
    est.audio.play("Piano/C4")
    for index in range(180):
        rt.sleep(0.005)
        clock_value = est.audio._read_register(3)
        header = est.audio._read_register(9)
        sample_rate = est.audio._read_register(5)
        volume = est.audio._read_register(11)
        clock_ok = clock_ok or clock_value == 0x9800
        mp3_seen = mp3_seen or (header is not None and header & 0xFFE0 == 0xFFE0)
        sample_rate_ok = sample_rate_ok or sample_rate == 22050
        muted = muted or volume == 0xFEFE
    status = est.audio._diagnostics()
    flags = int(clock_ok) + 2 * int(mp3_seen) + 4 * int(sample_rate_ok) + 8 * int(muted)
    est.display.clear()
    est.display.text_line(1, "Audio diagnostic")
    est.display.text_line(3, "Flags: " + str(flags))
    est.display.text_line(4, "DREQ: " + str(status[10]))
    est.display.text_line(5, "Bytes: " + str(status[2]))
    est._program_result(flags * 1000000 + min(status[10], 999999))
finally:
    est.audio.stop()
