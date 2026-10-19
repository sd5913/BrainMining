param([ValidateSet('Native','Runtime','Render','Rooms','Portal')][string]$Mode='Runtime')
$ErrorActionPreference='Stop'
$feedbackRoot=Split-Path -Parent $PSScriptRoot
$feedbackOut=Join-Path $feedbackRoot 'Saved/Validation/CollectionFeedback'
$feedbackStarted=Get-Date
if($Mode -ne 'Native') {
    $feedbackScript=if($Mode -eq 'Rooms'){'capture_collection_feedback_rooms.py'}elseif($Mode -eq 'Portal'){'capture_collection_feedback_portal.py'}else{'verify_collection_feedback.py'}
    if($Mode -in @('Render','Rooms','Portal')){& "$PSScriptRoot/Run_MainGameCollectionScript.ps1" -Script $feedbackScript -Render}
    else {& "$PSScriptRoot/Run_MainGameCollectionScript.ps1" -Script $feedbackScript}
    $feedbackReport=Join-Path $feedbackOut $(if($Mode -eq 'Rooms'){'rooms.json'}elseif($Mode -eq 'Portal'){'portal.json'}else{'runtime.json'})
    $feedbackResult=Get-Content -LiteralPath $feedbackReport -Raw -Encoding UTF8 | ConvertFrom-Json
    if((Get-Item -LiteralPath $feedbackReport).LastWriteTime -lt $feedbackStarted -or -not $feedbackResult.passed){throw "Feedback verification failed: $($feedbackResult.error)"}
} else {
    . "$PSScriptRoot/ResourceGuard.ps1"
    Assert-MiningLaunchResources
    $feedbackArgs=@("$feedbackRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosound','-nosplash','-NoLiveCoding','-nullrhi',
        '-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol',
        '-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$feedbackOut/NativeUser-$(Get-Date -Format yyyyMMdd-HHmmss)",
        '-ExecCmds=mining.SaveEnabled 0,Automation RunTests BrainMining.Collection+BrainMining.RoomDebug+BrainMining.Campaign.MousePaintingEndingTravel+BrainMining.Ending.PainterlyStageScope',
        '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$feedbackOut/Native","-abslog=$feedbackOut/Native.log")
    Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $feedbackArgs -ReportPath "$feedbackOut/Native-resources.csv" -MaximumSeconds 900
    $feedbackReport=Join-Path $feedbackOut 'Native/index.json';$feedbackResult=Get-Content -LiteralPath $feedbackReport -Raw -Encoding UTF8 | ConvertFrom-Json
    if((Get-Item -LiteralPath $feedbackReport).LastWriteTime -lt $feedbackStarted -or $feedbackResult.failed -gt 0 -or $feedbackResult.notRun -gt 0 -or ($feedbackResult.succeeded+$feedbackResult.succeededWithWarnings) -lt 9){throw "Native feedback checks failed: $feedbackReport"}
}
Write-Host "Feedback checks passed: $Mode"
