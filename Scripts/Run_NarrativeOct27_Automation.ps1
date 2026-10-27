param([string]$RunName='NarrativeOct27-Regression')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root ('Saved/Validation/'+$RunName)
New-Item -ItemType Directory -Path $out -Force | Out-Null
$argsUE=@("$root/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-nosound','-RenderOffscreen','-windowed','-ResX=1280','-ResY=800','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$out/profile",'-ExecCmds=mining.SaveEnabled 0,Automation RunTests BrainMining.Office.Art.PopupButtons','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$out","-abslog=$out/editor.log")
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $argsUE -ReportPath "$out/resources.csv" -MaximumSeconds 300
$report=Get-Content -LiteralPath "$out/index.json" -Raw | ConvertFrom-Json
if($report.failed -ne 0 -or $report.succeeded -lt 1){throw 'Office narrative automation failed.'}
