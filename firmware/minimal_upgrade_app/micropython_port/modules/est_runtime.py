import est


API_VERSION = 1

_COAST = 0
_BRAKE = 1
_HOLD = 2
_MAX_TASKS = 8
_MAX_EVENTS = 16
_BUTTON_DEBOUNCE_MS = 20
_MOTOR_PORTS = ("A", "B", "C", "D")
_SYNC_OWNER = object()
_start_handlers = []
_event_handlers = []
_current_task = None
_scheduler_running = False
_command_generation = 0
_motor_commands = {}
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
_timer_generation = 0


class _StopStack(BaseException):
    pass


class _Task:
    def __init__(self, task_id):
        self.task_id = task_id
        self.iterator = None
        self.event = None
        self.active = False
        self.cancelled = False
        self.synchronous = False


class _Event:
    def __init__(self, event_type, handler, argument=None, option=None):
        self.handler = handler
        self.type = event_type
        self.argument = argument
        self.option = option
        self.last_value = None
        self.armed = True
        self.active_task = None
        self.pending = False
        self.candidate_value = None
        self.candidate_since_ms = 0
        self.timer_generation = _timer_generation


_tasks = [_Task(task_id) for task_id in range(_MAX_TASKS)]


class _YieldOnce:
    def __init__(self):
        self._yielded = False

    def __iter__(self):
        return self

    def __await__(self):
        return self

    def __next__(self):
        if self._yielded:
            raise StopIteration
        self._yielded = True
        return None


class _ConditionWait:
    def __init__(self, predicate):
        self._predicate = predicate

    def __iter__(self):
        return self

    def __await__(self):
        return self

    def __next__(self):
        if boolean(self._predicate()):
            raise StopIteration
        return None


class _OperationWait:
    def __init__(self, ports, generation, state, running_state, fault_state):
        self._ports = ports
        self._generation = generation
        self._state = state
        self._running_state = running_state
        self._fault_state = fault_state

    def __iter__(self):
        return self

    def __await__(self):
        return self

    def __next__(self):
        if not _command_is_current(self._ports, self._generation):
            raise StopIteration
        state = self._state()
        if state == self._fault_state:
            raise RuntimeError("motor operation failed")
        if state != self._running_state:
            raise StopIteration
        return None


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
    port = int(port)
    if port < 1 or port > 4:
        raise ValueError("sensor port must be 1..4")
    cache_key = key + str(port)
    if cache_key not in _sensors:
        _sensors[cache_key] = constructor(port)
    return _sensors[cache_key]


def _in_async_task():
    return (
        _scheduler_running and _current_task is not None and
        not _current_task.synchronous
    )


def _reject_sync_event_wait(name):
    if (
        _scheduler_running and _current_task is not None and
        _current_task.synchronous
    ):
        raise RuntimeError(name + " requires an async event handler")


def _command_owner():
    if _current_task is None:
        return _SYNC_OWNER
    if _current_task.cancelled:
        raise _StopStack
    return _current_task


def _command_is_current(ports, generation):
    for port in ports:
        command = _motor_commands.get(port)
        if command is None or command[1] != generation:
            return False
    return True


def _next_command_generation():
    global _command_generation
    _command_generation += 1
    return _command_generation


def _begin_motor_command(ports, kind, allow_speed_retarget=False):
    owner = _command_owner()
    can_retarget = allow_speed_retarget and len(ports) == 1
    previous = _motor_commands.get(ports[0]) if can_retarget else None

    can_retarget = previous is not None and previous[2] == kind
    if not can_retarget:
        for port in ports:
            if port in _motor_commands:
                motor(port).stop(_COAST)
    generation = _next_command_generation()
    for port in ports:
        _motor_commands[port] = (owner, generation, kind)
    return generation


def _record_motor_stop(ports, stop_mode):
    owner = _command_owner()
    generation = _next_command_generation()
    if stop_mode == _HOLD:
        for port in ports:
            _motor_commands[port] = (owner, generation, "hold")
    else:
        for port in ports:
            _motor_commands.pop(port, None)
    return generation


