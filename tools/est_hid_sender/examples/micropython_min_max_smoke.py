import est


assert min(7, 2, 9) == 2
assert max(7, 2, 9) == 9
assert min((4, 1, 6)) == 1
assert max((4, 1, 6)) == 6

value = 37.5
limited = max(-25.0, min(value, 25.0))
assert limited == 25.0

value = -37.5
limited = max(-25.0, min(value, 25.0))
assert limited == -25.0

est._program_result(122)
