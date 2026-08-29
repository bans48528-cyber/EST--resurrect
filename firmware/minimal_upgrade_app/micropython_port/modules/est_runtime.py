import est


API_VERSION = 1

_COAST = 0
_BRAKE = 1
_HOLD = 2
_start_handlers = []
_motors = {}
_motor_speeds = {}
_motor_stop_actions = {}
_sensors = {}
_drive_left_port = "B"
_drive_right_port = "C"
_drive_speed = 50
_drive_stop_action = "float"
_drive_base = None
_timer_started_ms = est.millis()


def _not_implemented(name):
    raise NotImplementedError(
        "est_runtime." + name + " is not implemented in API version 1"
    )


def _unsupported(name):
    def unsupported(*args, **kwargs):
        _not_implemented(name)

    return unsupported


def _absolute(value):
    if value < 0:
        return -value
    return value


def _elapsed_ms(start_ms):
    return (est.millis() - start_ms) & 0xFFFFFFFF


def _speed_magnitude(value):
    value = _absolute(int(value))
    if value > 100:
        raise ValueError("speed magnitude must be 0..100")
    return value


def _stop_mode(action):
    if action == "float" or action == "coast":
        return _COAST
    if action == "brake":
        return _BRAKE
    if action == "hold":
        return _HOLD
    raise ValueError("stop action must be float, coast, brake or hold")


def _direction_sign(direction, forward_name, reverse_name):
    if direction == forward_name:
        return 1
    if direction == reverse_name:
        return -1
    raise ValueError("unsupported direction")


def _degrees_for(amount, unit):
    if unit == "rotations":
        return int(amount * 360)
    if unit == "degrees":
        return int(amount)
    raise ValueError("unit must be rotations, degrees or seconds")


def _sensor(key, constructor, port):
    cache_key = key + str(port)
    if cache_key not in _sensors:
        _sensors[cache_key] = constructor(port)
    return _sensors[cache_key]


def on_start(function):
    _start_handlers.append(function)
    return function


def run():
    for function in _start_handlers:
        function()


def yield_once():
    est.millis()


def seconds_to_ms(seconds):
    return int(seconds * 1000)


def sleep(seconds):
    duration_ms = seconds_to_ms(seconds)
    if duration_ms < 0:
        raise ValueError("sleep duration must not be negative")
    started_ms = est.millis()
    while _elapsed_ms(started_ms) < duration_ms:
        yield_once()


def wait_until(predicate):
    while not boolean(predicate()):
        yield_once()


def timer_seconds():
    return _elapsed_ms(_timer_started_ms) / 1000


def reset_timer():
    global _timer_started_ms
    _timer_started_ms = est.millis()


def compare(left, comparator, right):
    if comparator == "less":
        return left < right
    if comparator == "greater":
        return left > right
    if comparator == "equal":
        return left == right
    if comparator == "changed":
        _not_implemented("compare changed")
    raise ValueError("unknown comparator")


def boolean(value):
    return bool(value)


def repeat_count(value):
    value = int(value)
    if value < 0:
        return 0
    return value


def motor(port):
    port = str(port)
    if port not in _motors:
        _motors[port] = est.Motor(port)
    return _motors[port]


def motor_set_speed(port, speed):
    _motor_speeds[str(port)] = _speed_magnitude(speed)


def motor_set_stop_action(port, action):
    _stop_mode(action)
    _motor_stop_actions[str(port)] = action


def motor_start(port, direction):
    port = str(port)
    speed = _motor_speeds.get(port, 50)
    sign = _direction_sign(direction, "clockwise", "counterclockwise")
    motor(port).run_speed(sign * speed)


def motor_stop(port):
    port = str(port)
    action = _motor_stop_actions.get(port, "float")
    motor(port).stop(_stop_mode(action))


def motor_run_for(port, direction, amount, unit, speed=None):
    port = str(port)
    configured_speed = _motor_speeds.get(port, 50)
    if speed is None:
        speed = configured_speed
    speed = int(speed)
    if direction is None:
        sign = -1 if speed < 0 else 1
    else:
        sign = _direction_sign(direction, "clockwise", "counterclockwise")
    speed = _speed_magnitude(speed)
    action = _motor_stop_actions.get(port, "float")
    stop_mode = _stop_mode(action)
    device = motor(port)

    if unit == "seconds":
        duration_ms = seconds_to_ms(_absolute(amount))
        if duration_ms == 0:
            return
        device.run_time(duration_ms, speed=sign * speed, stop=stop_mode)
        while device.state() == device.STATE_TIMED:
            yield_once()
        return

    degrees = _absolute(_degrees_for(amount, unit))
    if degrees == 0:
        return
    device.run_angle(sign * degrees, speed=speed, stop=stop_mode)
    while device.state() == device.STATE_POSITION:
        yield_once()


def _get_drive_base():
    global _drive_base
    if _drive_base is None:
        _drive_base = est.DriveBase(_drive_left_port, _drive_right_port)
    return _drive_base


def drive_set_pair(left_port, right_port):
    global _drive_left_port, _drive_right_port, _drive_base
    left_port = str(left_port)
    right_port = str(right_port)
    if left_port == right_port:
        raise ValueError("drive motor ports must differ")
    _drive_left_port = left_port
    _drive_right_port = right_port
    _drive_base = None


