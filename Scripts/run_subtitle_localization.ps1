param(
    [ValidateSet('audit', 'apply', 'verify')][string]$Mode = 'audit',
    [string]$EngineRoot = 'E:/UE5/UE_5.8'
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/ResourceGuard.ps1"
$subtitleRoot = Split-Path -Parent $PSScriptRoot
$subtitleReport = Join-Path $subtitleRoot 'Saved/Validation/LocalizationAchievements'
$started = Get-Date
$previousMode = [Environment]::GetEnvironmentVariable('BM_LOCALIZATION_MODE', 'Process')
Assert-MiningLaunchResources -MinimumPhysicalGB 2.5 -MinimumCommitGB 8
try {
    [Environment]::SetEnvironmentVariable('BM_LOCALIZATION_MODE', $Mode, 'Process')
    Start-MiningGuardedProcess -Executable "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" -Arguments @(
        "$subtitleRoot/BrainMining.uproject", '-run=pythonscript', "-script=$subtitleRoot/Scripts/localize_subtitle_assets.py",
        '-unattended', '-nosplash', '-nullrhi', '-nosound', '-NoLiveCoding', '-corelimit=2',
        '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol',
        "-abslog=$subtitleReport/subtitle-$Mode.log", "-UserDir=$subtitleReport/SubtitleUser"
    ) -ReportPath "$subtitleReport/subtitle-$Mode-resources.csv" -TrimCommandletWorkingSet -MaximumSeconds 240
} finally {
    [Environment]::SetEnvironmentVariable('BM_LOCALIZATION_MODE', $previousMode, 'Process')
}
$manifestPath = Join-Path $subtitleReport 'subtitle-assets.json'
if (-not (Test-Path -LiteralPath $manifestPath) -or (Get-Item -LiteralPath $manifestPath).LastWriteTime -lt $started) { throw 'No fresh subtitle localization report was generated.' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($manifest.mode -ne $Mode -or $manifest.errors.Count -or -not $manifest.lines.Count) { throw 'Subtitle localization audit did not pass. Inspect subtitle-assets.json.' }
Write-Host "Subtitle localization $Mode succeeded: $($manifest.lines.Count) lines."
