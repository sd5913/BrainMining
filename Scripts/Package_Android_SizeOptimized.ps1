param(
    [string]$EngineRoot = 'E:\UE5\UE_5.8',
    [string]$AndroidSdkRoot = "$env:LOCALAPPDATA\Android\Sdk",
    [string]$JavaHome = "$env:LOCALAPPDATA\Programs\Microsoft\jdk-17.0.10.7-hotspot",
    [string]$RunName = ('AndroidSizeOptimized-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 4 -MinimumCommitGB 8
if ($RunName -notmatch '^AndroidSizeOptimized-[A-Za-z0-9-]+$') { throw 'RunName must be a single AndroidSizeOptimized-* directory name.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$runRoot = Join-Path $projectRoot "Saved/Validation/SizeOptimization/$RunName"
$archive = Join-Path $projectRoot "Builds/MobileVerticalSlice/$RunName"
$stagingRoot = Join-Path $projectRoot 'Saved/StagedBuilds'
if ((Test-Path -LiteralPath $runRoot) -or (Test-Path -LiteralPath $archive)) { throw 'Use a new RunName for a fresh cook and staging directory.' }
$changes = Get-Content -LiteralPath (Join-Path $projectRoot 'Saved/Validation/SizeOptimization/content_changes.json') -Raw | ConvertFrom-Json
if (-not $changes.passed) { throw 'Run optimize_mobile_content.py successfully before packaging.' }
$sdk = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Config/Android/Android_SDK.json') -Raw | ConvertFrom-Json
$ndkRoot = Join-Path $AndroidSdkRoot "ndk/$($sdk.ndk)"
foreach ($directory in @($AndroidSdkRoot, $ndkRoot, $JavaHome)) {
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw "Missing SDK directory: $directory" }
}
$runtime = Get-ChildItem -LiteralPath (Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/DotNet') -Filter dotnet.exe -Recurse -File |
    Where-Object { $_.Directory.Name -eq 'win-x64' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $runtime) { throw 'Bundled dotnet runtime not found.' }
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
$cookOptions = "-corelimit=2 -NoAsyncLoadingThread -asyncassetcompilationmaxconcurrency=1 -asynctexturecompilationmaxconcurrency=1 -ini:Engine:[ConsoleVariables]:r.StaticMesh.StripMinLodDataDuringCooking=1 -ini:Editor:[CookSettings]:MemoryMinFreePhysical=3072,[CookSettings]:PackagesPerGC=5,[CookSettings]:MaxConcurrentShaderJobs=2 -LocalDataCachePath=$projectRoot/Saved/DerivedDataCache"
$cookOptions += ' -CustomConfig=AndroidSize'
$nativeMaterials = @(Get-ChildItem -LiteralPath "$projectRoot/Source", "$projectRoot/Plugins" -Recurse -File -Include '*.cpp', '*.h' | ForEach-Object {
    [regex]::Matches([IO.File]::ReadAllText($_.FullName), '/Game/MainGame/Materials/[A-Za-z0-9_./]+') | ForEach-Object { $_.Value.Split('.')[0] }
} | Sort-Object -Unique)
if (-not $nativeMaterials.Count) { throw 'No native material paths found; refusing a cook without explicit runtime roots.' }
$cookOptions += ' -PACKAGE=' + ($nativeMaterials -join '+')
$arguments = @(
    (Join-Path $EngineRoot 'Engine/Binaries/DotNET/AutomationTool/AutomationTool.dll'),
    "-ScriptsForProject=$projectRoot/BrainMining.uproject", '-NoCompile', '-Utf8Output',
    'BuildCookRun', "-project=$projectRoot/BrainMining.uproject", '-noP4', '-unattended',
    '-target=BrainMining', '-platform=Android', '-targetplatform=Android', '-clientconfig=Shipping',
    '-skipbuild', '-cook', '-stage', '-package', '-pak', '-iostore', '-compressed', '-nodebuginfo', '-skipeditorcontent',
    '-map=/Game/MainGame/Maps/Lvl_MainGame+/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo',
    "-CookOutputDir=$runRoot/Cooked/Android", "-stagingdirectory=$stagingRoot",
    '-archive', "-archivedirectory=$archive", "-AdditionalCookerOptions=$cookOptions"
)
$environmentNames = @('ANDROID_HOME', 'ANDROID_SDK_ROOT', 'NDKROOT', 'NDK_ROOT', 'JAVA_HOME', 'UE-LocalDataCachePath', 'uebp_LogFolder')
$previous = @{}
foreach ($name in $environmentNames) { $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $env:ANDROID_HOME = $AndroidSdkRoot
    $env:ANDROID_SDK_ROOT = $AndroidSdkRoot
    $env:NDKROOT = $ndkRoot
    $env:NDK_ROOT = $ndkRoot
    $env:JAVA_HOME = $JavaHome
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', "$projectRoot/Saved/DerivedDataCache", 'Process')
    [Environment]::SetEnvironmentVariable('uebp_LogFolder', "$runRoot/UAT", 'Process')
    @{run=$RunName; archive=$archive; started=(Get-Date).ToString('o'); native_materials=$nativeMaterials; arguments=$arguments} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$runRoot/request.json" -Encoding UTF8
    Start-MiningGuardedProcess -Executable $runtime.FullName -Arguments $arguments -ReportPath "$runRoot/resources.csv" -BelowNormal -TrimCookWorkingSet -MaximumSeconds 14400
    $package = Get-ChildItem -LiteralPath $archive -Filter 'BrainMining-Android-Shipping-arm64.apk' -File -Recurse | Select-Object -First 1
    if (-not $package) { throw 'Packaging completed without the main APK.' }
    & (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealPak.exe') "-ListContainer=$stagingRoot/Android/BrainMining/Content/Paks/BrainMining-Android.utoc" "-Csv=$runRoot/android_iostore.csv" -unattended *> "$runRoot/container_listing.log"
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the cooked container.' }
    & python (Join-Path $PSScriptRoot 'verify_android_package.py') --apk $package.FullName --stage "$stagingRoot/Android" --report "$runRoot/package_integrity.json" --evidence "$runRoot/Staged" --iostore-csv "$runRoot/android_iostore.csv" --required-json "$runRoot/request.json"
    if ($LASTEXITCODE -ne 0) { throw 'APK container verification failed; do not distribute this package.' }
    @{passed=$true; apk=$package.FullName; staging_root="$stagingRoot/Android"; bytes=$package.Length; sha256=(Get-FileHash -LiteralPath $package.FullName -Algorithm SHA256).Hash; completed=(Get-Date).ToString('o')} |
        ConvertTo-Json | Set-Content -LiteralPath "$runRoot/result.json" -Encoding UTF8
    Write-Host ('Optimized package: {0} ({1:N2} MiB)' -f $package.FullName, ($package.Length / 1MB))
} finally {
    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process') }
}