def _release_task_motors(task):
    for port in _MOTOR_PORTS:
        command = _motor_commands.get(port)
        if command is not None and command[0] is task:
            try:
                motor(port).stop(_COAST)
            except Exception:
                pass
            _motor_commands.pop(port, None)


def _finish_task(task, cancelled=False):
    event = task.event
    task.cancelled = cancelled
    task.active = False
    _release_task_motors(task)
    if event is not None and event.active_task is task:
        event.active_task = None
    task.iterator = None
    task.event = None
    task.synchronous = False


def _cancel_task(task):
    if not task.active:
        return
    _finish_task(task, True)


def _free_task_slot():
    for task in _tasks:
        if not task.active:
            return task
    return None


def _has_active_tasks():
    for task in _tasks:
        if task.active:
            return True
    return False


def _start_async_task(iterator, event=None):
    task = _free_task_slot()
    if task is None:
        return None
    task.iterator = iterator
    task.event = event
    task.active = True
    task.cancelled = False
    task.synchronous = False
    if event is not None:
        event.active_task = task
    return task


def _launch_event(event):
    global _current_task
    task = _free_task_slot()
    if task is None:
        event.pending = True
        return
    event.pending = False
    event.active_task = task
    task.iterator = None
    task.event = event
    task.active = True
    task.cancelled = False
    task.synchronous = True
    _current_task = task
    try:
        result = event.handler()
    except _StopStack:
        if task.active:
            _cancel_task(task)
        return
    finally:
        _current_task = None
    if hasattr(result, "send"):
        if task.active:
            task.iterator = result
            task.synchronous = False
    elif task.active:
        _finish_task(task)


def _trigger_event(event):
    if event.active_task is not None:
        event.pending = True
        return
    _launch_event(event)


def _dispatch_pending_events():
    for event in _event_handlers:
        if event.pending and event.active_task is None:
            if _free_task_slot() is None:
                return
            _launch_event(event)


def _sample_button_event(event, button_mask, now_ms):
    value = (button_mask & event.argument) != 0
    if event.last_value is None:
        event.last_value = value
        event.candidate_value = value
        event.candidate_since_ms = now_ms
        return
    if value != event.candidate_value:
        event.candidate_value = value
        event.candidate_since_ms = now_ms
        return
    if value == event.last_value:
        return
    if ((now_ms - event.candidate_since_ms) & 0xFFFFFFFF) < _BUTTON_DEBOUNCE_MS:
        return
    event.last_value = value
    if (value and event.option == "pressed") or (
        not value and event.option == "released"
    ):
        _trigger_event(event)


def _sample_condition_event(event):
    value = boolean(event.argument())
    if event.last_value is None:
        event.last_value = value
        event.armed = not value
        return
    event.last_value = value
    if not value:
        event.armed = True
    elif event.armed:
        event.armed = False
        _trigger_event(event)


def _sample_timer_event(event, now_ms):
    if event.timer_generation != _timer_generation:
        event.timer_generation = _timer_generation
        event.last_value = False
        event.armed = True
        return
    value = ((now_ms - _timer_started_ms) & 0xFFFFFFFF) / 1000 > event.argument
    event.last_value = value
    if value and event.armed:
        event.armed = False
        _trigger_event(event)


def _sample_events(now_ms):
    button_mask = None
    for event in _event_handlers:
        if event.type == "button":
            if button_mask is None:
                button_mask = est.buttons.value()
            _sample_button_event(event, button_mask, now_ms)
        elif event.type == "condition":
            _sample_condition_event(event)
        else:
            _sample_timer_event(event, now_ms)


def _register_event(event_type, handler, argument=None, option=None):
    if len(_event_handlers) >= _MAX_EVENTS:
        raise RuntimeError("at most 16 event hats are supported")
    _event_handlers.append(_Event(event_type, handler, argument, option))
    return handler


def on_start(function):
    _start_handlers.append(function)
    return function


