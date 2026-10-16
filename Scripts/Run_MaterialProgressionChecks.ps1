param([ValidateSet('Author','Runtime','Tests','Verify','HUD')][string]$Mode = 'Tests', [switch]$Render, [switch]$LowMemoryRender)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot = Split-Path -Parent $PSScriptRoot
$editorExe = 'H:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$runArgs = @((Join-Path $projectRoot 'BrainMining.uproject'), '/Engine/Maps/Entry', '-unattended', '-nosplash', '-nosound', '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities', '-NoLiveCoding')
if (-not $Render) { $runArgs += '-nullrhi' }
elseif ($LowMemoryRender) {
    # Session-only overrides; keep the project's saved rendering settings intact.
    $runArgs += @('-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=False', '-ExecCmds=sg.ViewDistanceQuality 0,sg.ShadowQuality 0,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0,sg.TextureQuality 0,sg.EffectsQuality 0,r.Shadow.Virtual.Enable 0,r.Streaming.PoolSize 256')
}
if ($Mode -eq 'Tests') {
    $runArgs += @('-ExecCmds=Automation RunTests BrainMining.MaterialProgression+BrainMining.Pickaxe.Balance+BrainMining.Pickaxe.EditorConfigPersistence+BrainMining.Pickaxe.Hardness', '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$projectRoot\Saved\Automation\MaterialProgression")
} else {
    $scriptName = switch ($Mode) { 'Author' { 'apply_main_game_material_progression.py' }; 'Runtime' { 'verify_main_game_material_runtime.py' }; 'Verify' { 'verify_main_game_material_saved.py' }; 'HUD' { 'verify_material_hud.py' } }
    $runArgs += "-ExecutePythonScript=$projectRoot\Scripts\$scriptName"
}
$runArgs += "-abslog=$projectRoot\Saved\Validation\MaterialProgression\$Mode.log"
$report = Join-Path $projectRoot "Saved\Performance\materials-$Mode-resources.csv"
$validationPath = if ($Mode -eq 'Tests') { Join-Path $projectRoot 'Saved\Automation\MaterialProgression\index.json' } else {
    $reportName = switch ($Mode) { 'Author' { 'authoring.json' }; 'Verify' { 'saved.json' }; 'Runtime' { 'runtime.json' }; 'HUD' { 'hud.json' } }
    Join-Path $projectRoot "Saved\Validation\MaterialProgression\$reportName"
}
$startedAt = [DateTime]::UtcNow
Start-MiningGuardedProcess -Executable $editorExe -Arguments $runArgs -ReportPath $report -MaximumSeconds 1200
if (-not (Test-Path -LiteralPath $validationPath) -or (Get-Item -LiteralPath $validationPath).LastWriteTimeUtc -lt $startedAt) {
    throw "The run did not produce a fresh validation report: $validationPath"
}
$validation = Get-Content -LiteralPath $validationPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Mode -eq 'Tests') {
    if ($validation.failed -gt 0 -or $validation.notRun -gt 0 -or $validation.inProcess -gt 0 -or $validation.succeeded -lt 7) { throw "Native checks failed: $validationPath" }
} elseif (-not $validation.passed) { throw "Validation failed: $validationPath" }
Write-Host "Validation passed: $validationPath"
