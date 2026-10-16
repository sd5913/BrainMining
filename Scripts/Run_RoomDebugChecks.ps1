param([string]$EngineRoot = '', [switch]$Build, [switch]$Render)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
if (-not $EngineRoot) {
    foreach ($candidate in @($env:UE_ENGINE_ROOT, 'C:\UE_5.8', 'E:\UE5\UE_5.8')) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor.exe'))) {
            $EngineRoot = $candidate
            break
        }
    }
}
if (-not $EngineRoot) { throw 'UE 5.8 not found. Supply -EngineRoot.' }
if ($Build) { & (Join-Path $PSScriptRoot 'Build_Safe.ps1') -EngineRoot $EngineRoot }
Assert-MiningLaunchResources
$modulePath = Join-Path $projectRoot 'Binaries\Win64\UnrealEditor-BrainMining.dll'
if (-not (Test-Path -LiteralPath $modulePath)) { throw 'Editor module missing. Run this script with -Build.' }
$moduleTime = (Get-Item -LiteralPath $modulePath).LastWriteTimeUtc
$newerSource = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Recurse -File |
    Where-Object { $_.Extension -in '.cpp', '.h', '.cs' -and $_.LastWriteTimeUtc -gt $moduleTime } | Select-Object -First 1
if ($newerSource) { throw 'Source is newer than the editor module. Run this script with -Build.' }
$reportRoot = Join-Path $projectRoot 'Saved\Automation\RoomDebug'
$runArguments = @(
    (Join-Path $projectRoot 'BrainMining.uproject'), '/Engine/Maps/Entry',
    '-unattended', '-nosplash', '-nosound', '-NoLiveCoding',
    '-ExecCmds=Automation RunTests BrainMining.RoomDebug+BrainMining.Main.AllExperienceAssets',
    '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$reportRoot",
    "-abslog=$projectRoot\Saved\Logs\RoomDebug.log"
)
if (-not $Render) { $runArguments += '-nullrhi' }
$startedAt = [DateTime]::UtcNow
Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') `
    -Arguments $runArguments -ReportPath (Join-Path $projectRoot 'Saved\Performance\room-debug-resources.csv') -MaximumSeconds 900
$reportPath = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTimeUtc -lt $startedAt) {
    throw "No fresh automation report: $reportPath"
}
$report = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($report.failed -gt 0 -or $report.notRun -gt 0 -or $report.inProcess -gt 0 -or $report.succeeded -lt 3) {
    throw "Room debug validation failed: $reportPath"
}
Write-Host "Room debug validation passed: $reportPath"
