import est


pair = est.MotorPair("A", "C")
pair.run_speed(60, 60)
started = est.millis()
while est.millis() - started < 500:
    pass
est._program_result(est.millis() - started)
raise RuntimeError("intentional motor pair cleanup test")
