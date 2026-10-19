$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$collectionRoot=Split-Path -Parent $PSScriptRoot
$collectionArgs=@("$collectionRoot\BrainMining.uproject",'/Game/CollectionIntegration/Maps/Lvl_MainGame_CollectionsPreview','-game','-windowed','-ResX=1600','-ResY=900','-nosplash','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol','-ExecCmds=mining.SaveEnabled 0',"-UserDir=$collectionRoot\Saved\CollectionPreviewUser")
Start-MiningGuardedProcess -Executable 'C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -Arguments $collectionArgs -ReportPath "$collectionRoot\Saved\Performance\CollectionPreview.csv" -EditorSession
