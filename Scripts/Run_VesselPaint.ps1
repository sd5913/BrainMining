param([ValidateSet('Native','Preview')][string]$Mode='Preview')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$projectRoot=Split-Path -Parent $PSScriptRoot
$report=Join-Path $projectRoot "Saved/Validation/VesselPaint/$Mode"
New-Item -ItemType Directory -Path $report -Force | Out-Null
$runArgs=@("$projectRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoAsyncLoadingThread','-ddc=InstalledNoZenLocalFallback','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol','-asynctexturecompilationmaxconcurrency=1',"-UserDir=$report/User","-abslog=$report/runtime.log")
if($Mode -eq 'Native'){
 $runArgs+=@('-nullrhi','-ExecCmds=Automation RunTests BrainMining.Ending.PainterlyStageScope','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report")
}else{
 $runArgs+=@('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-ExecutePythonScript=$projectRoot/Scripts/verify_vessel_paint.py")
}
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -Arguments $runArgs -ReportPath "$report/resources.csv" -MaximumSeconds 180
