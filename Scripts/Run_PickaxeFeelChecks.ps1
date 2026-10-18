param([switch]$Native,[switch]$Render)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$pickaxeRoot=Split-Path -Parent $PSScriptRoot
$pickaxeArgs=@("$pickaxeRoot\BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosound','-nosplash','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$pickaxeRoot\Saved\Validation\PickaxeFeel\User-$(Get-Date -Format yyyyMMdd-HHmmss)","-abslog=$pickaxeRoot\Saved\Logs\PickaxeFeel.log")
if($Native){$pickaxeArgs+=@('-nullrhi','-ExecCmds=Automation RunTests BrainMining.Pickaxe+BrainMining.MaterialProgression+BrainMining.Progression','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$pickaxeRoot\Saved\Validation\PickaxeFeel\Native")}
else{
    $pickaxeArgs+="-ExecutePythonScript=$PSScriptRoot\verify_pickaxe_feel.py"
    if($Render){$pickaxeArgs+=@('-RenderOffscreen','-windowed','-ResX=1600','-ResY=900')}else{$pickaxeArgs+='-nullrhi'}
}
$pickaxeStart=Get-Date
Start-MiningGuardedProcess -Executable 'C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -Arguments $pickaxeArgs -ReportPath "$pickaxeRoot\Saved\Performance\PickaxeFeel.csv" -MaximumSeconds 480
$pickaxeFile=if($Native){'Native/index.json'}else{'runtime.json'}
$pickaxeReport=Get-Item -LiteralPath "$pickaxeRoot/Saved/Validation/PickaxeFeel/$pickaxeFile"
$pickaxeResult=Get-Content -LiteralPath $pickaxeReport.FullName -Raw|ConvertFrom-Json
if($pickaxeReport.LastWriteTime -lt $pickaxeStart){throw 'No fresh verification report.'}
if($Native){if($pickaxeResult.failed -gt 0 -or $pickaxeResult.notRun -gt 0){throw 'Native pickaxe verification failed.'}}
elseif(-not $pickaxeResult.passed){throw $pickaxeResult.error}
