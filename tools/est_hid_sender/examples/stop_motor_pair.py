import est


pair = est.MotorPair("A", "C")

# Leave a completed position task behind, then make sure stop() also stops
# the newer speed task instead of only clearing the old position state.
pair.run_angle(left_degrees=360, right_degrees=360, speed=80, wait=True)
pair.run_speed(80, 80)
started = est.millis()
while est.millis() - started < 800:
    pass
pair.stop(pair.STOP_BRAKE)
elapsed = est.millis() - started

assert not pair.running()
assert est.Motor("A").power() == 0
assert est.Motor("C").power() == 0
est._program_result(elapsed)
