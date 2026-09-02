<#
.SYNOPSIS
    Compiles the Tech 11 relay module firmware once, then flashes it to
    an unlimited number of ESP32 devices, one at a time - identifying the
    ESP32's COM port automatically by its actual USB device identity
    (CP210x or CH340 USB-to-serial chip, the two used on ESP32 dev
    boards), not just by watching for any new port to appear.

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

# Known ESP32 dev board USB-to-serial chip identifiers - matched against
# each COM port's PnP device description.
$EspChipPatterns = @("CP210", "Silicon Labs", "CH340", "CH341", "USB-SERIAL", "USB-Enhanced-SERIAL")

function Find-EspPorts {
    $results = @()
    $pnpDevices = Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction SilentlyContinue
    foreach ($device in $pnpDevices) {
        if ($device.Name -match "\((COM\d+)\)") {
            $comPort = $Matches[1]
            foreach ($pattern in $EspChipPatterns) {
                if ($device.Name -like "*$pattern*") {
                    $results += [PSCustomObject]@{ Port = $comPort; Description = $device.Name }
                    break
                }
            }
        }
    }
    return $results
}

Write-Host "`n=== Compiling firmware (once) ===" -ForegroundColor Cyan
arduino-cli compile --fqbn $CompileFqbn --output-dir $BuildDir $SketchDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed - aborting before flashing anything." -ForegroundColor Red
    exit 1
}

Write-Host "`nCompiled successfully." -ForegroundColor Green
Write-Host "`nLooking for an ESP32 (CP210x/CH340 USB-serial chip)..." -ForegroundColor Yellow

$found = Find-EspPorts
$port = $null

if ($found.Count -eq 1) {
    $port = $found[0].Port
    Write-Host "Found: $($found[0].Description)" -ForegroundColor Green
} elseif ($found.Count -gt 1) {
    Write-Host "Multiple matching devices found:" -ForegroundColor Yellow
    $found | ForEach-Object { Write-Host "  $($_.Port): $($_.Description)" }
    $port = Read-Host "Type which port to use"
} else {
    Write-Host "No ESP32-like device detected automatically." -ForegroundColor Yellow
    $port = Read-Host "Enter the COM port manually (e.g. COM6)"
}

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
