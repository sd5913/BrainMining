param(
    [string]$EngineRoot = 'E:\UE5\UE_5.8',
    [switch]$SkipBuild,
    [string]$TestFilter = 'BrainMining.Gamepad+BrainMining.Flight+BrainMining.MaterialProgression.AtomicUpgrade+BrainMining.Opening.MenuLifecycle'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'BrainMining.uproject'
$report = Join-Path $projectRoot ('Saved/Validation/Gamepad/{0}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (-not $SkipBuild) {
    Assert-MiningLaunchResources -MinimumPhysicalGB 4 -MinimumCommitGB 8
    $runtime = Get-ChildItem -LiteralPath (Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/DotNet') -Filter dotnet.exe -Recurse -File |
        Where-Object { $_.Directory.Name -eq 'win-x64' } | Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $runtime) { throw 'Bundled .NET runtime was not found.' }
    $buildArguments = @(
        (Join-Path $EngineRoot 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'),
        'BrainMiningEditor', 'Win64', 'Development', "-Project=$project", "-Log=$report/build.log", '-WaitMutex', '-NoHotReloadFromIDE',
        '-MaxParallelActions=1', '-NoUBA', '-NoXGE', '-NoFASTBuild', '-NoSNDBS'
    )
    Start-MiningGuardedProcess -Executable $runtime.FullName -Arguments $buildArguments -ReportPath "$report/build-resources.csv" -BelowNormal -MaximumSeconds 1800
}
Assert-MiningLaunchResources
$runArguments = @(
    $project, '/Engine/Maps/Entry', '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-nullrhi', '-NoAsyncLoadingThread',
    '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol', "-UserDir=$report/User",
    "-ExecCmds=Slate.SkipWidgetDrawingInHeadlessMode 0,Automation RunTests $TestFilter", '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$report", "-abslog=$report/runtime.log", "-LocalDataCachePath=$projectRoot/Saved/DerivedDataCache",
    '-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()'
)
Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -Arguments $runArguments -ReportPath "$report/resources.csv" -BelowNormal -MaximumSeconds 600
$result = Get-Content -LiteralPath "$report/index.json" -Raw | ConvertFrom-Json
if ($result.failed -ne 0 -or ($result.succeeded + $result.succeededWithWarnings) -lt 1) { throw "Gamepad validation failed; see $report/index.json" }
Write-Host "PASS: $($result.succeeded + $result.succeededWithWarnings) tests. Report: $report/index.json"
