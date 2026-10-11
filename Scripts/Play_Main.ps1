param(
    [string]$EngineRoot = 'E:\UE5\UE_5.8',
    [ValidateSet('Overview', 'Mine', 'Journey', 'WenSi', 'Office', 'Ending', 'Collection', 'Spatial', 'Manual', 'Chain')]
    [string]$Experience = 'Overview',
    [switch]$Editor
)
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'BrainMining.uproject'
$executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$maps = @{
    Overview = '/Game/Showcase/Maps/Lvl_MainExperiences'
    Mine = '/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo'
    Journey = '/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1'
    WenSi = '/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge'
    Office = '/Game/PopupClose/Maps/Lvl_PopupClose'
    Ending = '/Game/CellUniverse/Maps/Lvl_CellUniverse_Demo'
    Collection = '/Game/CollectionPrototype/Maps/Lvl_Collection_V03'
    Spatial = '/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo'
    Manual = '/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo'
    Chain = '/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo'
}
if (-not (Test-Path -LiteralPath $executable)) { throw "Unreal Editor not found: $executable. Supply -EngineRoot." }
$launchArguments = @($project, $maps[$Experience], '-NoSplash')
if (-not $Editor) { $launchArguments += @('-game', '-windowed', '-ResX=1600', '-ResY=900') }
& $executable @launchArguments
