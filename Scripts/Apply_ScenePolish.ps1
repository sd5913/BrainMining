# Build the current C++ with Build_Safe.ps1 first. Each authoring stage saves a report.
param([switch]$Validate)
$ErrorActionPreference='Stop'
foreach($stage in @('apply_scene_polish.py','add_scene_polish_malachite.py','finish_scene_polish_surfaces.py','finish_scene_polish_assets.py','prepare_scene_polish_duck.py','finish_scene_polish_clue.py','finish_scene_polish_lighting.py','fit_scene_polish_heaven.py','repair_scene_polish_mineral.py','finish_scene_polish_heaven_entry.py','finish_scene_polish_heaven_outer.py','apply_scene_feedback.py')) {
 & (Join-Path $PSScriptRoot 'ScenePolish_Run.ps1') -Script $stage
}
if($Validate){
 & (Join-Path $PSScriptRoot 'verify_optimization_round3.ps1') -Mode Card
 & (Join-Path $PSScriptRoot 'verify_optimization_round3.ps1') -Mode Office
 & (Join-Path $PSScriptRoot 'ScenePolish_Run.ps1') -Script capture_scene_polish_collectibles.py -Render
 & (Join-Path $PSScriptRoot 'ScenePolish_Run.ps1') -Script verify_scene_polish.py -Render -LowMemory
}
