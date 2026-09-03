"""Bounded, muted, read-only decoder diagnostics; never commands a motor."""
import json

from .cli import open_transport, parse_args
from .updater import FirmwareUpdater


def main():
    checks = [("pin_layout", "est.audio._diagnostics()[11]", 0.03),
              ("pin_probe", "est.audio._diagnostics()[12]", 0.03)]
    checks += [("SCI_" + str(reg), "est.audio._read_register(" + str(reg) + ")", 0.3)
              for reg in (0, 1, 3, 5, 9, 11)]
    checks += [("payload_crc31", "est.audio._diagnostics()[9] & 0x7fffffff", 1.9),
               ("bytes_sent", "est.audio._diagnostics()[2]", 0.3),
               ("dreq_waits", "est.audio._diagnostics()[10]", 0.3)]
    with open_transport(parse_args(["device-status"])) as transport:
        updater = FirmwareUpdater(transport)
        for label, expression, delay in checks:
            source = (
                "import est\nimport est_runtime as rt\n"
                "est.audio.set_volume(0)\ntry:\n"
                " est.audio.play('Piano/C4')\n"
                f" rt.sleep({delay})\n value = {expression}\n"
                " est._program_result(-1 if value is None else value)\n"
                "finally:\n est.audio.stop()\n"
            ).encode()
            status = updater.run_python_program(source, timeout_ms=3000)
            print(json.dumps({"check": label, "state": status.state,
                              "error": status.error, "value": status.result_value}), flush=True)
            if status.state != 5:
                break
        else:
            source = (
                "import est\nimport est_runtime as rt\n"
                "est.audio.set_volume(0)\ntry:\n"
                " est.audio.play('Piano/C4')\n rt.sleep(0.3)\n"
                " est.audio.set_volume(0)\n rt.sleep(0.02)\n"
                " value=est.audio._read_register(11)\n"
                " est._program_result(-1 if value is None else value)\n"
                "finally:\n est.audio.stop()\n"
            ).encode()
            status = updater.run_python_program(source, timeout_ms=3000)
            print(json.dumps({"check": "volume_after_sdi_finished", "state": status.state,
                              "error": status.error, "value": status.result_value}), flush=True)
            source = (
                "import est\nimport est_runtime as rt\n"
                "est.audio.set_volume(0)\nflags=0\ntry:\n"
                " est.audio.play('Piano/C4')\n"
                " for i in range(100):\n"
                "  status=est.audio._diagnostics()\n"
                "  if status[3] and status[2] == 0:\n"
                "   if est.audio._read_register(3) == 0x9800:\n    flags += 1\n"
                "   if est.audio._read_register(11) == 0xFEFE:\n    flags += 2\n"
                "   break\n"
                "  rt.sleep(0.001)\n"
                " rt.sleep(0.3)\n"
                " if est.audio._read_register(3) == 0:\n  flags += 4\n"
                " if est.audio._read_register(11) == 0:\n  flags += 8\n"
                " est._program_result(flags)\n"
                "finally:\n est.audio.stop()\n"
            ).encode()
            status = updater.run_python_program(source, timeout_ms=3000)
            print(json.dumps({"check": "registers_before_and_after_sdi", "state": status.state,
                              "error": status.error, "value": status.result_value}), flush=True)


if __name__ == "__main__":
    main()
