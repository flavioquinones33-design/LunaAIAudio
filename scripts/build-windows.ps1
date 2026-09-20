$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
    cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    cmake --build build --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    Write-Host 'Built: build\Release\LunaAudioAI.exe'
    Write-Host 'Physical microphone and monitoring acceptance remains required.'
} finally { Pop-Location }
