<#
.SYNOPSIS
    Compiles the Tech 11 relay module firmware once, then flashes it to
    an unlimited number of ESP32 devices, one at a time - auto-detecting
    each device's COM port rather than requiring you to look it up and
    type it manually every time.

.DESCRIPTION
    Every USB flash does a FULL FLASH ERASE first (EraseFlash=all), wiping
    NVS along with the code - this guarantees a true factory reset on every
    manual flash, so no device can end up running with stale WiFi
    credentials, an old static IP, or an old device name left over from
    before. OTA updates (GitHub self-check or the website's push-firmware
    feature) do NOT erase NVS - only manual USB flashing does.

    Runs indefinitely - keep connecting devices one after another. Type
    'q' at any "connect the next device" prompt to stop.

.EXAMPLE
    .\flash_batch.ps1
#>

$SketchDir   = Join-Path $PSScriptRoot "firmware\tech11_relay_module"
$BuildDir    = ".\build_output"
$CompileFqbn = "esp32:esp32:esp32:PartitionScheme=min_spiffs"
$UploadFqbn  = "esp32:esp32:esp32:PartitionScheme=min_spiffs,EraseFlash=all"

function Get-ComPorts {
    [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
}

Write-Host "`n=== Compiling firmware (once) ===" -ForegroundColor Cyan
arduino-cli compile --fqbn $CompileFqbn --output-dir $BuildDir $SketchDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed - aborting before flashing anything." -ForegroundColor Red
    exit 1
}

Write-Host "`nCompiled successfully." -ForegroundColor Green
Write-Host "Each flash does a FULL erase first (factory reset - wipes old WiFi/network config)." -ForegroundColor Yellow
Write-Host "Type 'q' at any prompt below to stop.`n" -ForegroundColor Yellow

$deviceNum = 0

while ($true) {
    $deviceNum++
    Write-Host "=== Device $deviceNum ===" -ForegroundColor Cyan

    $beforePorts = Get-ComPorts
    $response = Read-Host "Connect the next ESP32 via USB, then press Enter (or type 'q' to quit)"

    if ($response -eq "q") {
        break
    }

    # Give Windows a moment to finish enumerating the newly connected device
    Start-Sleep -Seconds 2
    $afterPorts = Get-ComPorts
    $newPorts = $afterPorts | Where-Object { $beforePorts -notcontains $_ }

    $port = $null
    if ($newPorts.Count -eq 1) {
        $port = $newPorts[0]
        Write-Host "Auto-detected device on $port" -ForegroundColor Green
    } elseif ($newPorts.Count -gt 1) {
        Write-Host "Multiple new ports appeared: $($newPorts -join ', ')" -ForegroundColor Yellow
        $port = Read-Host "Type which one to use"
    } else {
        Write-Host "Couldn't auto-detect a new port (maybe it was already plugged in, or drivers are still loading)." -ForegroundColor Yellow
        $port = Read-Host "Enter the COM port manually (e.g. COM6), or 'q' to quit"
        if ($port -eq "q") { break }
    }

    if ([string]::IsNullOrWhiteSpace($port)) {
        Write-Host "No port given - skipping this device.`n" -ForegroundColor Yellow
        $deviceNum--
        continue
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

Write-Host "`n=== Done - flashed $($deviceNum - 1) device(s) this session ===" -ForegroundColor Cyan
