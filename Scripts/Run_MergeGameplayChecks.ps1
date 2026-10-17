param([ValidateSet('Native','Flow','Office','P2','HUD','Map')][string]$Mode='Native', [string]$EngineRoot='C:\UE_5.8')
$ErrorActionPreference='Stop'
if($Mode -eq 'Flow') {
    # Separate processes prevent the campaign autosave from one PIE map from
    # granting inventory or unlocks to the next test's fresh-start fixture.
    & $PSCommandPath -Mode Office -EngineRoot $EngineRoot
    & $PSCommandPath -Mode P2 -EngineRoot $EngineRoot
    return
}
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$repairRoot=Split-Path -Parent $PSScriptRoot
$repairOut=Join-Path $repairRoot 'Saved/Validation/MergeGameplay'
New-Item -ItemType Directory -Path $repairOut -Force | Out-Null
$repairRunId=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$repairArgs=@("$repairRoot\BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosound','-nosplash','-NoLiveCoding',
    '-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol',
    '-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',"-UserDir=$repairOut/User-$Mode-$repairRunId", "-abslog=$repairOut/$Mode.log")
if($Mode -in @('Native','Office','P2')) {
    $repairTests=switch($Mode) {
        Native {'BrainMining.Treasure.CollectedCrystalFade+BrainMining.MaterialProgression+BrainMining.Flight+BrainMining.Pickaxe.Balance+BrainMining.Pickaxe.Hardness+BrainMining.Save.GarbageCollectionLifetime'}
        Office {'BrainMining.Office.ExitFlow'}
        P2 {'BrainMining.Encounters.PIE.P2CardEscapeCheckpoints'}
    }
    $repairArgs+=@('-nullrhi',"-ExecCmds=Automation RunTests $repairTests", '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$repairOut/$Mode")
    $repairReport="$repairOut/$Mode/index.json"
} elseif($Mode -eq 'HUD') {
    $repairArgs+=@("-ExecutePythonScript=$repairRoot/Scripts/verify_material_hud.py",'-RenderOffscreen','-windowed','-ResX=1600','-ResY=900')
    $repairReport="$repairRoot/Saved/Validation/MaterialProgression/hud.json"
} else {
    $repairArgs+=@('-nullrhi',"-ExecutePythonScript=$repairRoot/Scripts/verify_merge_gameplay_map.py")
    $repairReport="$repairOut/map.json"
}
$repairStarted=[DateTime]::UtcNow
Start-MiningGuardedProcess -Executable (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -Arguments $repairArgs -ReportPath "$repairOut/resources-$Mode.csv" -MaximumSeconds 400
if(-not (Test-Path -LiteralPath $repairReport) -or (Get-Item -LiteralPath $repairReport).LastWriteTimeUtc -lt $repairStarted) {throw "No fresh report: $repairReport"}
$repairResult=Get-Content -LiteralPath $repairReport -Raw -Encoding UTF8 | ConvertFrom-Json
if($Mode -in @('Native','Office','P2')) {
    $repairMinimum=if($Mode -eq 'Native'){17}else{1}
    if($repairResult.failed -gt 0 -or $repairResult.notRun -gt 0 -or $repairResult.inProcess -gt 0 -or ($repairResult.succeeded+$repairResult.succeededWithWarnings) -lt $repairMinimum){throw "Native checks incomplete or failed: $repairReport"}
} elseif(-not $repairResult.passed){throw "Validation failed: $repairReport"}
Write-Host "Validation passed: $repairReport"
