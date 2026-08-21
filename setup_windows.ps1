<#
.SYNOPSIS
    Automates the full setup of the Tech 11 Relay Control System Flask server
    on Windows Server, from a fresh machine through to a running Windows Service.

.DESCRIPTION
    - Installs Python 3.13 and Git (via winget if available, direct download otherwise)
    - Clones the repo (or pulls latest if already present)
    - Creates a virtual environment and installs dependencies
    - Installs NSSM and registers the Flask app as a Windows Service
    - Opens the firewall port
    - Starts the service

.NOTES
    Must be run as Administrator. Re-running this script is safe - each step
    checks whether it's already done before acting.
#>

# ============================================================
# CONFIG - adjust these if your setup differs
# ============================================================
$RepoUrl        = "https://github.com/dvargastech11/tech11-relay-control.git"
$InstallDir     = "C:\tech11-relay-control"
$FlaskDir       = "$InstallDir\flask-server"
$ServiceName    = "Tech11RelayServer"
$ServicePort    = 5000
$PythonWingetId = "Python.Python.3.13"
$GitWingetId    = "Git.Git"
$NssmUrl        = "https://nssm.cc/release/nssm-2.24.zip"
$NssmDir        = "C:\nssm"

# ============================================================
# 0. Require Administrator
# ============================================================
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "ERROR: This script must be run as Administrator." -ForegroundColor Red
    Write-Host "Right-click PowerShell and choose 'Run as Administrator', then re-run this script." -ForegroundColor Red
    exit 1
}

function Write-Step($msg) {
    Write-Host "`n=== $msg ===" -ForegroundColor Cyan
}

function Test-CommandExists($name) {
    return $null -ne (Get-Command $name -ErrorAction SilentlyContinue)
}

# ============================================================
# 1. Install Python (if missing)
# ============================================================
Write-Step "Checking Python installation"

if (Test-CommandExists "python") {
    $pyVersion = python --version 2>&1
    Write-Host "Python already installed: $pyVersion" -ForegroundColor Green
} else {
    Write-Host "Python not found. Installing..." -ForegroundColor Yellow

    if (Test-CommandExists "winget") {
        winget install --id $PythonWingetId -e --silent --accept-package-agreements --accept-source-agreements
    } else {
        Write-Host "winget not available - falling back to direct download." -ForegroundColor Yellow
        $pyInstaller = "$env:TEMP\python-installer.exe"
        # NOTE: update this URL if a newer 3.13.x patch release exists
        Invoke-WebRequest -Uri "https://www.python.org/ftp/python/3.13.1/python-3.13.1-amd64.exe" -OutFile $pyInstaller
        Start-Process -FilePath $pyInstaller -ArgumentList "/quiet InstallAllUsers=1 PrependPath=1" -Wait
        Remove-Item $pyInstaller -ErrorAction SilentlyContinue
    }

    # Refresh PATH in this session so subsequent commands see the new install
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

    if (Test-CommandExists "python") {
        Write-Host "Python installed successfully: $(python --version)" -ForegroundColor Green
    } else {
        Write-Host "ERROR: Python installation did not complete correctly. Install manually and re-run this script." -ForegroundColor Red
        exit 1
    }
}

# ============================================================
# 2. Install Git (if missing)
# ============================================================
Write-Step "Checking Git installation"

if (Test-CommandExists "git") {
    Write-Host "Git already installed: $(git --version)" -ForegroundColor Green
} else {
    Write-Host "Git not found. Installing..." -ForegroundColor Yellow

    if (Test-CommandExists "winget") {
        winget install --id $GitWingetId -e --silent --accept-package-agreements --accept-source-agreements
    } else {
        Write-Host "winget not available - falling back to direct download." -ForegroundColor Yellow
        $gitInstaller = "$env:TEMP\git-installer.exe"
        Invoke-WebRequest -Uri "https://github.com/git-for-windows/git/releases/latest/download/Git-64-bit.exe" -OutFile $gitInstaller
        Start-Process -FilePath $gitInstaller -ArgumentList "/VERYSILENT /NORESTART" -Wait
        Remove-Item $gitInstaller -ErrorAction SilentlyContinue
    }

    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

    if (Test-CommandExists "git") {
        Write-Host "Git installed successfully: $(git --version)" -ForegroundColor Green
    } else {
        Write-Host "ERROR: Git installation did not complete correctly. Install manually and re-run this script." -ForegroundColor Red
        exit 1
    }
}

# ============================================================
# 3. Clone or update the repository
# ============================================================
Write-Step "Setting up repository at $InstallDir"

