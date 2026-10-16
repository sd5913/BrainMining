param(
    [string]$EngineRoot = "E:\UE5\UE_5.8",
    [string]$ArchiveDirectory = "",
    [string]$MapPath = "/Game/Campaign/Maps/Lvl_FullJourney",
    [switch]$Distribution
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$RunUat = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path -LiteralPath $RunUat)) { throw "RunUAT.bat was not found: $RunUat" }

if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
    $ArchiveDirectory = Join-Path $ProjectRoot "Builds\MobileVerticalSlice\Android"
}

New-Item -ItemType Directory -Force -Path $ArchiveDirectory | Out-Null
$ProjectFile = Join-Path $ProjectRoot "BrainMining.uproject"
$Arguments = @(
    "BuildCookRun",
    "-project=$ProjectFile",
    "-noP4",
    "-utf8output",
    "-platform=Android",
    "-targetplatform=Android",
    "-clientconfig=Shipping",
    "-cook",
    "-build",
    "-stage",
    "-package",
    "-pak",
    "-iostore",
    "-compressed",
    "-nodebuginfo",
    "-skipeditorcontent",
    "-map=$MapPath",
    "-archive",
    "-archivedirectory=$ArchiveDirectory"
)

if ($Distribution) {
    $Arguments += "-distribution"
}

Write-Host "Packaging Android Mobile Vertical Slice"
Write-Host "Project: $ProjectFile"
Write-Host "Archive: $ArchiveDirectory"
& $RunUat @Arguments
if ($LASTEXITCODE -ne 0) { throw "Android packaging failed with exit code $LASTEXITCODE" }
