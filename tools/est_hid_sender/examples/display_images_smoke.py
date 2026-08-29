import est
import est_runtime as rt


IMAGES = (
    "Expressions/Big smile",
    "Expressions/Heart large",
    "Expressions/Heart small",
    "Expressions/Mouth 1 open",
    "Expressions/Mouth 1 shut",
    "Expressions/Mouth 2 open",
    "Expressions/Mouth 2 shut",
    "Expressions/Sad",
    "Expressions/Sick",
    "Expressions/Smile",
    "Expressions/Swearing",
    "Expressions/Talking",
    "Expressions/Wink",
    "Expressions/ZZZ",
    "Eyes/Angry",
    "Eyes/Awake",
    "Eyes/Black eye",
    "Eyes/Bottom left",
    "Eyes/Bottom right",
    "Eyes/Crazy 1",
    "Eyes/Crazy 2",
    "Eyes/Disappointed",
    "Eyes/Dizzy",
    "Eyes/Down",
    "Eyes/Evil",
    "Eyes/Hurt",
    "Eyes/Knocked out",
    "Eyes/Love",
    "Eyes/Middle left",
    "Eyes/Middle right",
    "Eyes/Neutral",
    "Eyes/Nuclear",
    "Eyes/Pinch left",
    "Eyes/Pinch middle",
    "Eyes/Pinch right",
    "Eyes/Tear",
    "Eyes/Tired left",
    "Eyes/Tired middle",
    "Eyes/Tired right",
    "Eyes/Toxic",
    "Eyes/Up",
    "Eyes/Winking",
)


try:
    est.display.image("Eyes/Unknown")
    assert False
except ValueError:
    pass

rt.display_image_for("Eyes/Neutral", 2)

for image in IMAGES:
    est.display.image(image)
    est.display.refresh()
    rt.sleep(0.7)

est._program_result(len(IMAGES))
