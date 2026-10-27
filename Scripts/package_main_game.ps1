param(
    [string]$EngineRoot = '',
    [string]$OutputDirectory = '',
    [string]$ZenServer = '',
    [ValidateSet('', 'PCSize')][string]$CustomConfig = '',
    [string]$ReportDirectory = '',
    [string]$CookOutputDirectory = '',
    [string]$StagingDirectory = '',
    [switch]$PlanOnly,
    [switch]$SkipBuild,
    [switch]$SkipCook
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $EngineRoot) {
    foreach ($candidate in @($env:UE_ENGINE_ROOT, 'C:/UE_5.8', 'E:/UE5/UE_5.8', 'H:/UE_5.8')) {
        if ($candidate -and (Test-Path -LiteralPath "$candidate/Engine/Binaries/DotNET/AutomationTool/AutomationTool.dll")) {
            $EngineRoot = $candidate
            break
        }
    }
}
if (-not $EngineRoot -or -not (Test-Path -LiteralPath "$EngineRoot/Engine/Binaries/DotNET/AutomationTool/AutomationTool.dll")) {
    throw 'UE 5.8 AutomationTool was not found. Supply -EngineRoot.'
}
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot ('Builds/MainGame-Windows-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$report = if ($ReportDirectory) { [IO.Path]::GetFullPath($ReportDirectory) } else { Join-Path $projectRoot 'Saved/Validation/MainGamePackage' }
New-Item -ItemType Directory -Path $report -Force | Out-Null
if (-not $PlanOnly -and (Get-PSDrive -Name ([IO.Path]::GetPathRoot($OutputDirectory).Substring(0,1))).Free -lt 20GB) { throw 'Packaging requires at least 20 GB free on the output drive.' }
$dotnet = Get-ChildItem "$EngineRoot/Engine/Binaries/ThirdParty/DotNet" -Filter dotnet.exe -Recurse -File | Where-Object { $_.Directory.Name -eq 'win-x64' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $dotnet) { throw 'Bundled .NET runtime not found.' }
# Commandlet cooks ignore percentage-based shader limits. Bound all asset work as well,
# since the finale's 8K crowd textures can each require several GB of temporary memory.
$nativeMaterials = @()
$cookOptions = "-LocalDataCachePath=$projectRoot/Saved/DerivedDataCache -corelimit=4 -asyncassetcompilationmaxconcurrency=1 -asynctexturecompilationmaxconcurrency=1 -ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=2 -ini:Editor:[CookSettings]:MemoryMinFreePhysical=4096"
if ($CustomConfig) {
    if ($SkipCook) { throw 'CustomConfig requires a fresh cook.' }
    $nativeMaterials = @(Get-ChildItem -LiteralPath "$projectRoot/Source", "$projectRoot/Plugins" -Recurse -File -Include '*.cpp', '*.h' | ForEach-Object {
        [regex]::Matches([IO.File]::ReadAllText($_.FullName), '/Game/MainGame/Materials/[A-Za-z0-9_./]+') | ForEach-Object { $_.Value.Split('.')[0] }
    } | Sort-Object -Unique)
    if (-not $nativeMaterials.Count) { throw 'No explicit native material cook roots found.' }
    foreach ($material in $nativeMaterials) {
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot ('Content/' + $material.Substring(6) + '.uasset')))) {
            throw "Native material cook root does not exist: $material"
        }
    }
    $cookOptions += " -CustomConfig=$CustomConfig -PACKAGE=" + ($nativeMaterials -join '+')
}
$argsUat = @(
    "$EngineRoot/Engine/Binaries/DotNET/AutomationTool/AutomationTool.dll",
    "-ScriptsForProject=$projectRoot/BrainMining.uproject", '-NoCompile', '-Utf8Output',
    'BuildCookRun', "-project=$projectRoot/BrainMining.uproject", '-NoP4', '-unattended',
    '-target=WOoOW', '-platform=Win64', '-clientconfig=Shipping',
    '-stage', '-pak', '-iostore', '-compressed',
    '-prereqs', '-nodebuginfo', '-archive', "-archivedirectory=$OutputDirectory",
    '-map=/Game/MainGame/Maps/Lvl_MainGame+/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo',
    '-UbtArgs=-MaxParallelActions=2 -NoUBA -NoXGE -NoFASTBuild -NoSNDBS',
    "-AdditionalCookerOptions=$cookOptions"
)
if ($CustomConfig) { $argsUat += "-CustomConfig=$CustomConfig" }
if ($CookOutputDirectory) { $argsUat += "-CookOutputDir=$([IO.Path]::GetFullPath($CookOutputDirectory))" }
if ($StagingDirectory) { $argsUat += "-stagingdirectory=$([IO.Path]::GetFullPath($StagingDirectory))" }
if ($ZenServer) { $argsUat += "-NoZenAutoLaunch=$ZenServer" }
if ($SkipBuild) { $argsUat += '-skipbuild' } else { $argsUat += '-build' }
if ($SkipCook) { $argsUat += '-skipcook' } else { $argsUat += '-cook' }
@{output=$OutputDirectory;started=(Get-Date).ToString('o');configuration='Shipping';custom_config=$CustomConfig;plan_only=[bool]$PlanOnly;native_materials=$nativeMaterials;arguments=$argsUat} | ConvertTo-Json -Depth 4 | Set-Content "$report/request.json" -Encoding UTF8
if ($PlanOnly) { Write-Host "Packaging plan: $report/request.json"; return }
Assert-MiningLaunchResources -MinimumPhysicalGB 6 -MinimumCommitGB 8
# Audit actual saved references, since replaced editable scenery can leave unused
# source assets on disk that are intentionally absent from the cooked game.
$dependencyAuditStarted = Get-Date
$previousReport = [Environment]::GetEnvironmentVariable('BM_PACKAGE_REPORT_DIR', 'Process')
try {
    [Environment]::SetEnvironmentVariable('BM_PACKAGE_REPORT_DIR', $report, 'Process')
    Start-MiningGuardedProcess -Executable "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" -Arguments @(
    "$projectRoot/BrainMining.uproject", '/Engine/Maps/Entry', '-unattended', '-nosplash', '-nosound', '-nullrhi',
    '-NoLiveCoding', '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol',
    "-ExecutePythonScript=$projectRoot/Scripts/audit_ending_package_dependencies.py", "-abslog=$report/dependency-audit.log"
) -ReportPath "$report/dependency-audit-resources.csv" -MaximumSeconds 180
} finally {
    [Environment]::SetEnvironmentVariable('BM_PACKAGE_REPORT_DIR', $previousReport, 'Process')
}
if (-not (Test-Path -LiteralPath "$report/ending-dependencies.json") -or (Get-Item -LiteralPath "$report/ending-dependencies.json").LastWriteTime -lt $dependencyAuditStarted) {
    throw 'The ending dependency audit did not produce a fresh manifest.'
}
$endingDependencies = Get-Content -LiteralPath "$report/ending-dependencies.json" -Raw | ConvertFrom-Json
# Keep derived data on the project drive in all child tools, without changing user settings.
$previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
$previousLogs = [Environment]::GetEnvironmentVariable('uebp_LogFolder', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', "$projectRoot/Saved/DerivedDataCache", 'Process')
    [Environment]::SetEnvironmentVariable('uebp_LogFolder', "$report/UAT", 'Process')
    Start-MiningGuardedProcess -Executable $dotnet.FullName -Arguments $argsUat -ReportPath "$report/package-resources.csv" -BelowNormal -TrimCookWorkingSet -MaximumSeconds 14400
} finally {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
    [Environment]::SetEnvironmentVariable('uebp_LogFolder', $previousLogs, 'Process')
}
if (-not (Test-Path "$OutputDirectory/Windows/WOoOW.exe")) { throw "Packaged launcher missing: $OutputDirectory" }
# Verify the actual container, not just source dependencies: native string loads are
# invisible to map reference gathering and can otherwise disappear in Shipping.
$containerList = "$report/container-files.csv"
Start-MiningGuardedProcess -Executable "$EngineRoot/Engine/Binaries/Win64/UnrealPak.exe" -Arguments @(
    "-ListContainer=$OutputDirectory/Windows/BrainMining/Content/Paks/*.utoc", "-Csv=$containerList"
) -ReportPath "$report/container-resources.csv" -MaximumSeconds 120
$containerText = Get-Content -LiteralPath $containerList -Raw
$requiredAssets = @(
    'Audio/Rooms/S_OfficeMouseClick.uasset',
    'Audio/Rooms/S_HeavenAngelChoir.uasset',
    'Audio/RoomMusic/M_Room_CardExplore_Loop.uasset',
    'Audio/RoomMusic/M_Room_Museum_Loop.uasset',
    'Audio/Pilot/M_DreamyFlashback_Loop.uasset',
    'Audio/Pilot/S_UI_Hover.uasset',
    'Audio/Pilot/S_UI_Click.uasset',
    'Audio/Rooms/S_TVEscapeVictory.uasset',
    'Pickaxe/SM_Pickaxe_max.uasset',
    'Subtitles/DA_CardsVictory.uasset',
    'Fonts/F_CreditsBilingual.uasset',
    'Fonts/FF_CreditsHiragino.uasset',
    'Fonts/FF_CreditsRockwell.uasset',
    'MainGame/Maps/Lvl_MainGame.umap',
    'MainGame/Blueprints/BP_MainGameCharacter.uasset',
    'Showcase/Blueprints/BP_SharedMiningCharacter.uasset',
    'Drill/Stylized/SM_DrillStylized.uasset',
    'Drill/M_DrillDust.uasset',
    'MainGame/Materials/M_BuriedMineralGlow.uasset',
    'MainGame/Materials/M_PlayerFootprint.uasset',
    'MainGame/VFX/NS_MushroomCrystalMotes.uasset',
    'CollectionPrototype/Materials/M_CollectionFeatherFade.uasset',
    'CollectionPrototype/Materials/M_CollectionFireflyHalo.uasset',
    'Passages/Echo/ArtP2/VFX/NS_P2_Poison.uasset',
    'Rooms/CardDuel/Materials/PoisonBurn/M_Smoke_2.uasset',
    'MainGame/Materials/M_BombWallIcon.uasset',
    'MainGame/Materials/M_BedrockDark.uasset',
    'PopupClose/OfficeArt/Architecture/MI_OfficeCeilingNoGI.uasset',
    'Audio/UI/S_TaskComplete.uasset',
    'UI/MiningGUI/Art/T_IconBlast.uasset',
    'DBStylizedSketchShader/Textures/T_GrungeA_MASK.uasset',
    'Fonts/F_SourceHanSansCN_Medium.uasset',
    'MainGame/UI/T_BombWallIcon.uasset',
    'MainGame/UI/T_MainGameTitle.uasset',
    'MainGame/UI/T_MainGameStart.uasset',
    'MainGame/UI/T_MainGameExit.uasset',
    'Rooms/CardDuel/UI/DA_CardRoomUISettings.uasset',
    'Rooms/CardDuel/UI/Refined/M_Card_Laser.uasset',
    'Rooms/CardDuel/UI/Refined/M_Card_Poison.uasset',
    'Rooms/CardDuel/UI/Refined/M_Card_Fire.uasset',
    'Rooms/CardDuel/UI/Refined/M_Health_Opponent.uasset',
    'Rooms/CardDuel/UI/Refined/M_Health_Player.uasset',
    'Rooms/CardDuel/UI/Refined/T_Supplied_Laser.uasset',
    'Rooms/CardDuel/UI/Refined/T_Supplied_Poison.uasset',
    'Rooms/CardDuel/UI/Refined/T_Supplied_Fire.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_11.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_12.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_13.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_14.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_15.uasset',
    'WenSi/Mirror/Materials/M_PoweredCable.uasset',
    'Rooms/CardDuel/Materials/M_EscapeCRTScreen.uasset',
    'CollectionPrototype/Materials/M_CollectionSilhouette.uasset',
    'CollectionPrototype/Materials/M_CollectionFlowerLogo.uasset',
    'CollectionPrototype/Materials/M_CollectionBranchGlass.uasset',
    'CollectionPrototype/Materials/M_CollectionCocktailLiquid.uasset',
    'CollectionPrototype/Materials/M_BranchClueFlat.uasset',
    'CollectionPrototype/Materials/M_BranchAroma.uasset',
    'CollectionPrototype/Materials/M_BranchInkClue.uasset',
    'CollectionPrototype/Materials/M_BranchVent.uasset',
    'CollectionPrototype/Textures/GPTFlowerReference.uasset',
    'Audio/Collectibles/S_Collect_Skewer.uasset',
    'Audio/Collectibles/S_Collect_Aquarium.uasset',
    'Audio/Collectibles/S_Collect_CoinBag.uasset',
    'Audio/Collectibles/S_Collect_Cocktail.uasset',
    'Audio/Collectibles/S_Collect_Taiji.uasset',
    'Audio/Collectibles/S_Collect_Subway.uasset',
    'Audio/Collectibles/S_Collect_Phone.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_16.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_17.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_18.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_19.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_20.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_21.uasset',
    'Subtitles/Collectibles/DA_CollectiblePickup_22.uasset',
    'EndingEmergence/Materials/M_EE_Fisheye.uasset',
    'EndingEmergence/Materials/M_EE_ScaleZoom.uasset',
    'EndingEmergence/Materials/M_EE_PulseTransit.uasset',
    'EndingEmergence/Materials/M_EE_WetLens.uasset',
    'EndingEmergence/Materials/M_EE_Exhaustion.uasset',
    'EndingEmergence/Materials/M_EE_StarlightSpark.uasset',
    'EndingEmergence/Materials/M_EE_StarlightStream.uasset',
    'EndingEmergence/Materials/M_EE_UniverseShard.uasset',
    'EndingEmergence/UI/WBP_EndingCredits.uasset',
    'EndingEmergence/Meshes/SM_EE_BloodCirculationVeinSegment.uasset',
    'EndingEmergence/Meshes/SM_EE_SealedCellNeck.uasset',
    'EndingEmergence/PaperFinale/MS_EndingFinale.uasset',
    'StartRoom/UI/M_OpeningTitlePerspective.uasset',
    'StartRoom/UI/T_OpeningTitle.uasset',
    'BloodCirculation/Mesh/Cell/SM_RedBloodCell.uasset',
    'BloodCirculation/Materials/MaterialInstance/BloodVessel/MI_Vein.uasset',
    'BloodCirculation/Materials/MaterialInstance/Cell/MI_RedBloodCell.uasset',
    'Rooms/CardDuel/Robot/SM_Robot_Head.uasset',
    'Rooms/CardDuel/Robot/SM_Robot_BrowL_Singed.uasset',
    'Rooms/CardDuel/Robot/SM_Robot_Body.uasset',
    'Rooms/CardDuel/Robot/SM_Robot_FootL.uasset',
    'Rooms/CardDuel/Robot/SM_Robot_FootR.uasset',
    'PopupClose/Maps/_GENERATED/Lenovo/office_doors_Submesh_704FAA0D.uasset'
)
Get-ChildItem -LiteralPath "$projectRoot/Content/Passages/Echo/ArtP2/Collision" -Filter '*_Walkable.uasset' -File | ForEach-Object {
    $requiredAssets += 'Passages/Echo/ArtP2/Collision/' + $_.Name
}
$requiredAssets += @($endingDependencies.assets)
$requiredAssets += @($nativeMaterials | ForEach-Object { $_.Substring(6) + '.uasset' })
$requiredAssets = @($requiredAssets | Sort-Object -Unique)
foreach ($asset in $requiredAssets) {
    if ($containerText.IndexOf("../../../BrainMining/Content/$asset,", [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Required runtime asset missing from packaged container: $asset"
    }
}
$requiredAssets | ConvertTo-Json | Set-Content "$report/required-container-assets.json" -Encoding UTF8
if ($CustomConfig -and $containerText -match '(?i)/HDRIBackdrop/') {
    throw 'PCSize unexpectedly contains HDRIBackdrop assets.'
}
$movie = 'Content/Movies/EndingEmergence/EndingFinale.mp4'
$stagedMovie = Join-Path $OutputDirectory "Windows/BrainMining/$movie"
if (-not (Test-Path -LiteralPath $stagedMovie) -or (Get-FileHash -LiteralPath $stagedMovie).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot $movie)).Hash) {
    throw 'The staged ending movie is missing or differs from the current silent source.'
}
Write-Host "MainGame Shipping package: $OutputDirectory/Windows/WOoOW.exe"
