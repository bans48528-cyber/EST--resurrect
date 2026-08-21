param(
    [string]$MakeCommand = "make",
    [string]$PythonCommand = "python",
    [string]$BuildVersion = "M0.18A"
)

$ErrorActionPreference = "Stop"

foreach ($command in @(
    "arm-none-eabi-gcc",
    "arm-none-eabi-objcopy",
    "arm-none-eabi-objdump",
    "arm-none-eabi-size",
    $MakeCommand,
    $PythonCommand
)) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "Required command not found in PATH: $command"
    }
}

$project = Join-Path $PSScriptRoot "..\firmware\minimal_upgrade_app"
Push-Location $project
try {
    & $MakeCommand "PYTHON=$PythonCommand" test
    if ($LASTEXITCODE -ne 0) {
        throw "Unit tests failed"
    }

    & $MakeCommand -j4 "BUILD_DIR=build/migration_check" `
        "APP_VERSION=$BuildVersion" "PYTHON=$PythonCommand" all
    if ($LASTEXITCODE -ne 0) {
        throw "Firmware build failed"
    }
} finally {
    Pop-Location
}

Write-Host "Toolchain verification passed for $BuildVersion"

