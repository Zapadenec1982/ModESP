# ModESP Build Script for PowerShell
# Run this from ESP-IDF PowerShell

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "       ModESP Build Script" -ForegroundColor Cyan
Write-Host "    Phase 5 - Adaptive UI" -ForegroundColor Cyan  
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check for idf.py
try {
    $null = Get-Command idf.py -ErrorAction Stop
} catch {
    Write-Host "ERROR: idf.py not found!" -ForegroundColor Red
    Write-Host "Please run from ESP-IDF PowerShell" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Steps:" -ForegroundColor Yellow
    Write-Host "1. Open 'ESP-IDF 5.x PowerShell' from Start Menu"
    Write-Host "2. Navigate to C:\ModESP_dev"
    Write-Host "3. Run: .\build.ps1"
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# Navigate to project
Set-Location -Path "C:\ModESP_dev"
Write-Host "Current directory: $PWD" -ForegroundColor Green
Write-Host ""

# Generate code from manifests
Write-Host "Generating code from manifests..." -ForegroundColor Yellow
python tools\process_manifests.py --project-root . --output-dir main\generated
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to generate code from manifests!" -ForegroundColor Red
    Write-Host "Make sure Python is installed and available." -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}
Write-Host "Code generation completed successfully." -ForegroundColor Green
Write-Host ""

# Clean build
Write-Host "Cleaning previous build..." -ForegroundColor Yellow
idf.py fullclean
Write-Host ""

# Set target
Write-Host "Setting target to ESP32..." -ForegroundColor Yellow
idf.py set-target esp32
Write-Host ""

# Build
Write-Host "Building ModESP..." -ForegroundColor Yellow
Write-Host "This may take several minutes..." -ForegroundColor Gray
Write-Host ""

idf.py build

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "      BUILD SUCCESSFUL!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "1. Connect ESP32 via USB"
    Write-Host "2. Run: idf.py -p COM3 flash monitor"
    Write-Host "   (replace COM3 with your port)"
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "       BUILD FAILED!" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Check error messages above" -ForegroundColor Yellow
    Write-Host ""
}

Read-Host "Press Enter to exit"
