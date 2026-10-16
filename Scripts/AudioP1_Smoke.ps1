param([ValidateSet('Capture', 'CampaignUI', 'SettingsUI')][string]$Mode = 'Capture', [switch]$Headless, [string]$EngineRoot = 'E:\UE5\UE_5.8')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot = Split-Path -Parent $PSScriptRoot
$map = if ($Mode -eq 'Capture') { '/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo' } else { '/Game/Campaign/Maps/Lvl_FullJourney' }
if ($Mode -eq 'SettingsUI') { $map = '/Engine/Maps/Entry?game=/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C' }
$commands = @('t.MaxFPS 30', 'sg.GlobalIlluminationQuality 1', 'sg.ReflectionQuality 1', 'sg.ShadowQuality 1', 'sg.TextureQuality 1', 'sg.EffectsQuality 1', 'r.ScreenPercentage 65')
if ($Mode -eq 'Capture') { $commands += @('au.DisableAppVolume 1', 'bm.Audio.Capture', 'csvprofile frames=1050') }
elseif ($Mode -eq 'SettingsUI') { $commands += @('DisableAllScreenMessages', 'bm.Audio.SettingsPreview', 'csvprofile frames=300') }
else { $commands += 'csvprofile frames=3600' }
$label = if ($Headless) { "$Mode-Headless" } else { $Mode }
$arguments = @("$projectRoot\BrainMining.uproject", $map, '-game', '-windowed', '-ResX=960', '-ResY=720', '-NoSplash', '-AudioMixer', '-ExitAfterCsvProfiling', ('-ExecCmds=' + ($commands -join ',')), "-abslog=$projectRoot\Saved\Logs\AudioP1$label.log")
if ($Headless) { $arguments += '-nullrhi' }
Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') -Arguments $arguments -ReportPath "$projectRoot\Saved\Performance\audio-p1-$label.csv" -BelowNormal -MaximumSeconds 180
