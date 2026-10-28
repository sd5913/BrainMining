param([string]$EngineRoot = 'E:/UE5/UE_5.8')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/ResourceGuard.ps1"
$projectRoot = Split-Path -Parent $PSScriptRoot
$reportRoot = Join-Path $projectRoot 'Saved/Validation/LocalizationAchievements'
$started = Get-Date
Assert-MiningLaunchResources -MinimumPhysicalGB 6 -MinimumCommitGB 8
Start-MiningGuardedProcess -Executable "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" -Arguments @(
    "$projectRoot/BrainMining.uproject", '/Engine/Maps/Entry', "-ExecutePythonScript=$PSScriptRoot/capture_localization_runtime.py",
    '-unattended', '-RenderOffscreen', '-nosplash', '-nosound', '-NoLiveCoding', '-corelimit=2', '-MiningQA',
    '-asyncassetcompilationmaxconcurrency=1', '-asynctexturecompilationmaxconcurrency=1',
    '-ExecCmds=t.MaxFPS 30',
    '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol',
    "-abslog=$reportRoot/runtime-capture.log", "-UserDir=$reportRoot/OpeningSettingsUser"
) -ReportPath "$reportRoot/runtime-capture-resources.csv" -TrimCommandletWorkingSet -MaximumSeconds 420
$resultPath = "$reportRoot/RuntimeScreens/report.json"
if (-not (Test-Path -LiteralPath $resultPath) -or (Get-Item -LiteralPath $resultPath).LastWriteTime -lt $started) { throw 'No fresh runtime capture report was generated.' }
$results = Get-Content -LiteralPath $resultPath -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not $results.passed -or $results.images.Count -ne 21) { throw 'Runtime capture did not pass; inspect RuntimeScreens/report.json.' }
Write-Host "Twenty-one runtime screenshots: $reportRoot/RuntimeScreens"
