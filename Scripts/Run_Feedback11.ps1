param([string]$Script='inspect_feedback11.py',[switch]$Render,[string]$NativeTests='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$projectRoot=Split-Path -Parent $PSScriptRoot
$report=Join-Path $projectRoot 'Saved/Validation/Feedback11'
New-Item -ItemType Directory -Path $report -Force | Out-Null
$runArgs=@("$projectRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoAsyncLoadingThread','-ddc=InstalledNoZenLocalFallback','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol','-asynctexturecompilationmaxconcurrency=1',"-UserDir=$report/User","-abslog=$report/$Script.log","-ExecutePythonScript=$projectRoot/Scripts/$Script")
if($Render){$runArgs+=@('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()')}else{$runArgs+='-nullrhi'}
if($NativeTests){
 $runArgs=@($runArgs|Where-Object{$_ -ne '-nosound' -and $_ -notlike '-ExecutePythonScript=*'})
 $runArgs[1]='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
 $runArgs+=@('-game',"-ExecCmds=mining.SaveEnabled 0,Automation RunTests $NativeTests",'-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report/Native")
}
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -Arguments $runArgs -ReportPath "$report/$Script-resources.csv" -MaximumSeconds 400
