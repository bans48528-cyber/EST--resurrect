import est
import est_runtime as rt


est.display.clear()
est.display.text(2, 2, "REGULAR BLACK", font="regular_black")
est.display.text(2, 13, "BOLD BLACK", font="bold_black")
est.display.text(2, 24, "LARGE BLACK", font="large_black")
est.display.text(2, 44, "REGULAR WHITE", font="regular_white")
est.display.text(2, 55, "BOLD WHITE", font="bold_white")
est.display.text(2, 67, "LARGE WHITE", font="large_white")
est.display.refresh()

while True:
    rt.sleep(1)
