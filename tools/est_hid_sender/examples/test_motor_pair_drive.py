import est


def pause(duration_ms):
    started = est.millis()
    while est.millis() - started < duration_ms:
        pass


pair = est.MotorPair("A", "C")
drive = est.DriveBase("A", "C")
est._program_result(100)
assert pair.left_port() == "A"
assert pair.right_port() == "C"

# A/C are large motors while D is medium on the current test device.
try:
    est.MotorPair("A", "D").run_speed(20, 20)
    assert False
except RuntimeError:
    pass
est._program_result(200)

pair.run_time(
    duration_ms=800,
    left_speed=80,
    right_speed=40,
    stop=pair.STOP_BRAKE,
    wait=True,
)
est._program_result(300)
assert pair.state() == pair.STATE_COMPLETE
pause(300)

pair.run_angle(left_degrees=720, right_degrees=720, speed=80)
gc_count = 0
while pair.running():
    est.force_gc()
    gc_count += 1
est._program_result(400)
pair_status = pair.status()
assert pair_status[0] == 0
assert pair_status[1] == pair.STATE_COMPLETE
assert abs(pair_status[2] - 720) <= 90
assert abs(pair_status[3] - 720) <= 90
est._program_result(500)
pause(300)

drive.straight_time(
    duration_ms=800,
    speed=-80,
    stop=drive.STOP_BRAKE,
    wait=True,
)
est._program_result(600)
assert drive.state() == drive.STATE_COMPLETE
pause(300)

drive.steer_angle(steering=50, degrees=720, speed=80, wait=True)
est._program_result(700)
drive_status = drive.status()
assert drive_status[0] == 0
assert drive_status[1] == drive.STATE_COMPLETE
assert drive_status[2] > drive_status[3] > 0

maximum_error = pair_status[5]
if maximum_error > 999:
    maximum_error = 999
est._program_result(gc_count * 1000 + maximum_error)
