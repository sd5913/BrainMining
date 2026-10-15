param([ValidateSet('Import', 'Test')][string]$Mode = 'Import', [string]$EngineRoot = 'E:\UE5\UE_5.8')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot = Split-Path -Parent $PSScriptRoot
$arguments = @("$projectRoot\BrainMining.uproject", '/Engine/Maps/Entry', '-unattended', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$projectRoot\Saved\Logs\AudioPilot$Mode.log")
if ($Mode -eq 'Import') {
    $arguments += @('-run=pythonscript', "-script=$projectRoot\Scripts\import_audio_pilot.py", '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities')
} else {
    $arguments += @('-ExecCmds=Automation RunTests BrainMining.AudioPilot', '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$projectRoot\Saved\Automation\AudioPilot")
}
$runStartedAt = [DateTime]::UtcNow
Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -Arguments $arguments -ReportPath "$projectRoot\Saved\Performance\audio-$Mode.csv" -BelowNormal -MaximumSeconds 180
if ($Mode -eq 'Import') {
    if (-not (Select-String -LiteralPath "$projectRoot\Saved\Logs\AudioPilotImport.log" -SimpleMatch 'AUDIO_PILOT_IMPORT_OK: 33' -Quiet)) { throw 'Audio import did not confirm all 33 assets.' }
} else {
    $reportFile = Get-Item -LiteralPath "$projectRoot\Saved\Automation\AudioPilot\index.json"
    if ($reportFile.LastWriteTimeUtc -lt $runStartedAt) { throw 'No fresh automation report was produced.' }
    $report = Get-Content -LiteralPath "$projectRoot\Saved\Automation\AudioPilot\index.json" -Raw | ConvertFrom-Json
    if ($null -eq $report.succeeded -or $report.failed -gt 0 -or $report.succeeded -lt 4) { throw 'Audio pilot automation did not pass all expected tests.' }
}
