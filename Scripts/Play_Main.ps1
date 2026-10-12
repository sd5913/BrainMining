param(
    [string]$EngineRoot = '',
    [ValidateSet('Campaign', 'Overview', 'Mine', 'Journey', 'WenSi', 'Office', 'Ending', 'Collection', 'Spatial', 'Manual', 'Chain')]
    [string]$Experience = 'Campaign',
    [switch]$Editor
)
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'BrainMining.uproject'
if (-not $EngineRoot) {
    foreach ($candidate in @($env:UE_ENGINE_ROOT, 'C:\UE_5.8', 'E:\UE5\UE_5.8')) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor.exe'))) {
            $EngineRoot = $candidate
            break
        }
    }
}
if (-not $EngineRoot) { throw 'UE 5.8 not found. Supply -EngineRoot or set UE_ENGINE_ROOT.' }
$executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$maps = @{
    Campaign = '/Game/Campaign/Maps/Lvl_FullJourney'
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
Push-Location (Split-Path -Parent $executable)
try { & $executable @launchArguments }
finally { Pop-Location }
