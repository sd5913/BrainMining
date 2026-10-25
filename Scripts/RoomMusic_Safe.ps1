param([ValidateSet('Import', 'Test')][string]$Mode = 'Test', [string]$EngineRoot = 'E:\UE5\UE_5.8')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot = Split-Path -Parent $PSScriptRoot
$log = "$projectRoot\Saved\Logs\RoomMusic$Mode.log"
$arguments = @("$projectRoot\BrainMining.uproject", '/Engine/Maps/Entry', '-unattended', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$log")
if ($Mode -eq 'Import') {
    $arguments += @('-run=pythonscript', "-script=$projectRoot\Scripts\import_room_music.py", '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities')
} else {
    $arguments += @('-ExecCmds=Automation RunTests BrainMining.RoomMusic+BrainMining.AudioPilot', '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$projectRoot\Saved\Validation\RoomMusicTests")
}
$started = [DateTime]::UtcNow
Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -Arguments $arguments -ReportPath "$projectRoot\Saved\Performance\room-music-$Mode.csv" -BelowNormal -MaximumSeconds 600
if ($Mode -eq 'Import') {
    $file = Get-Item -LiteralPath "$projectRoot\Saved\RoomMusicImport.json"
    $report = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($file.LastWriteTimeUtc -lt $started -or $report.Count -ne 5 -or -not (Select-String -LiteralPath $log -SimpleMatch 'ROOM_MUSIC_IMPORT_OK: 5' -Quiet)) { throw 'Room music import is incomplete or stale.' }
} else {
    $file = Get-Item -LiteralPath "$projectRoot\Saved\Validation\RoomMusicTests\index.json"
    $report = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $roomTests = @($report.tests | Where-Object { $_.fullTestPath -like 'BrainMining.RoomMusic.*' })
    if ($file.LastWriteTimeUtc -lt $started -or $report.failed -gt 0 -or $roomTests.Count -ne 2 -or @($roomTests | Where-Object { $_.state -ne 'Success' }).Count -gt 0) { throw 'Room music automation is incomplete or failed.' }
    Write-Host "Room music and existing audio regression: $($report.succeeded) passed."
}
