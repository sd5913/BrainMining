param(
    [ValidateSet('Core', 'MainGame', 'Regression', 'Office', 'Mirror')][string]$Mode = 'Core',
    [string]$EngineRoot = 'E:\UE5\UE_5.8'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot = Split-Path -Parent $PSScriptRoot
$selection = switch ($Mode) {
    'Core' { @{ Filter = 'BrainMining.RoomAudio'; Count = 2 } }
    'MainGame' { @{ Filter = 'BrainMining.MainGame.FullCardAndTelevisionEscape'; Count = 1 } }
    'Regression' { @{ Filter = 'BrainMining.AudioPilot+CardRoom.Rules+EchoPassage'; Count = 13 } }
    'Office' { @{ Filter = 'BrainMining.Office.ExitFlow'; Count = 1 } }
    'Mirror' { @{ Filter = 'BrainMining.MainGame.MirrorMuseumAudio'; Count = 1 } }
}
$reportRoot = Join-Path $projectRoot "Saved\Automation\RoomAudio$Mode"
$arguments = @(
    (Join-Path $projectRoot 'BrainMining.uproject'), '/Engine/Maps/Entry',
    '-unattended', '-nullrhi', '-nosound', '-NoSplash', '-NoLiveCoding',
    "-ExecCmds=Automation RunTests $($selection.Filter)",
    '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$reportRoot",
    "-abslog=$projectRoot\Saved\Logs\RoomAudio$Mode.log"
)
$startedAt = [DateTime]::UtcNow
Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -Arguments $arguments -ReportPath "$projectRoot\Saved\Performance\room-audio-$Mode.csv" -BelowNormal -MaximumSeconds 300
$reportFile = Get-Item -LiteralPath (Join-Path $reportRoot 'index.json')
if ($reportFile.LastWriteTimeUtc -lt $startedAt) { throw 'The automation report is stale.' }
$report = Get-Content -LiteralPath $reportFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
if ($report.succeeded -ne $selection.Count -or $report.failed -gt 0 -or $report.notRun -gt 0 -or $report.inProcess -gt 0) {
    throw "Room audio validation failed or is incomplete: $($reportFile.FullName)"
}
Write-Host "Room audio $Mode validation passed: $($reportFile.FullName)"
