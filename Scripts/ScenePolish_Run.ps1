param([string]$Script='inspect_scene_polish.py',[switch]$Render,[switch]$Baseline,[switch]$LowMemory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot=Split-Path -Parent $PSScriptRoot
$report=Join-Path $projectRoot 'Saved/Validation/ScenePolish'
New-Item -ItemType Directory -Path $report -Force | Out-Null
$runArgs=@("$projectRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol',"-UserDir=$report/User","-ExecutePythonScript=$projectRoot/Scripts/$Script","-abslog=$report/$Script.log","-LocalDataCachePath=$projectRoot/Saved/DerivedDataCache",'-NoAsyncLoadingThread')
if($Baseline){$runArgs+='-Baseline'}
if($LowMemory){$runArgs+=@('-noraytracing','-nosound','-ExecCmds=r.Lumen.HardwareRayTracing 0,r.Lumen.TraceMeshSDFs 1,r.Streaming.PoolSize 384,r.Shadow.Virtual.MaxPhysicalPages 1024,r.LumenScene.SurfaceCache.AtlasSize 1024')}
if($Render){$runArgs+=@('-RenderOffscreen','-windowed','-ResX=1600','-ResY=1000','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()')}
else{$runArgs+=@('-nullrhi','-nosound')}
$started=Get-Date
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $runArgs -ReportPath "$report/$Script.resources.csv" -MaximumSeconds 900
$reports=@{'finish_scene_polish_clue.py'='clue_finish.json';'finish_scene_polish_lighting.py'='lighting_finish.json';'apply_scene_polish.py'='authoring.json';'add_scene_polish_malachite.py'='malachite.json';'finish_scene_polish_surfaces.py'='surfaces.json';'finish_scene_polish_assets.py'='asset_finish.json';'prepare_scene_polish_duck.py'='duck.json';'capture_scene_polish_collectibles.py'='collectible_visual.json';'verify_scene_polish.py'='runtime.json'}
$reports['fit_scene_polish_heaven.py']='heaven_fit.json'
$reports['repair_scene_polish_mineral.py']='heaven_mineral.json'
$reports['finish_scene_polish_heaven_entry.py']='heaven_entry.json'
$reports['finish_scene_polish_heaven_outer.py']='heaven_outer.json'
$reports['apply_scene_feedback.py']='../SceneFeedback/authoring.json'
$reports['verify_scene_polish.py']='../SceneFeedback/runtime.json'
if($reports.ContainsKey($Script)){
 $resultPath=Join-Path $report $reports[$Script]
 if(!(Test-Path -LiteralPath $resultPath) -or (Get-Item -LiteralPath $resultPath).LastWriteTime -lt $started){throw "No fresh report: $resultPath"}
 $result=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
 if(!$result.passed){throw "Validation failed: $($result.error)"}
 Write-Host "PASS $Script"
}
