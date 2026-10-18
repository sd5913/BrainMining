param([string]$EngineRoot = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 6 -MinimumCommitGB 8
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $EngineRoot) {
    foreach ($candidate in @($env:UE_ENGINE_ROOT, 'C:\UE_5.8', 'E:\UE5\UE_5.8')) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'))) {
            $EngineRoot = $candidate
            break
        }
    }
}
if (-not $EngineRoot) { throw 'UE 5.8 not found. Supply -EngineRoot or set UE_ENGINE_ROOT.' }
$dotnet = Get-ChildItem -LiteralPath (Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet') -Filter dotnet.exe -Recurse -File |
    Where-Object { $_.Directory.Name -eq 'win-x64' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $dotnet) { throw 'Bundled win-x64 .NET runtime not found.' }
$arguments = @(
    (Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'),
    'BrainMiningEditor', 'Win64', 'Development', "-Project=$projectRoot\BrainMining.uproject",
    '-WaitMutex', '-NoHotReloadFromIDE', '-MaxParallelActions=12', '-NoUBA', '-NoXGE', '-NoFASTBuild', '-NoSNDBS'
)
$reportPath = Join-Path $projectRoot ('Saved\Performance\build-resources-{0}.csv' -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
Start-MiningGuardedProcess -Executable $dotnet.FullName -Arguments $arguments -ReportPath $reportPath -BelowNormal -MaximumSeconds 1800
