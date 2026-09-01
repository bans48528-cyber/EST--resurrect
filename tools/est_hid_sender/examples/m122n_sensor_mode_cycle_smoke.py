import est
import est_runtime as rt


@rt.on_start
async def stack_1():
    for cycle in range(2):
        est.display.clear()
        est.display.refresh()
        est._program_result(cycle * 10 + 1)
        rt.display_text_line(1, rt.color(1).reflection())
        await rt.sleep(1)
        est._program_result(cycle * 10 + 2)
        rt.display_text_line(2, rt.color(1).ambient())
        await rt.sleep(1)
        est._program_result(cycle * 10 + 3)
        rt.display_text_line(3, rt.color(1).color())
        await rt.sleep(1)
    rt.display_text_line(4, "OK")
    est._program_result(99)


rt.run()
