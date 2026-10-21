param([ValidateSet('Inspect','Assets','BrowAssets','Native','Visual','Music')][string]$Mode='Native')
$ErrorActionPreference='Stop'
. "$PSScriptRoot/ResourceGuard.ps1"
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$checkRoot=Split-Path -Parent $PSScriptRoot
$report="$checkRoot/Saved/Validation/Playtest18"
New-Item -ItemType Directory -Path $report -Force | Out-Null
$runArgs=@("$checkRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-NoLiveCoding','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-abslog=$report/$Mode.log","-UserDir=$report/User",'-asyncassetcompilationmaxconcurrency=1','-asynctexturecompilationmaxconcurrency=1',"-LocalDataCachePath=$checkRoot/Saved/DerivedDataCache")
if($Mode -eq 'Native') {
    $runArgs+=@('-nullrhi','-nosound','-ExecCmds=mining.SaveEnabled 0,Automation RunTests CardRoom.Rules+CardRoom.Presentation.PermanentSingedBrow+BrainMining.RoomAudio+BrainMining.Collection+BrainMining.MaterialProgression+BrainMining.RoomDebug', '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report/Native")
} elseif($Mode -eq 'Music') {
    $runArgs[1]='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
    $runArgs+=@('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ExecCmds=t.MaxFPS 30,mining.SaveEnabled 0,Automation RunTests BrainMining.EndingEmergence.MusicContinuity','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report/Music")
} else {
    $script=@{Inspect='inspect_playtest18_details.py';Assets='apply_playtest18_assets.py';BrowAssets='create_singed_robot_brow.py';Visual='capture_playtest18.py'}[$Mode]
    $runArgs+="-ExecutePythonScript=$checkRoot/Scripts/$script"
    if($Mode -eq 'Visual') {$runArgs+=@('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ExecCmds=t.MaxFPS 30,mining.SaveEnabled 0,r.Streaming.PoolSize 768')}
    else {$runArgs+=@('-nullrhi','-nosound')}
}
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -Arguments $runArgs -ReportPath "$report/$Mode-resources.csv" -MaximumSeconds 600
if($Mode -in @('Native','Music')) {
    $result=Get-Content -LiteralPath "$report/$Mode/index.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if($result.failed -gt 0 -or $result.notRun -gt 0){throw "$Mode regression checks failed; inspect $report/$Mode/index.json"}
} elseif($Mode -in @('Assets','BrowAssets','Visual')) {
    $resultFile=if($Mode -eq 'Assets'){"$report/assets-applied.json"}elseif($Mode -eq 'BrowAssets'){"$report/singed-brow.json"}else{"$report/Visual/runtime.json"}
    $result=Get-Content -LiteralPath $resultFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if(-not $result.passed){throw $result.error}
}
