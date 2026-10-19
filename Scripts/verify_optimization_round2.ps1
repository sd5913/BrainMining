param([ValidateSet('Office','Card','Ending','Scale','Finale')][string]$Mode='Office')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot=Split-Path -Parent $PSScriptRoot
$report=Join-Path $projectRoot "Saved/Validation/OptimizationRound2/$Mode"
New-Item -ItemType Directory -Path $report -Force | Out-Null
$tests=switch($Mode){
 'Office' {'BrainMining.Office.ExitFlow'}
 'Card' {'BrainMining.MainGame.FullCardAndTelevisionEscape'}
 'Ending' {'BrainMining.Campaign.MousePaintingEndingTravel'}
 'Scale' {'BrainMining.EndingEmergence.ScaleMotion'}
 'Finale' {'BrainMining.EndingEmergence.TitleDrillReality'}
}
$map=if($Mode -in @('Scale','Finale')){'/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'}else{'/Engine/Maps/Entry'}
$runArgs=@("$projectRoot/BrainMining.uproject",$map,'-unattended','-nosplash','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol',"-UserDir=$report/User", "-ExecCmds=Automation RunTests $tests",'-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report","-abslog=$report/runtime.log","-LocalDataCachePath=$projectRoot/Saved/DerivedDataCache",'-NoAsyncLoadingThread')
if($Mode -in @('Office','Card')){$runArgs+=@('-nullrhi','-nosound')}
else{$runArgs+=@('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()')}
if($Mode -in @('Scale','Finale')){$runArgs+='-game'}
if($Mode -eq 'Ending'){$runArgs+='-MouseMuseumScreenshots'}
$started=Get-Date
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $runArgs -ReportPath "$report/resources.csv" -MaximumSeconds 600
if((Get-Item "$report/index.json").LastWriteTime -lt $started){throw 'No fresh automation report.'}
$result=Get-Content "$report/index.json" -Raw | ConvertFrom-Json
if($result.failed -ne 0 -or ($result.succeeded+$result.succeededWithWarnings) -lt 1){throw "$Mode failed; see $report/index.json"}
Write-Host "PASS $Mode"
