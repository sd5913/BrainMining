param([string]$Script = 'inspect_routes_v5.py', [switch]$Render)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot = Split-Path -Parent $PSScriptRoot
$runArgs = @("$projectRoot\BrainMining.uproject", '/Engine/Maps/Entry', '-unattended', '-nosplash', '-nosound', '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities', '-NoLiveCoding', "-ExecutePythonScript=$projectRoot\Scripts\$Script", "-abslog=$projectRoot\Saved\Validation\MainGameRoutesV5\$Script.log")
if ($Script -eq 'NativeTests') {
    $runArgs = @($runArgs | Where-Object { $_ -notlike '-ExecutePythonScript=*' })
    $runArgs += @('-ExecCmds=Automation RunTests BrainMining.Bomb+BrainMining.MaterialProgression+BrainMining.Pickaxe.Balance+BrainMining.Pickaxe.EditorConfigPersistence+BrainMining.Pickaxe.Hardness', '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$projectRoot\Saved\Automation\MainGameRoutesV5")
}
if (!$Render) { $runArgs += '-nullrhi' }
else { $runArgs += @('-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=False', '-ExecCmds=sg.ViewDistanceQuality 0,sg.ShadowQuality 0,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0,sg.TextureQuality 0,r.Shadow.Virtual.Enable 0,r.Streaming.PoolSize 256') }
Start-MiningGuardedProcess -Executable 'H:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -Arguments $runArgs -ReportPath "$projectRoot\Saved\Performance\routes-v5-resources.csv" -MaximumSeconds 2400
