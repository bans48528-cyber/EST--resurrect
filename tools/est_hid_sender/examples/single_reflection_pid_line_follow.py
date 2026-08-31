import est
import est_runtime as rt


SENSOR_PORT = 4
LEFT_MOTOR = "B"
RIGHT_MOTOR = "C"

# Set this to the midpoint between measured black and white reflections.
TARGET_REFLECTION = 50
BASE_SPEED = 35
MAX_SPEED = 75

KP = 1.15
KI = 0.02
KD = 0.035

CONTROL_PERIOD = 0.02
INTEGRAL_LIMIT = 250
STEERING_SIGN = 1


def clamp(value, minimum, maximum):
    if value < minimum:
        return minimum
    if value > maximum:
        return maximum
    return value


pair = est.MotorPair(LEFT_MOTOR, RIGHT_MOTOR)
sensor = rt.color(SENSOR_PORT)


@rt.on_start
async def follow_line():
    integral = 0
    filtered_derivative = 0
    previous_error = (
        sensor.reflection() - TARGET_REFLECTION
    ) * STEERING_SIGN
    previous_ms = est.millis()

    try:
        while True:
            now_ms = est.millis()
            elapsed_ms = (now_ms - previous_ms) & 0xFFFFFFFF
            if elapsed_ms == 0:
                await rt.yield_once()
                continue

            reflection = sensor.reflection()
            error = (reflection - TARGET_REFLECTION) * STEERING_SIGN
            dt = elapsed_ms / 1000

            integral = clamp(
                integral + error * dt,
                -INTEGRAL_LIMIT,
                INTEGRAL_LIMIT,
            )
            derivative = (error - previous_error) / dt
            filtered_derivative = (
                filtered_derivative * 0.7 + derivative * 0.3
            )

            correction = (
                KP * error
                + KI * integral
                + KD * filtered_derivative
            )
            left_speed = int(clamp(
                BASE_SPEED + correction,
                -MAX_SPEED,
                MAX_SPEED,
            ))
            right_speed = int(clamp(
                BASE_SPEED - correction,
                -MAX_SPEED,
                MAX_SPEED,
            ))

            pair.run_speed(left_speed, right_speed)
            previous_error = error
            previous_ms = now_ms
            await rt.sleep(CONTROL_PERIOD)
    finally:
        pair.stop(pair.STOP_COAST)


rt.run()
