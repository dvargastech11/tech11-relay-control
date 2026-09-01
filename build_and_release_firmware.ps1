<#
.SYNOPSIS
    Builds, versions, and publishes the Tech 11 relay module firmware.

.DESCRIPTION
    Versioning scheme: main.overhaul.small
      - main:     a fundamentally new generation of the firmware
      - overhaul: a significant restructuring/rework of existing behavior
      - small:    routine fixes and minor changes (the default bump)

    - Reads the current version from firmware/version.txt (starts at
      1.0.0 if it doesn't exist yet)
    - Bumps ONE part per the -Bump parameter (default: small), resetting
      the parts below it to 0 (e.g. an overhaul bump resets small to 0)
    - Updates CURRENT_FIRMWARE_VERSION in config.h to match
    - Compiles the sketch via arduino-cli
    - Copies the compiled .bin to firmware/firmware.bin (the exact path the
      ESP32's own GitHub OTA check downloads from)
    - Archives a timestamped copy under firmware/releases/ for history
    - Updates firmware/version.txt
    - Commits and pushes everything to GitHub

.PARAMETER Bump
    Which part of main.overhaul.small to increment. Default: small.

.PARAMETER Version
    Optional. Set an explicit version (e.g. "2.0.0") instead of bumping.

.EXAMPLE
    .\build_and_release_firmware.ps1
    .\build_and_release_firmware.ps1 -Bump overhaul
    .\build_and_release_firmware.ps1 -Bump main
    .\build_and_release_firmware.ps1 -Version "2.0.0"
#>

param(
    [ValidateSet("main", "overhaul", "small")]
    [string]$Bump = "small",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot      = $PSScriptRoot
$SketchDir     = Join-Path $RepoRoot "firmware\tech11_relay_module"
$ConfigFile    = Join-Path $SketchDir "config.h"
$VersionFile   = Join-Path $RepoRoot "firmware\version.txt"
$OutputBin     = Join-Path $RepoRoot "firmware\firmware.bin"
$ReleasesDir   = Join-Path $RepoRoot "firmware\releases"
$BuildDir      = Join-Path $env:TEMP "tech11_firmware_build"
$Fqbn          = "esp32:esp32:esp32:PartitionScheme=min_spiffs"

function Write-Step($msg) {
    Write-Host "`n=== $msg ===" -ForegroundColor Cyan
}

# ============================================================
# 1. Determine the new version number
# ============================================================
Write-Step "Determining version"

if ($Version -ne "") {
    $newVersion = $Version
    Write-Host "Using explicit version: $newVersion" -ForegroundColor Yellow
} else {
    if (Test-Path $VersionFile) {
        $currentVersion = (Get-Content $VersionFile -Raw).Trim()
    } else {
        $currentVersion = "1.0.0"
    }

    $parts = $currentVersion.Split(".")
    # Tolerate an old-style two-part version (e.g. "1.5") left over from
    # before this scheme - treat the missing third part as 0.
    $mainPart     = [int]$parts[0]
    $overhaulPart = if ($parts.Count -gt 1) { [int]$parts[1] } else { 0 }
    $smallPart    = if ($parts.Count -gt 2) { [int]$parts[2] } else { 0 }

    switch ($Bump) {
        "main"     { $mainPart++; $overhaulPart = 0; $smallPart = 0 }
        "overhaul" { $overhaulPart++; $smallPart = 0 }
        "small"    { $smallPart++ }
    }

    $newVersion = "$mainPart.$overhaulPart.$smallPart"
    Write-Host "Bumping [$Bump]: $currentVersion -> $newVersion" -ForegroundColor Yellow
}

# ============================================================
# 2. Update CURRENT_FIRMWARE_VERSION in config.h
# ============================================================
Write-Step "Updating config.h to version $newVersion"

$configContent = Get-Content $ConfigFile -Raw
$configContent = $configContent -replace '#define CURRENT_FIRMWARE_VERSION "[^"]*"', "#define CURRENT_FIRMWARE_VERSION `"$newVersion`""
Set-Content -Path $ConfigFile -Value $configContent -NoNewline

Write-Host "config.h updated." -ForegroundColor Green

# ============================================================
# 3. Compile
# ============================================================
Write-Step "Compiling firmware (version $newVersion)"

if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force }
New-Item -ItemType Directory -Path $BuildDir | Out-Null

arduino-cli compile --fqbn $Fqbn --output-dir $BuildDir $SketchDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Compilation failed. Version was NOT published." -ForegroundColor Red
    exit 1
}

$compiledBin = Join-Path $BuildDir "tech11_relay_module.ino.bin"
if (-not (Test-Path $compiledBin)) {
    Write-Host "ERROR: Expected compiled binary not found at $compiledBin" -ForegroundColor Red
    exit 1
}

Write-Host "Compiled successfully." -ForegroundColor Green

# ============================================================
# 4. Publish the .bin (this is what the ESP32's GitHub OTA downloads)
# ============================================================
Write-Step "Publishing firmware.bin"

Copy-Item $compiledBin $OutputBin -Force

# Archive a timestamped copy for history/rollback reference
New-Item -ItemType Directory -Path $ReleasesDir -Force | Out-Null
$archiveName = "firmware_v$newVersion.bin"
Copy-Item $compiledBin (Join-Path $ReleasesDir $archiveName) -Force

Write-Host "Published to firmware\firmware.bin and archived as firmware\releases\$archiveName" -ForegroundColor Green

# ============================================================
# 5. Update version.txt
# ============================================================
Set-Content -Path $VersionFile -Value $newVersion -NoNewline

# ============================================================
# 6. Commit and push
# ============================================================
Write-Step "Committing and pushing to GitHub"

Push-Location $RepoRoot
git add "firmware/version.txt" "firmware/firmware.bin" "firmware/releases/$archiveName" "firmware/tech11_relay_module/config.h"
git commit -m "Release firmware v$newVersion"
git push origin main
Pop-Location

Write-Step "Done - firmware v$newVersion published"
Write-Host "ESP32 devices will detect this on their next GitHub OTA check (or 'Check for Updates' button)." -ForegroundColor Cyan
