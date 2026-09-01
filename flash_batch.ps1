<#
.SYNOPSIS
    Compiles the Tech 11 relay module firmware once, then flashes it to
    multiple ESP32 devices sequentially - one physical device at a time,
    prompting for the COM port each round (since it can vary depending on
    which USB port/hub position is used).

    Every USB flash does a FULL FLASH ERASE first (EraseFlash=all), wiping
    NVS along with the code - this guarantees a true factory reset on every
    manual flash, so no device can end up running with stale WiFi
    credentials, an old static IP, or an old device name left over from
    before. OTA updates (GitHub self-check or the website's push-firmware
    feature) do NOT erase NVS - only manual USB flashing does.

.EXAMPLE
    .\flash_batch.ps1
    .\flash_batch.ps1 -DeviceCount 12
#>

param(
    [int]$DeviceCount = 12
)

$SketchDir  = Join-Path $PSScriptRoot "firmware\tech11_relay_module"
$BuildDir   = ".\build_output"
$CompileFqbn = "esp32:esp32:esp32:PartitionScheme=min_spiffs"
$UploadFqbn  = "esp32:esp32:esp32:PartitionScheme=min_spiffs,EraseFlash=all"

Write-Host "`n=== Compiling firmware (once) ===" -ForegroundColor Cyan
arduino-cli compile --fqbn $CompileFqbn --output-dir $BuildDir $SketchDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed - aborting before flashing anything." -ForegroundColor Red
    exit 1
}

Write-Host "`nCompiled successfully. Ready to flash $DeviceCount device(s)." -ForegroundColor Green
Write-Host "Each flash does a FULL erase first (factory reset - wipes old WiFi/network config)." -ForegroundColor Yellow
Write-Host ""

for ($i = 1; $i -le $DeviceCount; $i++) {
    Write-Host "=== Device $i of $DeviceCount ===" -ForegroundColor Cyan
    Write-Host "Connect the next ESP32 via USB, then check Device Manager for its COM port." -ForegroundColor Yellow
    $port = Read-Host "Enter COM port for this device (e.g. COM6), or type 'skip' to skip it"

    if ($port -eq "skip") {
        Write-Host "Skipped device $i.`n" -ForegroundColor Yellow
        continue
    }

    arduino-cli upload -p $port --fqbn $UploadFqbn --input-dir $BuildDir $SketchDir

    if ($LASTEXITCODE -eq 0) {
        Write-Host "Device $i flashed and factory-reset successfully on $port.`n" -ForegroundColor Green
    } else {
        Write-Host "Device $i FAILED to flash on $port - check the connection and try again." -ForegroundColor Red
        $retry = Read-Host "Retry this device? (y/n)"
        if ($retry -eq "y") { $i-- }
    }
}

Write-Host "`n=== Batch flashing complete ===" -ForegroundColor Cyan
