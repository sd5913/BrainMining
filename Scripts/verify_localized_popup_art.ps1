param([string]$EngineRoot = 'E:/UE5/UE_5.8')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/ResourceGuard.ps1"
$projectRoot = Split-Path -Parent $PSScriptRoot
$reportRoot = Join-Path $projectRoot 'Saved/Validation/LocalizationAchievements'
$started = Get-Date
Assert-MiningLaunchResources -MinimumPhysicalGB 2.5 -MinimumCommitGB 8
Start-MiningGuardedProcess -Executable "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" -Arguments @(
    "$projectRoot/BrainMining.uproject", '/Engine/Maps/Entry', '-unattended', '-RenderOffscreen', '-d3d11',
    '-nosplash', '-nosound', '-NoLiveCoding', '-corelimit=2', '-asyncassetcompilationmaxconcurrency=1',
    '-asynctexturecompilationmaxconcurrency=1', '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol',
    '-ExecCmds=Automation RunTests BrainMining.Rendering.LocalizedPopupArtwork', '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$reportRoot/RenderTests", "-abslog=$reportRoot/popup-render.log", "-UserDir=$reportRoot/RenderUser"
) -ReportPath "$reportRoot/popup-render-resources.csv" -TrimCommandletWorkingSet -MaximumSeconds 300
$resultPath = "$reportRoot/RenderTests/index.json"
if (-not (Test-Path -LiteralPath $resultPath) -or (Get-Item -LiteralPath $resultPath).LastWriteTime -lt $started) { throw 'No fresh render test report was generated.' }
$results = Get-Content -LiteralPath $resultPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($results.failed -gt 0 -or $results.notRun -gt 0 -or ($results.succeeded + $results.succeededWithWarnings) -ne 1) { throw 'Localized popup artwork did not pass.' }
$images = "$reportRoot/RenderUser/Saved/Validation/LocalizationAchievements/PopupRenders"
foreach ($culture in @('zh-Hans', 'en', 'zh-Hant')) {
    foreach ($variant in 0..2) {
        $image = Get-Item -LiteralPath "$images/$culture-$variant.png"
        if ($image.LastWriteTime -lt $started -or $image.Length -lt 5000) { throw "Missing fresh render: $culture-$variant" }
    }
}
Write-Host "Nine localized artwork previews: $images"
