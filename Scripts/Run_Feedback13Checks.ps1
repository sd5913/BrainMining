param([ValidateSet('Main','Ending','Card','Finale')][string]$Mode='Main',[switch]$PaperOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$projectRoot=Split-Path -Parent $PSScriptRoot
$report=Join-Path $projectRoot 'Saved/Validation/Feedback13'
$checkArgs=@("$projectRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-nosound','-NoLiveCoding','-NoAsyncLoadingThread','-ddc=InstalledNoZenLocalFallback','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-asynctexturecompilationmaxconcurrency=1','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$report/User","-abslog=$report/$Mode.log")
if($Mode -eq 'Finale'){
 $checkArgs=@($checkArgs | Where-Object { $_ -ne '-nosound' })
 $checkArgs[1]='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
 $checkArgs+=@('-game','-ExecCmds=mining.SaveEnabled 0,Automation RunTests BrainMining.EndingEmergence.TitleDrillReality','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report/Finale")
 if($PaperOnly){$checkArgs+='-PaperFinaleOnly'}
}else{
 $script=switch($Mode){'Main'{"$projectRoot/Scripts/verify_feedback13_main.py"};'Ending'{"$projectRoot/Scripts/verify_feedback13_ending.py"};'Card'{"$projectRoot/Scripts/verify_feedback13_card.py"}}
 $checkArgs+="-ExecutePythonScript=$script"
}
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $checkArgs -ReportPath "$report/$Mode-resources.csv" -MaximumSeconds 300