if (Test-Path "$InstallDir\.git") {
    Write-Host "Repo already cloned - pulling latest changes..." -ForegroundColor Yellow
    Push-Location $InstallDir
    git pull
    Pop-Location
} else {
    if (Test-Path $InstallDir) {
        Write-Host "ERROR: $InstallDir exists but is not a git repo. Remove or rename it, then re-run this script." -ForegroundColor Red
        exit 1
    }
    git clone $RepoUrl $InstallDir
}

if (-not (Test-Path $FlaskDir)) {
    Write-Host "ERROR: Expected folder not found: $FlaskDir" -ForegroundColor Red
    exit 1
}

# ============================================================
# 4. Create virtual environment and install dependencies
# ============================================================
Write-Step "Setting up Python virtual environment"

Push-Location $FlaskDir

if (-not (Test-Path "venv")) {
    python -m venv venv
}

& ".\venv\Scripts\python.exe" -m pip install --upgrade pip --quiet
& ".\venv\Scripts\python.exe" -m pip install flask requests flask-login werkzeug waitress psutil --quiet

Write-Host "Dependencies installed." -ForegroundColor Green
Pop-Location

# ============================================================
# 5. Install NSSM
# ============================================================
Write-Step "Setting up NSSM (service wrapper)"

if (Test-CommandExists "nssm") {
    Write-Host "NSSM already available on PATH." -ForegroundColor Green
} else {
    Write-Host "Downloading NSSM..." -ForegroundColor Yellow
    $nssmZip = "$env:TEMP\nssm.zip"
    Invoke-WebRequest -Uri $NssmUrl -OutFile $nssmZip
    Expand-Archive -Path $nssmZip -DestinationPath $NssmDir -Force
    Remove-Item $nssmZip -ErrorAction SilentlyContinue

    $nssmExe = Get-ChildItem -Path $NssmDir -Recurse -Filter "nssm.exe" | Where-Object { $_.DirectoryName -like "*win64*" } | Select-Object -First 1
    if (-not $nssmExe) {
        Write-Host "ERROR: Could not locate nssm.exe after extraction. Check $NssmDir manually." -ForegroundColor Red
        exit 1
    }
    Copy-Item $nssmExe.FullName -Destination "C:\Windows\System32\nssm.exe" -Force
    Write-Host "NSSM installed to System32." -ForegroundColor Green
}

# ============================================================
# 6. Register the Windows Service
# ============================================================
Write-Step "Registering Windows Service: $ServiceName"

$existingService = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($existingService) {
    Write-Host "Service '$ServiceName' already exists - stopping before reconfiguring." -ForegroundColor Yellow
    nssm stop $ServiceName
    nssm remove $ServiceName confirm
}

$pythonExe = "$FlaskDir\venv\Scripts\python.exe"
nssm install $ServiceName $pythonExe "run_production.py"
nssm set $ServiceName AppDirectory $FlaskDir
nssm set $ServiceName AppExit Default Restart
nssm set $ServiceName Start SERVICE_AUTO_START

Write-Host "Service registered." -ForegroundColor Green

# ============================================================
# 7. Open firewall port
# ============================================================
Write-Step "Configuring firewall"

$existingRule = Get-NetFirewallRule -DisplayName "Tech11 Relay Server" -ErrorAction SilentlyContinue
if (-not $existingRule) {
    New-NetFirewallRule -DisplayName "Tech11 Relay Server" -Direction Inbound -LocalPort $ServicePort -Protocol TCP -Action Allow | Out-Null
    Write-Host "Firewall rule created for port $ServicePort." -ForegroundColor Green
} else {
    Write-Host "Firewall rule already exists." -ForegroundColor Green
}

# ============================================================
# 8. Start the service
# ============================================================
Write-Step "Starting service"

nssm start $ServiceName
Start-Sleep -Seconds 2

$status = Get-Service -Name $ServiceName
Write-Host "Service status: $($status.Status)" -ForegroundColor $(if ($status.Status -eq "Running") { "Green" } else { "Red" })

# ============================================================
# Done
# ============================================================
Write-Step "Setup complete"
$localIp = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -notlike "*Loopback*" } | Select-Object -First 1).IPAddress
Write-Host "Visit: http://localhost:$ServicePort" -ForegroundColor Cyan
Write-Host "Or from another machine: http://$localIp`:$ServicePort" -ForegroundColor Cyan
Write-Host "Service name: $ServiceName (use 'nssm status $ServiceName', 'net stop/start $ServiceName' to manage)" -ForegroundColor Cyan
