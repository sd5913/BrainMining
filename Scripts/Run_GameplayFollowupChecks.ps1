param([ValidateSet('Native','Collections','Visual')][string]$Mode='Native')
$ErrorActionPreference='Stop'
$followRoot=Split-Path -Parent $PSScriptRoot
$followOut=Join-Path $followRoot 'Saved/Validation/GameplayFollowup'
$followStarted=Get-Date
if($Mode -eq 'Native') {
    . "$PSScriptRoot/ResourceGuard.ps1"
    Assert-MiningLaunchResources
    $followArgs=@("$followRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosound','-nosplash','-NoLiveCoding','-nullrhi',
      '-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol',
      '-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$followOut/NativeUser-$(Get-Date -Format yyyyMMdd-HHmmss)",
      '-ExecCmds=mining.SaveEnabled 0,Automation RunTests BrainMining.Collection+BrainMining.Flight+BrainMining.Ending.PainterlyStageScope+BrainMining.Encounters.PIE.P2CardEscapeCheckpoints',
      '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$followOut/Native","-abslog=$followOut/Native.log")
    Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $followArgs -ReportPath "$followOut/Native-resources.csv" -MaximumSeconds 900
    $followReport=Join-Path $followOut 'Native/index.json';$r=Get-Content -LiteralPath $followReport -Raw -Encoding UTF8 | ConvertFrom-Json
    if((Get-Item -LiteralPath $followReport).LastWriteTime -lt $followStarted -or $r.failed -gt 0 -or $r.notRun -gt 0){throw 'Follow-up native checks failed.'}
} else {
    $followScript=if($Mode -eq 'Collections'){'verify_gameplay_followup_collections.py'}else{'capture_gameplay_followup.py'}
    & "$PSScriptRoot/Run_MainGameCollectionScript.ps1" -Script $followScript -Render
    $followReport=Join-Path $followOut $(if($Mode -eq 'Collections'){'runtime.json'}else{'visual.json'})
    $r=Get-Content -LiteralPath $followReport -Raw -Encoding UTF8 | ConvertFrom-Json
    if((Get-Item -LiteralPath $followReport).LastWriteTime -lt $followStarted -or -not $r.passed){throw "Follow-up checks failed: $($r.error)"}
}
