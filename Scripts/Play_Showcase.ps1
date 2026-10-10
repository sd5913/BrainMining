param(
    [string]$EngineRoot = 'E:\UE5\UE_5.8',
    [switch]$Editor
)
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'BrainMining.uproject'
$executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw "Unreal Editor not found: $executable. Supply -EngineRoot." }
$arguments = @($project, '/Game/Showcase/Maps/Lvl_ContinuousShowcase', '-NoSplash')
if (-not $Editor) { $arguments += @('-game', '-windowed', '-ResX=1600', '-ResY=900') }
& $executable @arguments
