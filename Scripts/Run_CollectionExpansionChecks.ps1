param([ValidateSet('Upgrade','Native','Runtime','Render')][string]$Mode='Runtime')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$collectionRoot=Split-Path -Parent $PSScriptRoot
$collectionArgs=@("$collectionRoot\BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosound','-nosplash','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()')
$collectionExe='C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
if($Mode -eq 'Upgrade') {
    $collectionExe='C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $collectionArgs+=@('-nullrhi',"-ExecutePythonScript=$collectionRoot\Scripts\expand_collection_test_room.py")
} elseif($Mode -eq 'Native') {
    $collectionArgs+=@('-nullrhi','-ExecCmds=Automation RunTests BrainMining.Collection+BrainMining.Save.GarbageCollectionLifetime','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$collectionRoot\Saved\Validation\CollectionExpansion\Native")
} else {
    $collectionArgs+=@("-ExecutePythonScript=$collectionRoot\Scripts\verify_collection_expansion.py",'-windowed','-ResX=1600','-ResY=900')
    if($Mode -eq 'Render') {$collectionArgs+='-RenderOffscreen'} else {$collectionArgs+='-nullrhi'}
}
$collectionArgs+="-abslog=$collectionRoot\Saved\Logs\CollectionExpansion-$Mode.log"
$collectionSavePath=Join-Path $collectionRoot 'Saved/SaveGames/BrainMining_Auto.sav'
$collectionSaveBefore=if(Test-Path -LiteralPath $collectionSavePath){(Get-FileHash -LiteralPath $collectionSavePath).Hash}else{''}
$collectionRunStarted=Get-Date
Start-MiningGuardedProcess -Executable $collectionExe -Arguments $collectionArgs -ReportPath "$collectionRoot\Saved\Performance\CollectionExpansion-$Mode.csv" -MaximumSeconds 400

if($Mode -in @('Runtime','Render')) {
    $collectionSaveAfter=if(Test-Path -LiteralPath $collectionSavePath){(Get-FileHash -LiteralPath $collectionSavePath).Hash}else{''}
    if($collectionSaveBefore -ne $collectionSaveAfter){throw 'Collection validation unexpectedly changed the campaign save.'}
    $collectionReport=Get-Item -LiteralPath "$collectionRoot\Saved\Validation\CollectionExpansion\validation.json"
    if($collectionReport.LastWriteTime -lt $collectionRunStarted){throw 'Collection validation did not produce a fresh report.'}
    $collectionResult=Get-Content -LiteralPath $collectionReport.FullName -Raw | ConvertFrom-Json
    if(-not $collectionResult.passed){throw "Collection validation failed: $($collectionResult.error)"}
    Write-Host "Collection validation passed: $($collectionResult.checks.Count) checks; campaign save unchanged."
} elseif($Mode -eq 'Native') {
    $collectionReport=Get-Item -LiteralPath "$collectionRoot\Saved\Validation\CollectionExpansion\Native\index.json"
    $collectionResult=Get-Content -LiteralPath $collectionReport.FullName -Raw | ConvertFrom-Json
    if($collectionReport.LastWriteTime -lt $collectionRunStarted -or $collectionResult.failed -gt 0 -or $collectionResult.notRun -gt 0){throw 'Native collection tests failed or did not complete.'}
}
