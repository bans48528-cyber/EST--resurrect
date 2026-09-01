import est_runtime as rt
import est

@rt.on_start
async def stack_1():
  while True:
    est.display.clear()
    est.display.refresh()
    rt.display_text_line(1, rt.color('1').reflection())
    await rt.sleep(1)
    rt.display_text_line(2, rt.color('1').ambient())
    await rt.sleep(1)
    rt.display_text_line(3, rt.color('1').color())
    await rt.sleep(1)
    await rt.yield_once()


rt.run()