def on_brick_button(button, event):
    if button == "center":
        button = "confirm"
    if button == "back" or button == "none":
        raise ValueError("back and none cannot be event buttons")
    names = ("left", "right", "up", "down", "confirm")
    masks = (
        est.buttons.LEFT, est.buttons.RIGHT, est.buttons.UP,
        est.buttons.DOWN, est.buttons.CONFIRM
    )
    button_mask = None
    for index in range(len(names)):
        if button == names[index]:
            button_mask = masks[index]
            break
    if button_mask is None:
        raise ValueError("invalid event button")
    if event != "pressed" and event != "released":
        raise ValueError("button event must be pressed or released")

    def decorator(handler):
        return _register_event("button", handler, button_mask, event)

    return decorator


def on_condition(predicate):
    def decorator(handler):
        return _register_event("condition", handler, predicate)

    return decorator


def on_timer_gt(seconds):
    try:
        seconds = float(seconds)
    except Exception:
        raise ValueError("timer threshold must be a non-negative number")
    if seconds < 0 or seconds != seconds:
        raise ValueError("timer threshold must be a non-negative number")

    def decorator(handler):
        return _register_event("timer", handler, seconds)

    return decorator


def run():
    global _current_task, _scheduler_running
    if len(_start_handlers) > _MAX_TASKS:
        raise RuntimeError("at most 8 on_start tasks are supported")
    for task in _tasks:
        if task.active:
            _cancel_task(task)
        task.cancelled = False
    for event in _event_handlers:
        event.last_value = None
        event.active_task = None
        event.pending = False
        event.armed = True
        event.candidate_value = None
        event.timer_generation = _timer_generation
    for function in _start_handlers:
        try:
            result = function()
        except _StopStack:
            continue
        if hasattr(result, "send"):
            if _start_async_task(result) is None:
                raise RuntimeError("all 8 task slots are active")

    if not _event_handlers and not _has_active_tasks():
        return

    _scheduler_running = True
    try:
        while True:
            now_ms = est.millis()
            _dispatch_pending_events()
            _sample_events(now_ms)
            for task in _tasks:
                if not task.active:
                    continue
                _current_task = task
                try:
                    task.iterator.send(None)
                except StopIteration:
                    _finish_task(task)
                except _StopStack:
                    _cancel_task(task)
                finally:
                    _current_task = None
            if not _event_handlers and not _has_active_tasks():
                return
    finally:
        _current_task = None
        _scheduler_running = False


def yield_once():
    _reject_sync_event_wait("yield_once")
    if _in_async_task():
        return _YieldOnce()
    est.millis()


def seconds_to_ms(seconds):
    return int(seconds * 1000)


def sleep(seconds):
    _reject_sync_event_wait("sleep")
    duration_ms = seconds_to_ms(seconds)
    if duration_ms < 0:
        raise ValueError("sleep duration must not be negative")
    started_ms = est.millis()
    if _in_async_task():
        return _ConditionWait(lambda: _elapsed_ms(started_ms) >= duration_ms)
    while _elapsed_ms(started_ms) < duration_ms:
        yield_once()


def wait_until(predicate):
    _reject_sync_event_wait("wait_until")
    if _in_async_task():
        return _ConditionWait(predicate)
    while not boolean(predicate()):
        yield_once()


def timer_seconds():
    return _elapsed_ms(_timer_started_ms) / 1000


def reset_timer():
    global _timer_started_ms, _timer_generation
    _timer_started_ms = est.millis()
    _timer_generation += 1


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


def motor_stalled(port):
    return bool(motor(str(port)).stalled())


def motor_set_speed(port, speed):
    _motor_speeds[str(port)] = _speed_magnitude(speed)


def motor_set_stop_action(port, action):
    _stop_mode(action)
    _motor_stop_actions[str(port)] = action


def motor_start(port, direction):
    port = str(port)
    speed = _motor_speeds.get(port, 50)
    sign = _direction_sign(direction, "clockwise", "counterclockwise")
    generation = _begin_motor_command((port,), "speed", True)
    try:
        motor(port).run_speed(sign * speed)
    except Exception:
        if _command_is_current((port,), generation):
            _motor_commands.pop(port, None)
        raise


