<#
.SYNOPSIS
    Sets up the Arduino/ESP32 development environment on Windows for the
    Tech 11 Relay Control System firmware.

.DESCRIPTION
    - Installs Arduino CLI (scriptable, avoids GUI-only setup steps)
    - Adds the ESP32 board index and installs a STABLE core version
      (explicitly pinned - NOT the alpha/latest, which caused mbedtls
      compile errors earlier)
    - Installs the ArduinoJson library
    - Clones (or updates) the firmware repo

.NOTES
    Safe to re-run - each step checks whether it's already done.
    Does NOT require Administrator privileges (installs to user profile).
#>

# ============================================================
# CONFIG - adjust if needed
# ============================================================
$RepoUrl        = "https://github.com/dvargastech11/tech11-relay-control.git"
$RepoDir        = "$env:USERPROFILE\tech11-relay-control"
$ArduinoCliDir  = "$env:USERPROFILE\arduino-cli-bin"
$Esp32CoreVersion = "3.0.7"   # STABLE - do not change to "latest" without checking for alpha releases first
$Esp32BoardIndexUrl = "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

function Write-Step($msg) {
    Write-Host "`n=== $msg ===" -ForegroundColor Cyan
}

function Test-CommandExists($name) {
    return $null -ne (Get-Command $name -ErrorAction SilentlyContinue)
}

# ============================================================
# 1. Install Arduino CLI
# ============================================================
Write-Step "Checking Arduino CLI"

if (Test-CommandExists "arduino-cli") {
    Write-Host "Arduino CLI already installed: $(arduino-cli version)" -ForegroundColor Green
} else {
    Write-Host "Installing Arduino CLI..." -ForegroundColor Yellow

    New-Item -ItemType Directory -Path $ArduinoCliDir -Force | Out-Null
    $cliZip = "$env:TEMP\arduino-cli.zip"
    Invoke-WebRequest -Uri "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip" -OutFile $cliZip
    Expand-Archive -Path $cliZip -DestinationPath $ArduinoCliDir -Force
    Remove-Item $cliZip -ErrorAction SilentlyContinue

    # Add to PATH for this session and permanently for this user
    $env:Path = "$ArduinoCliDir;$env:Path"
    $currentUserPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
    if ($currentUserPath -notlike "*$ArduinoCliDir*") {
        [System.Environment]::SetEnvironmentVariable("Path", "$currentUserPath;$ArduinoCliDir", "User")
    }

    if (Test-CommandExists "arduino-cli") {
        Write-Host "Arduino CLI installed: $(arduino-cli version)" -ForegroundColor Green
    } else {
        Write-Host "ERROR: Arduino CLI installation failed. Check $ArduinoCliDir manually." -ForegroundColor Red
        exit 1
    }
}

# ============================================================
# 2. Initialize config and add ESP32 board index
# ============================================================
Write-Step "Configuring Arduino CLI"

arduino-cli config init --overwrite | Out-Null
arduino-cli config add board_manager.additional_urls $Esp32BoardIndexUrl
arduino-cli core update-index

# ============================================================
# 3. Install the STABLE ESP32 core (explicitly pinned version)
# ============================================================
Write-Step "Installing ESP32 core (stable, pinned to $Esp32CoreVersion)"

$installedCores = arduino-cli core list 2>&1
if ($installedCores -match "esp32:esp32\s+$Esp32CoreVersion") {
    Write-Host "ESP32 core $Esp32CoreVersion already installed." -ForegroundColor Green
} else {
    Write-Host "Installing esp32:esp32@$Esp32CoreVersion - this can take several minutes..." -ForegroundColor Yellow
    arduino-cli core install "esp32:esp32@$Esp32CoreVersion"
}

Write-Host "`nIMPORTANT: If Arduino IDE (the GUI) is also installed on this machine," -ForegroundColor Yellow
Write-Host "open Tools > Board > Boards Manager and confirm 'esp32 by Espressif Systems'" -ForegroundColor Yellow
Write-Host "is set to version $Esp32CoreVersion, NOT an alpha/pre-release version." -ForegroundColor Yellow

# ============================================================
# 4. Install required libraries
# ============================================================
Write-Step "Installing required libraries"

arduino-cli lib install "ArduinoJson" "Adafruit SH110X" "Adafruit GFX Library"

# ============================================================
# 5. Clone or update the firmware repo
# ============================================================
Write-Step "Setting up firmware repo at $RepoDir"

if (-not (Test-CommandExists "git")) {
    Write-Host "ERROR: git is not installed. Install Git for Windows first (git-scm.com), then re-run this script." -ForegroundColor Red
    exit 1
}

if (Test-Path "$RepoDir\.git") {
    Write-Host "Repo already cloned - pulling latest..." -ForegroundColor Yellow
    Push-Location $RepoDir
    git pull
    Pop-Location
} else {
    git clone $RepoUrl $RepoDir
}

# ============================================================
# Done
# ============================================================
Write-Step "Setup complete"
Write-Host "Firmware sketch: $RepoDir\firmware\tech11_relay_module\tech11_relay_module.ino" -ForegroundColor Cyan
Write-Host "`nTo compile from the command line:" -ForegroundColor Cyan
Write-Host "  arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs `"$RepoDir\firmware\tech11_relay_module`"" -ForegroundColor White
Write-Host "`nTo upload (replace COM3 with your actual port):" -ForegroundColor Cyan
Write-Host "  arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs `"$RepoDir\firmware\tech11_relay_module`"" -ForegroundColor White
Write-Host "`nOr open the .ino file above directly in Arduino IDE if you prefer the GUI." -ForegroundColor Cyan
