param([ValidateSet('Align','Native','Visual','Pickup','P1','P2','Input')][string]$Mode='Native')
$ErrorActionPreference='Stop'
if($Mode -eq 'Native') {
    # Each PIE fixture gets its own process and fresh save directory.
    foreach($cardCase in @('P1','P2','Input')) { & $PSCommandPath -Mode $cardCase }
    return
}
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$cardRoot=Split-Path -Parent $PSScriptRoot
$cardOut=Join-Path $cardRoot 'Saved/Validation/CardP2Flow'
New-Item -ItemType Directory -Path $cardOut -Force | Out-Null
$cardRunId=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$cardArgs=@("$cardRoot/BrainMining.uproject",'/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2','-unattended','-nosplash','-NoLiveCoding','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$cardOut/User-$Mode-$cardRunId", "-abslog=$cardRoot/Saved/Logs/CardP2-$Mode.log")
if($Mode -in @('P1','P2','Input')) {
    $cardTest=switch($Mode) { P1 {'CardEscapeCheckpoints'} P2 {'P2CardEscapeCheckpoints'} Input {'GiantRoomInput'} }
    $cardArgs+=@('-nullrhi','-nosound',"-ExecCmds=Automation RunTests BrainMining.Encounters.PIE.$cardTest",'-TestExit=Automation Test Queue Empty',"-ReportExportPath=$cardOut/Automation-$Mode")
} else {
    $cardScript=switch($Mode){Align {'align_p2_escape.py'} Pickup {'verify_card_p2_pickup.py'} default {'verify_card_p2_flow.py'}}
    $cardArgs+=@('-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ExecCmds=t.MaxFPS 30,r.Streaming.PoolSize 1024',"-ExecutePythonScript=$cardRoot/Scripts/$cardScript")
}
$cardStarted=[DateTime]::UtcNow
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $cardArgs -ReportPath "$cardOut/resources-$Mode.csv" -BelowNormal -MaximumSeconds 400
$cardReport=switch($Mode){Align {"$cardOut/aligned.json"} Visual {"$cardOut/visual.json"} Pickup {"$cardOut/pickup.json"} default {"$cardOut/Automation-$Mode/index.json"}}
if(-not (Test-Path -LiteralPath $cardReport) -or (Get-Item -LiteralPath $cardReport).LastWriteTimeUtc -lt $cardStarted){throw "Missing fresh report: $cardReport"}
$cardResult=Get-Content -LiteralPath $cardReport -Raw -Encoding UTF8 | ConvertFrom-Json
if($Mode -in @('P1','P2','Input') -and ($cardResult.failed -gt 0 -or $cardResult.notRun -gt 0 -or ($cardResult.succeeded+$cardResult.succeededWithWarnings) -lt 1)){throw 'Card-room regression failed'}
if($Mode -in @('Visual','Pickup') -and -not $cardResult.passed){throw 'Card-room visual interaction failed'}
Write-Host "Completed $Mode : $cardReport"
