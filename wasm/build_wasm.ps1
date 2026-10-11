# wasm/build_wasm.ps1
# Builds the CollabPad CRDT C++ engine to WebAssembly using Emscripten.

param (
    [string]$OutputDir = "../client/public"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "  CollabPad WebAssembly Compiler" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# Check if emcc is available
$emccPath = Get-Command emcc -ErrorAction SilentlyContinue

if (-not $emccPath) {
    Write-Host "[ERROR] 'emcc' (Emscripten compiler) was not found in your PATH." -ForegroundColor Red
    Write-Host ""
    Write-Host "To install and activate Emscripten:" -ForegroundColor Yellow
    Write-Host "  1. git clone https://github.com/emscripten-core/emsdk.git"
    Write-Host "  2. cd emsdk"
    Write-Host "  3. .\emsdk install latest"
    Write-Host "  4. .\emsdk activate latest"
    Write-Host "  5. .\emsdk_env.ps1"
    Write-Host "  6. Re-run .\wasm\build_wasm.ps1"
    exit 1
}

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Host "[Compiling] wasm/bindings.cpp -> $OutputDir/collabpad_wasm.js ..." -ForegroundColor Green

emcc --bind -O3 -std=c++17 `
    -I ../core/include `
    bindings.cpp `
    -o "$OutputDir/collabpad_wasm.js" `
    -s MODULARIZE=1 `
    -s EXPORT_NAME="CollabPadWasm" `
    -s ALLOW_MEMORY_GROWTH=1 `
    -s ENVIRONMENT='web'

if ($LASTEXITCODE -eq 0) {
    Write-Host "[SUCCESS] WebAssembly module built successfully!" -ForegroundColor Green
    Write-Host "Artifacts written to: $OutputDir/collabpad_wasm.js and $OutputDir/collabpad_wasm.wasm"
} else {
    Write-Host "[ERROR] Compilation failed with exit code $LASTEXITCODE." -ForegroundColor Red
}
