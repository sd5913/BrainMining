param(
    [string]$EngineRoot = 'E:/UE5/UE_5.8',
    [ValidateSet('Both', 'Baseline', 'Optimized')][string]$Mode = 'Both',
    [string]$RunName = ('PCSize-' + (Get-Date -Format 'yyyyMMdd-HHmmss')),
    [string]$Python = 'python',
    [switch]$PlanOnly
)

$ErrorActionPreference = 'Stop'
if ($RunName -notmatch '^PCSize-[A-Za-z0-9-]+$') { throw 'RunName must be a single PCSize-* directory name.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$runRoot = Join-Path $projectRoot "Saved/Validation/PCSize/$RunName"
$archiveRoot = Join-Path $projectRoot "Builds/$RunName"
if ((Test-Path -LiteralPath $runRoot) -or (Test-Path -LiteralPath $archiveRoot)) { throw 'Use a new RunName; existing builds and reports are preserved.' }
if (-not (Get-Command $Python -ErrorAction SilentlyContinue)) { throw "Python was not found: $Python" }
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
$variants = if ($Mode -eq 'Both') { @('Baseline', 'Optimized') } else { @($Mode) }
$completed = @()
try {
    foreach ($variant in $variants) {
        $report = Join-Path $runRoot $variant
        $archive = Join-Path $archiveRoot $variant
        $parameters = @{
            EngineRoot = $EngineRoot
            OutputDirectory = $archive
            ReportDirectory = $report
            CookOutputDirectory = (Join-Path $report 'Cooked/Windows')
            StagingDirectory = (Join-Path $report 'Stage')
            PlanOnly = $PlanOnly
        }
        if ($variant -eq 'Optimized') { $parameters.CustomConfig = 'PCSize' }
        & (Join-Path $PSScriptRoot 'package_main_game.ps1') @parameters
        if (-not $PlanOnly) {
            $reportArgs = @((Join-Path $PSScriptRoot 'report_pc_package.py'), '--archive', "$archive/Windows", '--report', $report, '--zip', "$archive/BrainMining-Windows.zip")
            if ($Mode -eq 'Both' -and $variant -eq 'Optimized') { $reportArgs += @('--baseline', "$runRoot/Baseline/result.json") }
            & $Python @reportArgs
            if ($LASTEXITCODE -ne 0) { throw "Package verification failed: $report" }
        }
        $completed += $variant
    }
    @{status=$(if ($PlanOnly) { 'planned' } else { 'packaged_pending_gameplay_review' });completed=$completed;archive=$archiveRoot;report=$runRoot;time=(Get-Date).ToString('o')} |
        ConvertTo-Json | Set-Content -LiteralPath "$runRoot/status.json" -Encoding UTF8
} catch {
    @{status='failed';completed=$completed;error=$_.Exception.Message;report=$runRoot;time=(Get-Date).ToString('o')} |
        ConvertTo-Json | Set-Content -LiteralPath "$runRoot/status.json" -Encoding UTF8
    throw
}
Write-Host "PC package reports: $runRoot"