def motor_stop(port):
    port = str(port)
    action = _motor_stop_actions.get(port, "float")
    stop_mode = _stop_mode(action)
    generation = _begin_motor_command((port,), "stop")
    try:
        motor(port).stop(stop_mode)
    except Exception:
        if _command_is_current((port,), generation):
            _motor_commands.pop(port, None)
        raise
    _record_motor_stop((port,), stop_mode)


def motor_run_for(port, direction, amount, unit, speed=None):
    _reject_sync_event_wait("motor_run_for")
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
        generation = _begin_motor_command((port,), "timed")
        try:
            device.run_time(duration_ms, speed=sign * speed, stop=stop_mode)
        except Exception:
            if _command_is_current((port,), generation):
                _motor_commands.pop(port, None)
            raise
        if _in_async_task():
            return _OperationWait(
                (port,), generation, device.state, device.STATE_TIMED,
                getattr(device, "STATE_FAULT", -1)
            )
        while device.state() == device.STATE_TIMED:
            yield_once()
        return

    degrees = _absolute(_degrees_for(amount, unit))
    if degrees == 0:
        return
    generation = _begin_motor_command((port,), "position")
    try:
        device.run_angle(sign * degrees, speed=speed, stop=stop_mode)
    except Exception:
        if _command_is_current((port,), generation):
            _motor_commands.pop(port, None)
        raise
    if _in_async_task():
        return _OperationWait(
            (port,), generation, device.state, device.STATE_POSITION,
            getattr(device, "STATE_FAULT", -1)
        )
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
    _reject_sync_event_wait("drive_move_for")
    sign = _direction_sign(direction, "forward", "backward")
    drive = _get_drive_base()
    stop_mode = _stop_mode(_drive_stop_action)
    ports = (_drive_left_port, _drive_right_port)
    if unit == "seconds":
        duration_ms = seconds_to_ms(_absolute(amount))
        if duration_ms == 0:
            return
        generation = _begin_motor_command(ports, "drive-timed")
        try:
            drive.straight_time(
                duration_ms, speed=sign * _drive_speed, stop=stop_mode,
                wait=not _in_async_task()
            )
        except Exception:
            if _command_is_current(ports, generation):
                for port in ports:
                    _motor_commands.pop(port, None)
            raise
        if _in_async_task():
            return _OperationWait(
                ports, generation, drive.state, drive.STATE_RUNNING,
                drive.STATE_FAULT
            )
        return
    degrees = _absolute(_degrees_for(amount, unit))
    if degrees == 0:
        return
    generation = _begin_motor_command(ports, "drive-position")
    try:
        drive.straight_angle(
            sign * degrees, speed=_drive_speed, stop=stop_mode,
            wait=not _in_async_task()
        )
    except Exception:
        if _command_is_current(ports, generation):
            for port in ports:
                _motor_commands.pop(port, None)
        raise
    if _in_async_task():
        return _OperationWait(
            ports, generation, drive.state, drive.STATE_RUNNING,
            drive.STATE_FAULT
        )


def drive_steer_for(steering, amount, unit, speed=None):
    _reject_sync_event_wait("drive_steer_for")
    if speed is None:
        speed = _drive_speed
    speed = _speed_magnitude(speed)
    sign = -1 if amount < 0 else 1
    drive = _get_drive_base()
    stop_mode = _stop_mode(_drive_stop_action)
    ports = (_drive_left_port, _drive_right_port)
    if unit == "seconds":
        duration_ms = seconds_to_ms(_absolute(amount))
        if duration_ms == 0:
            return
        generation = _begin_motor_command(ports, "drive-steer-timed")
        try:
            drive.steer_time(
                int(steering), duration_ms, speed=sign * speed,
                stop=stop_mode, wait=not _in_async_task()
            )
        except Exception:
            if _command_is_current(ports, generation):
                for port in ports:
                    _motor_commands.pop(port, None)
            raise
        if _in_async_task():
            return _OperationWait(
                ports, generation, drive.state, drive.STATE_RUNNING,
                drive.STATE_FAULT
            )
        return
    degrees = _absolute(_degrees_for(amount, unit))
    if degrees == 0:
        return
    generation = _begin_motor_command(ports, "drive-steer-position")
    try:
        drive.steer_angle(
            int(steering), degrees, speed=sign * speed,
            stop=stop_mode, wait=not _in_async_task()
        )
    except Exception:
        if _command_is_current(ports, generation):
            for port in ports:
                _motor_commands.pop(port, None)
        raise
    if _in_async_task():
        return _OperationWait(
            ports, generation, drive.state, drive.STATE_RUNNING,
            drive.STATE_FAULT
        )


