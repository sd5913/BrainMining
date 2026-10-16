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
try {
    Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -Arguments $arguments -ReportPath "$projectRoot\Saved\Performance\audio-$Mode.csv" -BelowNormal -MaximumSeconds 180
} catch {
    if ($Mode -ne 'Import' -or $_.Exception.Message -notlike 'Process exited with code 1.*') { throw }
    $importLog = Get-Content -LiteralPath "$projectRoot\Saved\Logs\AudioPilotImport.log"
    $unexpected = @($importLog | Where-Object { $_ -match ': Error:' -and $_ -notmatch 'FBodyInstance::Get(SimplePhysicalMaterial|ComplexPhysicalMaterials) : GEngine not initialized' })
    if ($unexpected.Count -gt 0) { throw }
    Write-Warning 'Import commandlet returned the known CDO physics startup error; checking fresh import artifacts before accepting completion.'
}
if ($Mode -eq 'Import') {
    $manifest = Get-Content -LiteralPath "$projectRoot\SourceAssets\AudioPilot\manifest.json" -Raw | ConvertFrom-Json
    $expected = $manifest.Count
    if (-not (Select-String -LiteralPath "$projectRoot\Saved\Logs\AudioPilotImport.log" -SimpleMatch "AUDIO_PILOT_IMPORT_OK: $expected" -Quiet)) { throw 'Audio import did not confirm all manifest assets.' }
    $importReport = Get-Item -LiteralPath "$projectRoot\Saved\AudioPilotImport.json"
    $routingReport = Get-Item -LiteralPath "$projectRoot\Saved\AudioRouting.json"
    if ($importReport.LastWriteTimeUtc -lt $runStartedAt -or $routingReport.LastWriteTimeUtc -lt $runStartedAt) { throw 'Import reports are stale.' }
    $imported = Get-Content -LiteralPath $importReport.FullName -Raw | ConvertFrom-Json
    if ($imported.Count -ne $expected) { throw 'Import report count does not match manifest.' }
} else {
    $reportFile = Get-Item -LiteralPath "$projectRoot\Saved\Automation\AudioPilot\index.json"
    if ($reportFile.LastWriteTimeUtc -lt $runStartedAt) { throw 'No fresh automation report was produced.' }
    $report = Get-Content -LiteralPath "$projectRoot\Saved\Automation\AudioPilot\index.json" -Raw | ConvertFrom-Json
    if ($null -eq $report.succeeded -or $report.failed -gt 0 -or $report.succeeded -lt 5) { throw 'Audio pilot automation did not pass all expected tests.' }
}
