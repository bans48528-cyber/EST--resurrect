import est
import est_runtime as rt


def show(line, text):
    est.display.text_line(line, text)


@rt.on_broadcast("message_1")
async def receive_message_1():
    show(2, "MSG1 START")
    await rt.sleep(1)
    show(3, "MSG1 END")


@rt.on_broadcast("message_8")
def receive_message_8():
    show(5, "MSG8 RECEIVED")


@rt.on_start
async def main():
    est.display.clear()
    show(1, "BROADCAST TEST")
    await rt.broadcast("message_1", wait=True)
    show(4, "WAIT COMPLETE")
    rt.broadcast("message_8", wait=False)
    await rt.sleep(5)


rt.run()
