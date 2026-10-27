param(
    [string]$EngineRoot = 'E:/UE5/UE_5.8',
    [switch]$RunTests
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/ResourceGuard.ps1"
$localizationRoot = Split-Path -Parent $PSScriptRoot
$localizationReport = Join-Path $localizationRoot 'Saved/Validation/LocalizationAchievements'
New-Item -ItemType Directory -Path $localizationReport -Force | Out-Null
& "$EngineRoot/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" "$PSScriptRoot/localize_game.py"
if ($LASTEXITCODE -ne 0) { throw 'Localization source validation failed.' }
Assert-MiningLaunchResources -MinimumPhysicalGB 2.5 -MinimumCommitGB 8
$localizationArgs = @(
    "$localizationRoot/BrainMining.uproject", '-run=GatherText', '-config=Config/Localization/Game.ini', '-SkipNestedMacroPrepass',
    '-unattended', '-nosplash', '-nullrhi', '-nosound', '-NoLiveCoding', '-corelimit=2',
    '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol',
    "-abslog=$localizationReport/gather.log", "-UserDir=$localizationReport/GatherUser"
)
Start-MiningGuardedProcess -Executable "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" -Arguments $localizationArgs -ReportPath "$localizationReport/gather-resources.csv" -TrimCommandletWorkingSet -MaximumSeconds 600
foreach ($culture in @('zh-Hans','en','zh-Hant')) {
    $locres = Join-Path $localizationRoot "Content/Localization/Game/$culture/Game.locres"
    if (-not (Test-Path -LiteralPath $locres) -or (Get-Item -LiteralPath $locres).Length -lt 32) { throw "Missing localization resource: $culture" }
}
if ($RunTests) {
    $testStarted = Get-Date
    Assert-MiningLaunchResources -MinimumPhysicalGB 2.5 -MinimumCommitGB 8
    $testArgs = @(
        "$localizationRoot/BrainMining.uproject", '/Engine/Maps/Entry', '-unattended', '-nosplash', '-nullrhi', '-nosound', '-NoLiveCoding', '-corelimit=2',
        '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol', '-ExecCmds=Automation RunTests BrainMining.Localization+BrainMining.Achievements',
        '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$localizationReport/Tests",
        "-abslog=$localizationReport/tests.log", "-UserDir=$localizationReport/TestUser"
    )
    Start-MiningGuardedProcess -Executable "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" -Arguments $testArgs -ReportPath "$localizationReport/test-resources.csv" -TrimCommandletWorkingSet -MaximumSeconds 600
    $resultPath = "$localizationReport/Tests/index.json"
    if (-not (Test-Path -LiteralPath $resultPath) -or (Get-Item -LiteralPath $resultPath).LastWriteTime -lt $testStarted) { throw 'No fresh automation report was generated.' }
    $results = Get-Content -LiteralPath $resultPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($results.failed -gt 0 -or $results.notRun -gt 0 -or ($results.succeeded + $results.succeededWithWarnings) -lt 7) { throw 'Localization/achievement automation tests did not all pass.' }
}
