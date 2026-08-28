import est

started_ms = est.millis()
started_ticks = est._runtime_ticks()
while est.millis() - started_ms < 1000:
    pass
est._program_result(est._runtime_ticks() - started_ticks)