def drive_start_steer(steering, speed=None):
    if speed is None:
        speed = _drive_speed
    speed = int(speed)
    _speed_magnitude(speed)
    ports = (_drive_left_port, _drive_right_port)
    generation = _begin_motor_command(ports, "drive-continuous")
    try:
        _get_drive_base().steer(int(steering), speed=speed)
    except Exception:
        if _command_is_current(ports, generation):
            for port in ports:
                _motor_commands.pop(port, None)
        raise


def drive_stop():
    stop_mode = _stop_mode(_drive_stop_action)
    ports = (_drive_left_port, _drive_right_port)
    generation = _begin_motor_command(ports, "drive-stop")
    try:
        _get_drive_base().stop(stop_mode)
    except Exception:
        if _command_is_current(ports, generation):
            for port in ports:
                _motor_commands.pop(port, None)
        raise
    _record_motor_stop(ports, stop_mode)


def color(port):
    return _sensor("color:", est.ColorSensor, port)


def touch(port):
    return _sensor("touch:", est.TouchSensor, port)


def sound(port):
    return _sensor("sound:", est.SoundSensor, port)


def gyro(port):
    return _sensor("gyro:", est.GyroSensor, port)


class _Temperature:
    def __init__(self, port):
        self._sensor = est.TemperatureSensor(port)

    def celsius(self):
        return self._sensor.celsius_tenths() / 10

    def fahrenheit(self):
        return self._sensor.fahrenheit_tenths() / 10


def temperature(port):
    return _sensor("temperature:", _Temperature, port)


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
    _reject_sync_event_wait("display_image_for")
    est.display.image(name)
    est.display.refresh()
    return sleep(seconds)


def stop(scope="all"):
    if scope == "this_stack":
        if _current_task is not None:
            _cancel_task(_current_task)
        raise _StopStack
    if scope == "all":
        est._stop_user_program()
        raise RuntimeError("global program stop returned unexpectedly")
    raise ValueError("stop scope must be this_stack or all")


def stop_other_stacks():
    if _current_task is None:
        raise RuntimeError("stop_other_stacks requires an active task")
    for task in _tasks:
        if task is not _current_task:
            _cancel_task(task)


broadcast = _unsupported("broadcast")
color_calibrate = _unsupported("color_calibrate")
color_reset_calibration = _unsupported("color_reset_calibration")
drive_dual_speed_for = _unsupported("drive_dual_speed_for")
drive_start_dual_speed = _unsupported("drive_start_dual_speed")
ir_beacon_compare = _unsupported("ir_beacon_compare")
on_broadcast = _unsupported("on_broadcast")
on_color = _unsupported("on_color")
on_gyro_angle = _unsupported("on_gyro_angle")
on_ir_beacon_button = _unsupported("on_ir_beacon_button")
on_ir_proximity = _unsupported("on_ir_proximity")
on_touch = _unsupported("on_touch")
on_ultrasonic = _unsupported("on_ultrasonic")
wait_brick_button = _unsupported("wait_brick_button")
wait_color = _unsupported("wait_color")
wait_gyro = _unsupported("wait_gyro")
wait_ir_beacon_button = _unsupported("wait_ir_beacon_button")
wait_ir_proximity = _unsupported("wait_ir_proximity")
wait_touch = _unsupported("wait_touch")
wait_ultrasonic = _unsupported("wait_ultrasonic")
