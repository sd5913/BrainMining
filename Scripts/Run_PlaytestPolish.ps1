param([ValidateSet('Native','Opening','Cards','Ending')][string]$Mode='Native')
$ErrorActionPreference='Stop'
. "$PSScriptRoot/ResourceGuard.ps1"
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$polishRoot=Split-Path -Parent $PSScriptRoot
$polishReport="$polishRoot/Saved/Validation/PlaytestPolish/$Mode"
New-Item -ItemType Directory -Path $polishReport -Force | Out-Null
$polishArgs=@("$polishRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-nosound','-NoLiveCoding','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-abslog=$polishReport/runtime.log","-UserDir=$polishReport/User",'-asyncassetcompilationmaxconcurrency=1','-asynctexturecompilationmaxconcurrency=1')
if($Mode -eq 'Native'){
 $polishArgs+=@('-nullrhi','-ExecCmds=mining.SaveEnabled 0,Automation RunTests BrainMining.Pickaxe+BrainMining.Opening.Bootstrap+BrainMining.Opening.TitleProjection+BrainMining.Opening.MenuLifecycle+BrainMining.Opening.PublishedBedToDoor+CardRoom.Rules','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$polishReport")
}else{
 $polishScript=if($Mode -eq 'Opening'){'verify_playtest_opening.py'}elseif($Mode -eq 'Ending'){'verify_playtest_ending.py'}else{'verify_playtest_cards.py'}
 $polishArgs+=@('-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ExecCmds=t.MaxFPS 30,r.Streaming.PoolSize 1024',"-ExecutePythonScript=$polishRoot/Scripts/$polishScript")
}
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $polishArgs -ReportPath "$polishReport/resources.csv" -BelowNormal -MaximumSeconds 600
$polishResult=Get-Content $(if($Mode -eq 'Native'){"$polishReport/index.json"}else{"$polishReport/runtime.json"}) -Raw -Encoding UTF8 | ConvertFrom-Json
if($Mode -eq 'Native'){if($polishResult.failed -gt 0 -or $polishResult.notRun -gt 0){throw 'Native checks failed'}}elseif(-not $polishResult.passed){throw $polishResult.error}
