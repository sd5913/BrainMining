param([string]$Script='inspect_collection_main_game.py',[switch]$Render)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$collectionRoot=Split-Path -Parent $PSScriptRoot
$collectionSession=Join-Path $collectionRoot ('Saved/Validation/MainGameCollections/User-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$collectionArgs=@("$collectionRoot\BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosound','-nosplash','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$collectionSession", "-ExecutePythonScript=$PSScriptRoot\$Script", "-abslog=$collectionRoot\Saved\Logs\MainGameCollections-$Script.log")
if($Render){$collectionArgs+=@('-RenderOffscreen','-windowed','-ResX=1600','-ResY=900')}else{$collectionArgs+='-nullrhi'}
$collectionRunStarted=Get-Date
Start-MiningGuardedProcess -Executable 'C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -Arguments $collectionArgs -ReportPath "$collectionRoot\Saved\Performance\MainGameCollections-$Script.csv" -MaximumSeconds 600
$collectionReportName=switch($Script){
    'apply_main_game_collections.py' {'authoring.json'}
    'verify_main_game_collections.py' {'runtime.json'}
    'refine_main_game_collections.py' {'dressing.json'}
    'capture_main_game_collections.py' {'visual.json'}
    'promote_main_game_collections.py' {'promotion.json'}
    default {''}
}
if($Script -eq 'verify_restored_drill_pose.py'){
    $poseReport=Get-Item -LiteralPath "$collectionRoot/Saved/Validation/MainGameDrill/runtime.json"
    $poseResult=Get-Content -LiteralPath $poseReport.FullName -Raw | ConvertFrom-Json
    if($poseReport.LastWriteTime -lt $collectionRunStarted -or -not $poseResult.passed){throw 'MainGame drill pose/input validation failed.'}
    Write-Host 'MainGame drill pose and locked-reward input checks passed.'
}
if($collectionReportName){
    $collectionReport=Get-Item -LiteralPath "$collectionRoot\Saved\Validation\MainGameCollections\$collectionReportName"
    if($collectionReport.LastWriteTime -lt $collectionRunStarted){throw 'MainGame collection run did not produce a fresh report.'}
    $collectionResult=Get-Content -LiteralPath $collectionReport.FullName -Raw | ConvertFrom-Json
    if(-not $collectionResult.passed){throw "MainGame collection run failed: $($collectionResult.error)"}
    Write-Host "MainGame collection run passed: $Script"
}
