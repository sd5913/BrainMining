param(
    [string]$EngineRoot = 'H:/UE_5.8',
    [string]$OutputDirectory = '',
    [switch]$SkipBuild,
    [switch]$SkipCook
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 6 -MinimumCommitGB 8
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot ('Builds/MainGame-Windows-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$report = Join-Path $projectRoot 'Saved/Validation/MainGamePackage'
New-Item -ItemType Directory -Path $report -Force | Out-Null
if ((Get-PSDrive -Name ([IO.Path]::GetPathRoot($OutputDirectory).Substring(0,1))).Free -lt 20GB) { throw 'Packaging requires at least 20 GB free on the output drive.' }
$dotnet = Get-ChildItem "$EngineRoot/Engine/Binaries/ThirdParty/DotNet" -Filter dotnet.exe -Recurse -File | Where-Object { $_.Directory.Name -eq 'win-x64' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $dotnet) { throw 'Bundled .NET runtime not found.' }
# Commandlet cooks ignore percentage-based shader limits. Bound all asset work as well,
# since the finale's 8K crowd textures can each require several GB of temporary memory.
$argsUat = @(
    "$EngineRoot/Engine/Binaries/DotNET/AutomationTool/AutomationTool.dll",
    "-ScriptsForProject=$projectRoot/BrainMining.uproject", '-NoCompile', '-Utf8Output',
    'BuildCookRun', "-project=$projectRoot/BrainMining.uproject", '-NoP4', '-unattended',
    '-target=BrainMining', '-platform=Win64', '-clientconfig=Shipping',
    '-stage', '-pak', '-iostore', '-compressed',
    '-prereqs', '-nodebuginfo', '-archive', "-archivedirectory=$OutputDirectory",
    '-map=/Game/MainGame/Maps/Lvl_MainGame+/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo',
    '-UbtArgs=-MaxParallelActions=2 -NoUBA -NoXGE -NoFASTBuild -NoSNDBS',
    "-AdditionalCookerOptions=-LocalDataCachePath=$projectRoot/Saved/DerivedDataCache -corelimit=4 -asyncassetcompilationmaxconcurrency=1 -asynctexturecompilationmaxconcurrency=1 -ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=2 -ini:Editor:[CookSettings]:MemoryMinFreePhysical=4096"
)
if ($SkipBuild) { $argsUat += '-skipbuild' } else { $argsUat += '-build' }
if ($SkipCook) { $argsUat += '-skipcook' } else { $argsUat += '-cook' }
@{output=$OutputDirectory;started=(Get-Date).ToString('o');configuration='Shipping';arguments=$argsUat} | ConvertTo-Json -Depth 4 | Set-Content "$report/request.json" -Encoding UTF8
# Keep derived data on the project drive in all child tools, without changing user settings.
$previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
$previousLogs = [Environment]::GetEnvironmentVariable('uebp_LogFolder', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', "$projectRoot/Saved/DerivedDataCache", 'Process')
    [Environment]::SetEnvironmentVariable('uebp_LogFolder', "$report/UAT", 'Process')
    Start-MiningGuardedProcess -Executable $dotnet.FullName -Arguments $argsUat -ReportPath "$report/package-resources.csv" -BelowNormal -MaximumSeconds 14400
} finally {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
    [Environment]::SetEnvironmentVariable('uebp_LogFolder', $previousLogs, 'Process')
}
if (-not (Test-Path "$OutputDirectory/Windows/BrainMining.exe")) { throw "Packaged launcher missing: $OutputDirectory" }
Write-Host "MainGame Shipping package: $OutputDirectory/Windows/BrainMining.exe"
