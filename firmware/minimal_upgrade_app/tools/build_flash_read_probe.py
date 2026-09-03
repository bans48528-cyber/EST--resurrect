"""Link a read-only probe using the preserved, accepted M1.22Y objects."""

import hashlib
import subprocess
import sys
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[1]
    baseline = root / "build/m122y_pair_pi_candidate"
    package = baseline / "est_minimal_upgrade_app.upgrade.bin"
    expected = "16dbbd31e53eb7ae20dc0db03d05b547b9b5021b80f0903a30b641ef0e14cb2f"
    if hashlib.sha256(package.read_bytes()).hexdigest() != expected:
        raise RuntimeError("M1.22Y baseline package has changed")
    output = root / "build/m122z_flash_read_probe"
    output.mkdir(parents=True, exist_ok=True)
    prefix = "D:/codex-arm-gcc-12/bin/arm-none-eabi"
    arch = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]
    def run(*args):
        subprocess.run([str(arg) for arg in args], cwd=root, check=True)
    replacements = []
    for name in ("update_protocol", "app_version"):
        target = output / (name + ".o")
        run(prefix + "-gcc", *arch, "-std=c11", "-Os", "-g3", "-Wall", "-Wextra",
            "-Werror", "-ffunction-sections", "-fdata-sections", "-DSTM32F4",
            "-DAUDIO_RESOURCE_PROBE_ONLY=1", '-DAPP_VERSION_TEXT="M1.22Z"',
            "-Iinclude", "-Ithird_party/libopencm3/include", "-c", "src/" + name + ".c",
            "-o", target)
        replacements.append(target)
    objects = sorted(path for path in (baseline / "obj").glob("*.o")
                     if path.stem not in ("update_protocol", "app_version"))
    stem = "est_minimal_upgrade_app"
    elf = output / (stem + ".elf")
    binary = output / (stem + ".bin")
    run(prefix + "-gcc", *arch, *objects, *replacements,
        baseline / "micropython/libest_micropython.a", "-Tmemory.ld",
        "-Lthird_party/libopencm3/lib", "-nostartfiles", "-Wl,--gc-sections",
        "-Wl,-Map=" + str(output / (stem + ".map")), "-lopencm3_stm32f4",
        "-Wl,--start-group", "-lc", "-lm", "-lgcc", "-lnosys", "-Wl,--end-group",
        "-o", elf)
    run(prefix + "-objcopy", "-O", "binary", elf, binary)
    run(prefix + "-size", elf)
    run(sys.executable, "tools/package_firmware.py", "--input", binary,
        "--output-dir", output, "--stem", stem, "--version", "M1.22Z",
        "--package-size", "327680")
    run(sys.executable, "tools/verify_build.py", "--objdump", prefix + "-objdump",
        "--elf", elf, "--bin", binary, "--app-package", output / (stem + ".app.bin"),
        "--upgrade-package", output / (stem + ".upgrade.bin"), "--manifest",
        output / (stem + ".manifest.json"), "--version", "M1.22Z")


if __name__ == "__main__":
    main()
