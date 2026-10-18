param(
    [string]$EngineRoot = "E:\UE5\UE_5.8",
    [string]$ArchiveDirectory = "",
    [string]$MapPath = "/Game/MainGame/Maps/Lvl_MainGame",
    [string]$AndroidSdkRoot = "",
    [string]$NdkRoot = "",
    [string]$JavaHome = "",
    [string]$TargetPlatform = "Android",
    [string]$AdditionalCookerOptions = "",
    [string]$CookFlavor = "",
    [switch]$SkipCook,
    [switch]$SkipBuild,
    [switch]$Distribution
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$RunUat = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path -LiteralPath $RunUat)) { throw "RunUAT.bat was not found: $RunUat" }

function Resolve-ExistingDirectory([string[]]$Candidates) {
    foreach ($Candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($Candidate) -and (Test-Path -LiteralPath $Candidate -PathType Container)) {
            return (Resolve-Path -LiteralPath $Candidate).Path
        }
    }
    return ""
}

if ([string]::IsNullOrWhiteSpace($AndroidSdkRoot)) {
    $AndroidSdkRoot = Resolve-ExistingDirectory @($env:ANDROID_HOME, $env:ANDROID_SDK_ROOT)
}
if ([string]::IsNullOrWhiteSpace($AndroidSdkRoot)) {
    throw "Android SDK was not found. Pass -AndroidSdkRoot or set ANDROID_HOME."
}
$env:ANDROID_HOME = $AndroidSdkRoot
$env:ANDROID_SDK_ROOT = $AndroidSdkRoot

if ([string]::IsNullOrWhiteSpace($NdkRoot)) {
    $SdkConfigPath = Join-Path $EngineRoot "Engine\Config\Android\Android_SDK.json"
    $RequiredNdk = ""
    if (Test-Path -LiteralPath $SdkConfigPath) {
        $RequiredNdk = (Get-Content -Raw -LiteralPath $SdkConfigPath | ConvertFrom-Json).ndk
    }
    $NdkCandidates = @()
    if (-not [string]::IsNullOrWhiteSpace($RequiredNdk)) {
        $NdkCandidates += Join-Path $AndroidSdkRoot "ndk\$RequiredNdk"
    }
    $NdkCandidates += @($env:NDKROOT, $env:NDK_ROOT)
    $NdkRoot = Resolve-ExistingDirectory $NdkCandidates
    if ([string]::IsNullOrWhiteSpace($NdkRoot)) {
        $NdkRoot = Get-ChildItem -Path (Join-Path $AndroidSdkRoot "ndk\*") -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            Select-Object -First 1 -ExpandProperty FullName
    }
}
if ([string]::IsNullOrWhiteSpace($NdkRoot) -or -not (Test-Path -LiteralPath $NdkRoot -PathType Container)) {
    throw "Android NDK was not found. Pass -NdkRoot or install the NDK required by UE."
}
$env:NDKROOT = (Resolve-Path -LiteralPath $NdkRoot).Path
$env:NDK_ROOT = $env:NDKROOT

if ([string]::IsNullOrWhiteSpace($JavaHome)) {
    $JavaCandidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Microsoft\jdk-17.0.10.7-hotspot"),
        (Join-Path $env:ProgramFiles "Java\jdk-17"),
        (Join-Path $env:ProgramFiles "Android\Android Studio\jbr")
    )
    $JavaHome = Resolve-ExistingDirectory @($JavaCandidates + $env:JAVA_HOME)
}
if ([string]::IsNullOrWhiteSpace($JavaHome) -or -not (Test-Path -LiteralPath (Join-Path $JavaHome "bin\java.exe"))) {
    throw "Java 17+ was not found. Pass -JavaHome or install a compatible JDK."
}
$env:JAVA_HOME = (Resolve-Path -LiteralPath $JavaHome).Path

Write-Host "Android SDK: $env:ANDROID_HOME"
Write-Host "Android NDK: $env:NDKROOT"
Write-Host "Java home: $env:JAVA_HOME"

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
    "-targetplatform=$TargetPlatform",
    "-clientconfig=Shipping",
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

if ($SkipCook) {
    $Arguments = @($Arguments[0..6] + "-skipcook" + $Arguments[7..($Arguments.Count - 1)])
} else {
    $Arguments = @($Arguments[0..6] + "-cook" + $Arguments[7..($Arguments.Count - 1)])
}
if (-not $SkipBuild) {
    $Arguments = @($Arguments[0..6] + "-build" + $Arguments[7..($Arguments.Count - 1)])
}

if (-not [string]::IsNullOrWhiteSpace($CookFlavor)) {
    $Arguments += "-cookflavor=$CookFlavor"
}

if (-not [string]::IsNullOrWhiteSpace($AdditionalCookerOptions)) {
    $Arguments += "-AdditionalCookerOptions=$AdditionalCookerOptions"
}

if ($Distribution) {
    $Arguments += "-distribution"
}

Write-Host "Packaging Android Mobile Vertical Slice"
Write-Host "Project: $ProjectFile"
Write-Host "Archive: $ArchiveDirectory"
& $RunUat @Arguments
if ($LASTEXITCODE -ne 0) { throw "Android packaging failed with exit code $LASTEXITCODE" }
