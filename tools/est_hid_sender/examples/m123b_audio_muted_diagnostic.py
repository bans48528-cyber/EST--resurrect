"""Read decoder status without audible output or motor commands."""
import est
import est_runtime as rt

est.audio.set_volume(0)
try:
    est.audio.play("Piano/C4")
    rt.sleep(0.35)
    status = est.audio._diagnostics()
    est._program_result(status[1] * 65536 + status[5])
finally:
    est.audio.stop()

