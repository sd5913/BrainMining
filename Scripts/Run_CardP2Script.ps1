param([Parameter(Mandatory=$true)][string]$Script)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$cardRoot=Split-Path -Parent $PSScriptRoot
$cardArgs=@("$cardRoot/BrainMining.uproject",'/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2','-nullrhi','-unattended','-nosplash','-NoLiveCoding','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol',"-ExecutePythonScript=$cardRoot/Scripts/$Script", "-abslog=$cardRoot/Saved/Logs/CardP2-Script.log")
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $cardArgs -ReportPath "$cardRoot/Saved/Validation/CardP2Flow/resources-script.csv" -BelowNormal -MaximumSeconds 240
