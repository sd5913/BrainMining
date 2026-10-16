param([ValidateSet('Import', 'Test', 'Capture')][string]$Mode = 'Capture', [string]$EngineRoot = 'E:\UE5\UE_5.8')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot = Split-Path -Parent $PSScriptRoot
$log = "$projectRoot\Saved\Logs\MainGameAudio$Mode.log"
$arguments = @("$projectRoot\BrainMining.uproject", '/Engine/Maps/Entry', '-unattended', '-nullrhi', '-NoSplash', "-abslog=$log")
if ($Mode -eq 'Import') {
    $arguments += @('-nosound', '-run=pythonscript', "-script=$projectRoot\Scripts\import_main_game_audio.py", '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities')
} elseif ($Mode -eq 'Test') {
    $arguments += @('-nosound', '-ExecCmds=Automation RunTests BrainMining.AudioPilot', '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$projectRoot\Saved\Automation\MainGameAudio")
} else {
    $arguments[1] = '/Game/MainGame/Maps/Lvl_MainGame'
    $arguments += @('-game', '-AudioMixer', '-ExitAfterCsvProfiling', '-ExecCmds=t.MaxFPS 30,au.DisableAppVolume 1,bm.Audio.MainGameCapture,csvprofile frames=3000')
}
$started = [DateTime]::UtcNow
try {
    Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -Arguments $arguments -ReportPath "$projectRoot\Saved\Performance\main-game-audio-$Mode.csv" -BelowNormal -MaximumSeconds 180
} catch {
    if ($Mode -ne 'Import' -or $_.Exception.Message -notlike 'Process exited with code 1.*') { throw }
    $unexpected = @(Get-Content -LiteralPath $log | Where-Object { $_ -match ': Error:' -and $_ -notmatch 'FBodyInstance::Get(SimplePhysicalMaterial|ComplexPhysicalMaterials) : GEngine not initialized' })
    if ($unexpected.Count -gt 0) { throw }
    Write-Warning 'Known CDO physics startup error; requiring fresh successful import evidence.'
}
if ($Mode -eq 'Import') {
    $file = Get-Item -LiteralPath "$projectRoot\Saved\MainGameAudioImport.json"
    $report = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($file.LastWriteTimeUtc -lt $started -or $report.Count -ne 19 -or -not (Select-String -LiteralPath $log -SimpleMatch 'MAIN_GAME_AUDIO_IMPORT_OK: 19' -Quiet)) { throw 'Import is incomplete or stale.' }
} elseif ($Mode -eq 'Test') {
    $file = Get-Item -LiteralPath "$projectRoot\Saved\Automation\MainGameAudio\index.json"
    $report = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($file.LastWriteTimeUtc -lt $started -or $report.succeeded -lt 8 -or $report.failed -gt 0) { throw 'Audio automation is incomplete or failed.' }
} else {
    $file = Get-Item -LiteralPath "$projectRoot\Saved\MainGameAudioRuntime.json"
    $report = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $recording = Get-Item -LiteralPath "$projectRoot\Saved\MainGameAudioRuntime.wav"
    if ($file.LastWriteTimeUtc -lt $started -or -not $report.passed -or $recording.LastWriteTimeUtc -lt $started) { throw 'MainGame audio runtime validation failed or is stale.' }
}
