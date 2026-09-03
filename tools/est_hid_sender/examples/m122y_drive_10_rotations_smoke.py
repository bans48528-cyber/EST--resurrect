import est_runtime as rt


@rt.on_start
def stack_1():
    rt.drive_move_for("forward", 10, "rotations")


rt.run()
