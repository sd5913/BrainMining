param([switch]$ImportAssets,[switch]$Screenshots)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$museumRoot=Split-Path -Parent $PSScriptRoot
$report=Join-Path $museumRoot 'Saved/Validation/MouseMuseum'
New-Item -ItemType Directory -Path $report -Force | Out-Null
$runArgs=@("$museumRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-nosound','-NoLiveCoding','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol')
$runArgs+='-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()'
if($ImportAssets){$runArgs+=@("-ExecutePythonScript=$museumRoot/Scripts/import_mouse_museum.py","-abslog=$report/import.log")}
else{$runArgs+=@('-ExecCmds=Automation RunTests BrainMining.Campaign.MousePaintingEndingTravel+BrainMining.Ending.PainterlyStageScope','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report","-abslog=$report/runtime.log")}
if($Screenshots){$runArgs+=@('-MouseMuseumScreenshots','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720')}
else{$runArgs+='-nullrhi'}
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $runArgs -ReportPath "$report/resources.csv" -BelowNormal -MaximumSeconds 900
if($ImportAssets){if(-not (Select-String -Path "$report/import.log" -Pattern 'MOUSE_MUSEUM_ASSETS_READY' -Quiet)){throw 'Asset import did not finish.'}}
else{$result=Get-Content "$report/index.json" -Raw | ConvertFrom-Json;if($result.failed -ne 0 -or ($result.succeeded+$result.succeededWithWarnings) -lt 2){throw 'Mouse museum checks failed.'}}
