# Build script for HtkisPro
$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = "$ProjectRoot\build"

Write-Host "=== HtkisPro Build Script ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectRoot"
Write-Host ""

# Check cmake
try {
    $CmakeVer = (cmake --version) -split "`n" | Select-Object -First 1
    Write-Host "[OK] $CmakeVer" -ForegroundColor Green
} catch {
    Write-Error "CMake not found. Install CMake 3.14+ and add to PATH."
    exit 1
}

# Check git (for FetchContent)
try {
    $GitVer = git --version
    Write-Host "[OK] $GitVer" -ForegroundColor Green
} catch {
    Write-Error "Git not found. Install Git and add to PATH."
    exit 1
}

# Parse args
$BuildTesting = $args -contains "-t" -or $args -contains "--tests"
$CleanBuild = $args -contains "-c" -or $args -contains "--clean"

if ($CleanBuild -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Configure
Write-Host ""
Write-Host "Configuring..." -ForegroundColor Cyan
$CmakeArgs = @("-B", $BuildDir, "-S", $ProjectRoot, "-G", "Visual Studio 17 2022", "-A", "x64",
    "-DOPENSSL_ROOT_DIR=C:/vcpkg/installed/x64-windows-static")
if ($BuildTesting) {
    $CmakeArgs += @("-DBUILD_TESTING=ON")
    Write-Host "  Testing enabled" -ForegroundColor Yellow
}
cmake @CmakeArgs
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configuration failed."; exit 1 }

# Build
Write-Host ""
Write-Host "Building..." -ForegroundColor Cyan
cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }

$ExePath = "$BuildDir\Release\proxy_server.exe"
if (Test-Path $ExePath) {
    $Size = (Get-Item $ExePath).Length / 1MB
    Write-Host ""
    Write-Host "Build success! Output: $ExePath ($([math]::Round($Size, 2)) MB)" -ForegroundColor Green
} else {
    Write-Host "Build completed but exe not found." -ForegroundColor Yellow
}

# Run tests if enabled
if ($BuildTesting) {
    $TestExe = "$BuildDir\tests\Release\proxy_tests.exe"
    if (Test-Path $TestExe) {
        Write-Host ""
        Write-Host "Running tests..." -ForegroundColor Cyan
        & $TestExe --gtest_color=yes
        if ($LASTEXITCODE -eq 0) {
            Write-Host "All tests passed!" -ForegroundColor Green
        } else {
            Write-Host "Some tests failed!" -ForegroundColor Red
        }
    }
}
