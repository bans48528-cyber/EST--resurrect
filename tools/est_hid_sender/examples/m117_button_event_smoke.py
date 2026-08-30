import est
import est_runtime as rt


pressed = [0]


@rt.on_brick_button("confirm", "pressed")
def confirm_pressed():
    pressed[0] += 1


@rt.on_brick_button("confirm", "released")
def confirm_released():
    if pressed[0] == 1:
        est._program_result(1172)
    else:
        est._program_result(-pressed[0])
    rt.stop("all")


rt.run()
