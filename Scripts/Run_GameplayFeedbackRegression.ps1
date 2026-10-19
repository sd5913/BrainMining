param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$feedbackRoot=Split-Path -Parent $PSScriptRoot
$feedbackReport=Join-Path $feedbackRoot 'Saved/Validation/GameplayFeedback/Regression'
New-Item -ItemType Directory -Force -Path $feedbackReport | Out-Null
# The office pickup samples input on a 60 Hz timer; cap headless frames so a
# one-frame E press remains visible for at least one timer tick.
$feedbackArgs=@("$feedbackRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-nosound','-NoLiveCoding','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()','-ExecCmds=t.MaxFPS 30,Automation RunTests BrainMining.Campaign.MousePaintingEndingTravel+BrainMining.Ending.PainterlyStageScope+BrainMining.Office.ExitFlow','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$feedbackReport","-abslog=$feedbackReport/runtime.log",'-nullrhi')
$feedbackStarted=Get-Date
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $feedbackArgs -ReportPath "$feedbackReport/resources.csv" -BelowNormal -MaximumSeconds 900
$feedbackFile=Get-Item -LiteralPath "$feedbackReport/index.json"
if($feedbackFile.LastWriteTime -lt $feedbackStarted){throw 'No fresh regression report.'}
$feedbackResult=Get-Content -LiteralPath $feedbackFile.FullName -Raw | ConvertFrom-Json
if($feedbackResult.failed -ne 0 -or ($feedbackResult.succeeded+$feedbackResult.succeededWithWarnings) -ne 3){throw 'Gameplay feedback regression checks failed.'}
