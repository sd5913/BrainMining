param([string]$Script='inspect_gameplay_feedback.py',[switch]$Render)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$feedbackRoot=Split-Path -Parent $PSScriptRoot
$feedbackOut=Join-Path $feedbackRoot 'Saved/Validation/GameplayFeedback'
New-Item -ItemType Directory -Force -Path $feedbackOut | Out-Null
$feedbackStem=[System.IO.Path]::GetFileName($Script)
$feedbackArgs=@("$feedbackRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-nosound','-NoLiveCoding','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-ExecutePythonScript=$PSScriptRoot/$Script","-abslog=$feedbackOut/$feedbackStem.log")
if($Render){$feedbackArgs+=@('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720')}else{$feedbackArgs+='-nullrhi'}
$feedbackStarted=Get-Date
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $feedbackArgs -ReportPath "$feedbackOut/$feedbackStem.resources.csv" -BelowNormal -MaximumSeconds 900
if(Select-String -LiteralPath "$feedbackOut/$feedbackStem.log" -Pattern 'LogPython: Error:' -Quiet){throw 'Unreal Python script reported an error.'}
$feedbackReportName=switch($Script){'apply_gameplay_feedback.py' {'authoring.json'} 'verify_gameplay_feedback.py' {'runtime.json'} default {''}}
if($feedbackReportName){
    $feedbackFile=Get-Item -LiteralPath "$feedbackOut/$feedbackReportName"
    if($feedbackFile.LastWriteTime -lt $feedbackStarted){throw 'No fresh gameplay feedback report.'}
    $feedbackResult=Get-Content -LiteralPath $feedbackFile.FullName -Raw | ConvertFrom-Json
    if(-not $feedbackResult.passed){throw "Gameplay feedback checks failed: $($feedbackResult.error)"}
}
