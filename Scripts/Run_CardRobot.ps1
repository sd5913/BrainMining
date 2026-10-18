param([Parameter(Mandatory=$true)][string]$Script,[switch]$Render,[switch]$Entry)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$robotRoot=Split-Path -Parent $PSScriptRoot
$robotMap=if($Entry){'/Engine/Maps/Entry'}else{'/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2'}
$robotArgs=@("$robotRoot/BrainMining.uproject",$robotMap,'-unattended','-nosplash','-NoLiveCoding','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$robotRoot/Saved/Validation/CardRobot/User-$(Get-Date -Format yyyyMMdd-HHmmss)","-ExecutePythonScript=$robotRoot/Scripts/$Script","-abslog=$robotRoot/Saved/Logs/CardRobot-$Script.log")
if($Render){$robotArgs+=@('-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ExecCmds=t.MaxFPS 30,r.Streaming.PoolSize 1024')}else{$robotArgs+='-nullrhi'}
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $robotArgs -ReportPath "$robotRoot/Saved/Validation/CardRobot/resources-$Script.csv" -BelowNormal -MaximumSeconds 400
