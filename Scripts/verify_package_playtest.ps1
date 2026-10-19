param([ValidateSet('Native','Office','Scale','Finale')][string]$Mode='Native')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot=Split-Path -Parent $PSScriptRoot
$report=Join-Path $projectRoot "Saved/Validation/PackagePlaytest/$Mode"
New-Item -ItemType Directory -Path $report -Force | Out-Null
$testNames=switch($Mode){
    'Native' {'BrainMining.Collection+BrainMining.Pickaxe+BrainMining.MainGame.MirrorMuseumAudio'}
    'Office' {'BrainMining.Office.ExitFlow'}
    'Scale' {'BrainMining.EndingEmergence.ScaleMotion'}
    'Finale' {'BrainMining.EndingEmergence.TitleDrillReality'}
}
$runArgs=@("$projectRoot/BrainMining.uproject",'-unattended','-nosplash','-nosound','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol',"-UserDir=$report/User", "-ExecCmds=Automation RunTests $testNames",'-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report","-abslog=$report/runtime.log","-LocalDataCachePath=$projectRoot/Saved/DerivedDataCache")
if($Mode -in @('Native','Office')){$runArgs+=@('/Engine/Maps/Entry','-nullrhi')}
else{$runArgs+=@('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo','-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()')}
$started=Get-Date
$mapArg=if($Mode -in @('Native','Office')){'/Engine/Maps/Entry'}else{'/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'}
$runArgs=@($runArgs[0],$mapArg)+@($runArgs | Select-Object -Skip 1 | Where-Object {$_ -ne $mapArg})
$runArgs+='-NoAsyncLoadingThread'
if($Mode -eq 'Finale'){$runArgs=@($runArgs | Where-Object {$_ -ne '-nosound'})}
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $runArgs -ReportPath "$report/resources.csv" -MaximumSeconds 600
if((Get-Item "$report/index.json").LastWriteTime -lt $started){throw 'No fresh automation report.'}
$result=Get-Content "$report/index.json" -Raw | ConvertFrom-Json
if($result.failed -ne 0 -or ($result.succeeded+$result.succeededWithWarnings) -lt 1){throw "$Mode regression failed; see $report/index.json"}
Write-Host "PASS $Mode : $($result.succeeded+$result.succeededWithWarnings) tests"
