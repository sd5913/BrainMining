param(
    [string]$EngineRoot = '',
    [ValidateSet('Campaign', 'MainGame', 'Overview', 'Mine', 'Journey', 'WenSi', 'Office', 'Ending', 'Collection', 'Spatial', 'Manual', 'Chain')]
    [string]$Experience = 'Campaign',
    [ValidateSet('Low', 'Balanced', 'Epic')]
    [string]$Quality = 'Balanced',
    [ValidateRange(800, 7680)]
    [int]$Width = 1600,
    [ValidateRange(600, 4320)]
    [int]$Height = 900,
    [switch]$Profile,
    [switch]$TraceStartup,
    [string]$PackagedRoot = '',
    [switch]$Editor
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'BrainMining.uproject'
if ($Editor -and ($Profile -or $TraceStartup -or $PackagedRoot)) { throw 'Profiling and packaged launch require game mode; omit -Editor.' }
Assert-MiningLaunchResources
if (-not $PackagedRoot -and -not $EngineRoot) {
    foreach ($candidate in @($env:UE_ENGINE_ROOT, 'C:\UE_5.8', 'E:\UE5\UE_5.8')) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor.exe'))) {
            $EngineRoot = $candidate
            break
        }
    }
}
if ($PackagedRoot) {
    $executable = ''
    foreach ($name in @('BrainMining.exe', 'BrainMining-Win64-Development.exe', 'BrainMining-Win64-Shipping.exe')) {
        $candidate = Join-Path $PackagedRoot "BrainMining\Binaries\Win64\$name"
        if (Test-Path -LiteralPath $candidate) { $executable = $candidate; break }
    }
    if (-not $executable) { throw 'No packaged BrainMining executable found under PackagedRoot\BrainMining\Binaries\Win64.' }
} else {
    if (-not $EngineRoot) { throw 'UE 5.8 not found. Supply -EngineRoot or set UE_ENGINE_ROOT.' }
    $executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $modulePath = Join-Path $projectRoot 'Binaries\Win64\UnrealEditor-BrainMining.dll'
    if (-not (Test-Path -LiteralPath $modulePath)) { throw 'Editor module missing. Close Unreal and run Scripts\Build_Safe.ps1 first.' }
    $moduleTime = (Get-Item -LiteralPath $modulePath).LastWriteTimeUtc
    $newerSource = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Recurse -File |
        Where-Object { $_.Extension -in '.cpp','.h','.cs' -and $_.LastWriteTimeUtc -gt $moduleTime } | Select-Object -First 1
    if ($newerSource) { throw 'Source files are newer than the editor module. Close Unreal and run Scripts\Build_Safe.ps1 before playing.' }
}
$maps = @{
    Campaign = '/Game/Campaign/Maps/Lvl_FullJourney'
    MainGame = '/Game/MainGame/Maps/Lvl_MainGame'
    Overview = '/Game/Showcase/Maps/Lvl_MainExperiences'
    Mine = '/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo'
    Journey = '/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1'
    WenSi = '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum'
    Office = '/Game/PopupClose/Maps/Lvl_PopupClose'
    Ending = '/Game/CellUniverse/Maps/Lvl_CellUniverse_Demo'
    Collection = '/Game/CollectionPrototype/Maps/Lvl_Collection_V03'
    Spatial = '/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo'
    Manual = '/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo'
    Chain = '/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo'
}
if (-not (Test-Path -LiteralPath $executable)) { throw "Unreal Editor not found: $executable. Supply -EngineRoot." }
$map = $maps[$Experience]
$launchArguments = if ($PackagedRoot) { @($map, '-NoSplash') } else { @($project, $map, '-NoSplash') }
$runLabel = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$reportPath = Join-Path $projectRoot "Saved\Performance\resources-$runLabel.csv"
if (-not $Editor) {
    $tier = if ($Quality -eq 'Epic') { 3 } elseif ($Quality -eq 'Low') { 1 } else { 2 }
    $qualityCommands = @("sg.GlobalIlluminationQuality $tier", "sg.ReflectionQuality $tier", "sg.ShadowQuality $tier", 't.MaxFPS 60')
    if ($Quality -eq 'Low') { $qualityCommands += @('sg.TextureQuality 1', 'sg.EffectsQuality 1', 'sg.PostProcessQuality 1', 'r.ScreenPercentage 75') }
    if ($Profile) {
        $qualityCommands += @('r.VSync 0', 'csvprofile frames=1800')
        $launchArguments += @('-csvGpuStats', '-csvCompression=0', '-ExitAfterCsvProfiling')
        Write-Host 'Capturing 1800 frames; the game exits when complete. Reports: Saved\Profiling\CSV'
    }
    if ($TraceStartup) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $reportPath) -Force | Out-Null
        $launchArguments += @('-trace=default,loadtime', "-tracefile=$projectRoot\Saved\Performance\startup-$runLabel.utrace")
    }
    $launchArguments += @('-game', '-windowed', "-ResX=$Width", "-ResY=$Height", ('-ExecCmds=' + ($qualityCommands -join ',')))
}
$maximumSeconds = if ($Profile) { 180 } else { 0 }
Start-MiningGuardedProcess -Executable $executable -Arguments $launchArguments -ReportPath $reportPath -EditorSession:$Editor -MaximumSeconds $maximumSeconds
