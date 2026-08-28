import est


def wait_ms(duration_ms):
    started_ms = est.millis()
    while est.millis() - started_ms < duration_ms:
        pass


assert est.display.WIDTH == 180
assert est.display.HEIGHT == 128

est.display.clear()
est.display.rectangle(0, 0, 180, 128)
est.display.line(0, 0, 179, 127)
est.display.line(179, 0, 0, 127)
est.display.rectangle(24, 42, 132, 38, True)
est.display.rectangle(28, 46, 124, 30, True, False)
est.display.text(54, 12, "M1.05A", 2)
est.display.text(52, 53, "DISPLAY OK", 1)
est.display.bitmap(86, 94, 8, 8, b"\x3c\x42\xa5\x81\xa5\x99\x42\x3c")
est.display.pixel(90, 98, False)

try:
    est.display.rectangle(179, 127, 2, 2)
    assert False
except ValueError:
    pass

ticks_before = est._runtime_ticks()
refresh_started_ms = est.millis()
est.display.refresh()
refresh_duration_ms = est.millis() - refresh_started_ms
ticks_after = est._runtime_ticks()
assert refresh_duration_ms < 1000
assert ticks_after >= ticks_before
est._program_result(10)

est.led.set(est.led.RED)
assert est.led.get() == est.led.RED
wait_ms(600)
est._program_result(20)

est.led.set(est.led.BLUE)
assert est.led.get() == est.led.BLUE
wait_ms(600)
est._program_result(21)

est.led.set(est.led.RED_BLUE)
assert est.led.get() == est.led.RED_BLUE
wait_ms(600)
est._program_result(22)

est.led.set(est.led.OFF)
assert est.led.get() == est.led.OFF

est.backlight.set(20)
assert est.backlight.get() == 20
wait_ms(600)
est._program_result(30)

est.backlight.set(0)
assert est.backlight.get() == 0
wait_ms(600)
est._program_result(31)

est.backlight.set(100)
assert est.backlight.get() == 100
wait_ms(4000)
est._program_result(7)
