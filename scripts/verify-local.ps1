param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',

    [int]$BenchmarkSeconds = 60,

    [switch]$SkipBuild,
    [switch]$SkipLive,
    [switch]$OpenApp
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Step([string]$Message) {
    Write-Host "`n==> $Message"
}

function Require-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command not found on PATH: $Name"
    }
}

Require-Command git
Require-Command cmake
Require-Command ctest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$commit = (git rev-parse --short=12 HEAD).Trim()
$branch = (git rev-parse --abbrev-ref HEAD).Trim()
$status = git status --porcelain
$runDir = Join-Path $repoRoot "validation/local/$timestamp-$commit"
New-Item -ItemType Directory -Path $runDir -Force | Out-Null

Write-Step 'Recording local environment'
$environment = @()
$environment += "timestamp=$timestamp"
$environment += "branch=$branch"
$environment += "commit=$commit"
$environment += "working_tree_clean=$([string]::IsNullOrWhiteSpace(($status -join '')))"
$environment += "configuration=$Configuration"
$environment += "computer_name=$env:COMPUTERNAME"
$environment += "processor_architecture=$env:PROCESSOR_ARCHITECTURE"
$environment += "processor_identifier=$env:PROCESSOR_IDENTIFIER"
$environment += "os=$([System.Environment]::OSVersion.VersionString)"
$environment += "cmake=$((cmake --version | Select-Object -First 1))"
$environment += "git=$((git --version | Select-Object -First 1))"
$environment | Set-Content -Path (Join-Path $runDir 'environment.txt') -Encoding utf8

if (-not $SkipBuild) {
    Write-Step 'Configuring Visual Studio 2022 x64 build'
    cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 2>&1 |
        Tee-Object -FilePath (Join-Path $runDir 'configure.txt')
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

    Write-Step "Building $Configuration"
    cmake --build build --config $Configuration --parallel 2>&1 |
        Tee-Object -FilePath (Join-Path $runDir 'build.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
}

$binDir = Join-Path $repoRoot "build/$Configuration"
$cli = Join-Path $binDir 'luna-cli.exe'
$app = Join-Path $binDir 'LunaAudioAI.exe'

if (-not (Test-Path $cli)) {
    throw "Expected CLI binary not found: $cli"
}

Write-Step 'Running CTest'
ctest --test-dir build -C $Configuration --output-on-failure 2>&1 |
    Tee-Object -FilePath (Join-Path $runDir 'ctest.txt')
if ($LASTEXITCODE -ne 0) { throw 'CTest failed.' }

Write-Step "Running $BenchmarkSeconds-second benchmark"
& $cli benchmark --seconds $BenchmarkSeconds 2>&1 |
    Tee-Object -FilePath (Join-Path $runDir 'benchmark.txt')
if ($LASTEXITCODE -ne 0) { throw 'Benchmark failed.' }

Write-Step 'Enumerating audio devices'
& $cli devices 2>&1 | Tee-Object -FilePath (Join-Path $runDir 'devices.txt')
if ($LASTEXITCODE -ne 0) { throw 'Device enumeration failed.' }

Write-Step 'Generating deterministic WAV regression fixtures'
$audioDir = Join-Path $runDir 'audio'
New-Item -ItemType Directory -Path $audioDir -Force | Out-Null
$original = Join-Path $audioDir 'original.wav'
$denoised = Join-Path $audioDir 'denoised.wav'
$bypass = Join-Path $audioDir 'bypass.wav'
$mix50 = Join-Path $audioDir 'mix50.wav'

& $cli fixture $original --seconds 12
if ($LASTEXITCODE -ne 0) { throw 'Fixture generation failed.' }
& $cli wav $original $denoised
if ($LASTEXITCODE -ne 0) { throw 'Denoised WAV regression failed.' }
& $cli wav $original $bypass --bypass
if ($LASTEXITCODE -ne 0) { throw 'Bypass WAV regression failed.' }
& $cli wav $original $mix50 --strength 0.5
if ($LASTEXITCODE -ne 0) { throw '50% mix WAV regression failed.' }

if (-not $SkipLive) {
    Write-Step 'Running 60-second live smoke test using default devices'
    Write-Host 'This test opens the default microphone/output. Use -SkipLive if no physical device is available.'
    & $cli live --seconds 60 2>&1 | Tee-Object -FilePath (Join-Path $runDir 'live-smoke.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Live smoke test failed.' }
}

if ($OpenApp) {
    if (-not (Test-Path $app)) {
        throw "Expected desktop binary not found: $app"
    }
    Write-Step 'Opening LunaAudioAI desktop application for manual hardware checklist'
    Start-Process $app
}

@(
    '# Local verification notes',
    '',
    "- Branch: `$branch`",
    "- Commit: `$commit`",
    "- Configuration: `$Configuration`",
    "- Automated gates: completed by scripts/verify-local.ps1",
    '- Physical end-to-end latency: NOT measured by this script',
    '- Manual WINDOWS_TEST_CHECKLIST.md: pending unless explicitly completed by operator',
    '- Listening evaluation: pending unless explicitly documented',
    '',
    'Do not infer target-speaker isolation performance from the synthetic fixture benchmark.'
) | Set-Content -Path (Join-Path $runDir 'notes.md') -Encoding utf8

Write-Step 'Local verification complete'
Write-Host "Evidence directory: $runDir"
