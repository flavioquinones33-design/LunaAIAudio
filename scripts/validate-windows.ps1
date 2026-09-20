param([string]$Bin = '', [int]$Seconds = 60, [switch]$Live, [switch]$Monitor, [int]$Mic = 0, [int]$Output = 0)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $Bin) { $Bin = Join-Path $root 'build\Release' }
$cli = Join-Path $Bin 'luna-cli.exe'
$tests = Join-Path $Bin 'luna-tests.exe'
$run = Join-Path $root ('out\validation-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $run -Force | Out-Null
function Invoke-Checked([string]$Exe, [string[]]$Arguments, [string]$Log) {
    & $Exe @Arguments 2>&1 | Tee-Object -FilePath (Join-Path $run $Log)
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $Exe $Arguments" }
}
Invoke-Checked -Exe $tests -Arguments @() -Log 'tests.txt'
Invoke-Checked -Exe $cli -Arguments @('fixture', (Join-Path $run 'original.wav'), '--seconds', '12') -Log 'fixture.txt'
Invoke-Checked -Exe $cli -Arguments @('wav', (Join-Path $run 'original.wav'), (Join-Path $run 'denoised.wav')) -Log 'wav.json'
Invoke-Checked -Exe $cli -Arguments @('benchmark', '--seconds', "$Seconds") -Log 'benchmark.json'
Invoke-Checked -Exe $cli -Arguments @('devices') -Log 'devices.txt'
if ($Live) {
    $liveArgs = @('live', '--mic', "$Mic", '--output', "$Output", '--seconds', "$Seconds")
    if ($Monitor) { $liveArgs += '--monitor' }
    Invoke-Checked -Exe $cli -Arguments $liveArgs -Log 'live.txt'
}
Write-Host "Results: $run"
Write-Host 'This script does not validate subjective quality or physical end-to-end latency.'
