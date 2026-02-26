# PC Stats Backend - Unified Installer (Windows)
# This script installs 'uv', sets up the environment, and runs the app hidden in the background.

Write-Host "--- PC Stats Backend Installer (Windows) ---" -ForegroundColor Cyan

# 1. Check for 'uv'
if (!(Get-Command uv -ErrorAction SilentlyContinue)) {
    Write-Host "'uv' not found. Installing now..." -ForegroundColor Yellow
    powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"
    $env:Path += ";$HOME\.cargo\bin"
} else {
    Write-Host "'uv' is already installed." -ForegroundColor Green
}

# 2. Sync Dependencies
Write-Host "Syncing dependencies..." -ForegroundColor Gray
uv sync

# 3. Start Background Process
Write-Host "Starting PC Stats Backend in the background..." -ForegroundColor Green
$appPath = Get-Location
$script = "uv run python main.py"

# Use Start-Process with Hidden window to run it as a service-like background task
Start-Process "uv" -ArgumentList "run python $appPath\main.py" -WindowStyle Hidden -WorkingDirectory $appPath

Write-Host "--- Installation Complete ---" -ForegroundColor Cyan
Write-Host "API is running on http://0.0.0.0:58008" -ForegroundColor Green
Write-Host "--- Security Note: The API is READ-ONLY ---" -ForegroundColor Yellow