def drive_set_speed(speed):
    global _drive_speed
    _drive_speed = _speed_magnitude(speed)


def drive_set_stop_action(action):
    global _drive_stop_action
    _stop_mode(action)
    _drive_stop_action = action


def drive_move_for(direction, amount, unit):
    sign = _direction_sign(direction, "forward", "backward")
    drive = _get_drive_base()
    stop_mode = _stop_mode(_drive_stop_action)
    if unit == "seconds":
        duration_ms = seconds_to_ms(_absolute(amount))
        if duration_ms == 0:
            return
        drive.straight_time(
            duration_ms, speed=sign * _drive_speed, stop=stop_mode, wait=True
        )
        return
    degrees = _absolute(_degrees_for(amount, unit))
    if degrees == 0:
        return
    drive.straight_angle(
        sign * degrees, speed=_drive_speed, stop=stop_mode, wait=True
    )


def drive_steer_for(steering, amount, unit, speed=None):
    if speed is None:
        speed = _drive_speed
    speed = _speed_magnitude(speed)
    sign = -1 if amount < 0 else 1
    drive = _get_drive_base()
    stop_mode = _stop_mode(_drive_stop_action)
    if unit == "seconds":
        duration_ms = seconds_to_ms(_absolute(amount))
        if duration_ms == 0:
            return
        drive.steer_time(
            int(steering), duration_ms, speed=sign * speed,
            stop=stop_mode, wait=True
        )
        return
    degrees = _absolute(_degrees_for(amount, unit))
    if degrees == 0:
        return
    drive.steer_angle(
        int(steering), degrees, speed=sign * speed,
        stop=stop_mode, wait=True
    )


def drive_start_steer(steering, speed=None):
    if speed is None:
        speed = _drive_speed
    speed = int(speed)
    _speed_magnitude(speed)
    _get_drive_base().steer(int(steering), speed=speed)


def drive_stop():
    _get_drive_base().stop(_stop_mode(_drive_stop_action))


def color(port):
    return _sensor("color:", est.ColorSensor, port)


def touch(port):
    return _sensor("touch:", est.TouchSensor, port)


def gyro(port):
    return _sensor("gyro:", est.GyroSensor, port)


class _Ultrasonic:
    def __init__(self, port):
        self._sensor = est.UltrasonicSensor(port)

    def distance(self, unit):
        if unit == "centimeters":
            return self._sensor.distance_mm() / 10
        if unit == "inches":
            return self._sensor.inches_tenths() / 10
        raise ValueError("distance unit must be centimeters or inches")

    def presence(self):
        return self._sensor.presence()


def ultrasonic(port):
    return _sensor("ultrasonic:", _Ultrasonic, port)


class _Infrared:
    def __init__(self, port):
        self._sensor = est.InfraredSensor(port)

    def proximity(self):
        return self._sensor.proximity()

    def beacon(self):
        return self._sensor.beacon()

    def remote(self):
        return self._sensor.remote()

    def beacon_heading(self, channel):
        _not_implemented("infrared beacon_heading")

    def beacon_proximity(self, channel):
        _not_implemented("infrared beacon_proximity")

    def beacon_buttons(self, channel):
        _not_implemented("infrared beacon_buttons")

    def beacon_button_pressed(self, channel, button):
        _not_implemented("infrared beacon_button_pressed")

    def beacon_active(self, channel):
        _not_implemented("infrared beacon_active")


def infrared(port):
    return _sensor("infrared:", _Infrared, port)


def display_image_for(name, seconds):
    est.display.image(name)
    est.display.refresh()
    sleep(seconds)


broadcast = _unsupported("broadcast")
color_calibrate = _unsupported("color_calibrate")
color_reset_calibration = _unsupported("color_reset_calibration")
drive_dual_speed_for = _unsupported("drive_dual_speed_for")
drive_start_dual_speed = _unsupported("drive_start_dual_speed")
ir_beacon_compare = _unsupported("ir_beacon_compare")
on_brick_button = _unsupported("on_brick_button")
on_broadcast = _unsupported("on_broadcast")
on_color = _unsupported("on_color")
on_condition = _unsupported("on_condition")
on_gyro_angle = _unsupported("on_gyro_angle")
on_ir_beacon_button = _unsupported("on_ir_beacon_button")
on_ir_proximity = _unsupported("on_ir_proximity")
on_timer_gt = _unsupported("on_timer_gt")
on_touch = _unsupported("on_touch")
on_ultrasonic = _unsupported("on_ultrasonic")
stop = _unsupported("stop")
stop_other_stacks = _unsupported("stop_other_stacks")
wait_brick_button = _unsupported("wait_brick_button")
wait_color = _unsupported("wait_color")
wait_gyro = _unsupported("wait_gyro")
wait_ir_beacon_button = _unsupported("wait_ir_beacon_button")
wait_ir_proximity = _unsupported("wait_ir_proximity")
wait_touch = _unsupported("wait_touch")
wait_ultrasonic = _unsupported("wait_ultrasonic")
