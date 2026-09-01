<#
.SYNOPSIS
    Compiles the Tech 11 relay module firmware once, then flashes it to
    an unlimited number of ESP32 devices, one at a time, using the SAME
    COM port for every device (asked once up front) - for the common
    workflow of swapping boards in/out of the same USB connector rather
    than plugging into different physical ports each time.

.DESCRIPTION
    Every USB flash does a FULL FLASH ERASE first (EraseFlash=all), wiping
    NVS along with the code - this guarantees a true factory reset on every
    manual flash, so no device can end up running with stale WiFi
    credentials, an old static IP, or an old device name left over from
    before. OTA updates (GitHub self-check or the website's push-firmware
    feature) do NOT erase NVS - only manual USB flashing does.

    Runs indefinitely - keep swapping devices in on the same port. Type
    'q' at any "connect the next device" prompt to stop.

.EXAMPLE
    .\flash_batch.ps1
#>

$SketchDir   = Join-Path $PSScriptRoot "firmware\tech11_relay_module"
$BuildDir    = ".\build_output"
$CompileFqbn = "esp32:esp32:esp32:PartitionScheme=min_spiffs"
$UploadFqbn  = "esp32:esp32:esp32:PartitionScheme=min_spiffs,EraseFlash=all"

Write-Host "`n=== Compiling firmware (once) ===" -ForegroundColor Cyan
arduino-cli compile --fqbn $CompileFqbn --output-dir $BuildDir $SketchDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed - aborting before flashing anything." -ForegroundColor Red
    exit 1
}

Write-Host "`nCompiled successfully." -ForegroundColor Green

$port = Read-Host "`nEnter the COM port to use for all devices (e.g. COM6)"
if ([string]::IsNullOrWhiteSpace($port)) {
    Write-Host "No port given - aborting." -ForegroundColor Red
    exit 1
}

Write-Host "Using $port for every device." -ForegroundColor Green
Write-Host "Each flash does a FULL erase first (factory reset - wipes old WiFi/network config)." -ForegroundColor Yellow
Write-Host "Type 'q' at any prompt below to stop.`n" -ForegroundColor Yellow

$deviceNum = 0

while ($true) {
    $deviceNum++
    Write-Host "=== Device $deviceNum ($port) ===" -ForegroundColor Cyan
    $response = Read-Host "Connect the next ESP32 on $port, then press Enter (or type 'q' to quit)"

    if ($response -eq "q") {
        break
    }

    arduino-cli upload -p $port --fqbn $UploadFqbn --input-dir $BuildDir $SketchDir

    if ($LASTEXITCODE -eq 0) {
        Write-Host "Device $deviceNum flashed and factory-reset successfully on $port.`n" -ForegroundColor Green
    } else {
        Write-Host "Device $deviceNum FAILED to flash on $port." -ForegroundColor Red
        $retry = Read-Host "Retry this device? (y/n)"
        if ($retry -eq "y") { $deviceNum-- }
        else { Write-Host "" }
    }
}

Write-Host "`n=== Done - flashed $($deviceNum - 1) device(s) this session on $port ===" -ForegroundColor Cyan
