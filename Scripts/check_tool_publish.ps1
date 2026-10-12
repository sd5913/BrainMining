# Validate committed tool dependencies before Push. Runs in Windows PowerShell.
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
Push-Location -LiteralPath $taskRoot
try {
    $taskTracked = @(& git ls-tree -r --name-only HEAD)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the committed tree.' }
    $taskRequired = @(
        'Content/Drill/Stylized/SM_DrillStylized.uasset',
        'Content/Drill/Stylized/M_DrillStylized.uasset',
        'Content/Drill/CF_DrillSpin.uasset',
        'Content/Bomb/SM_StickyBomb.uasset',
        'Content/UI/MiningGUI/WBP_MiningHUD.uasset',
        'Source/BrainMining/MiningBombPresentationComponent.cpp',
        'Source/BrainMining/MiningBombPresentationComponent.h'
    )
    foreach ($taskChannel in @('BaseColor','Roughness','Metallic','Normal')) {
        $taskRequired += "Content/Drill/Stylized/T_DrillStylized_$taskChannel.uasset"
    }
    foreach ($taskTool in @('Pickaxe','Drill','Bomb')) {
        $taskRequired += "Content/UI/MiningGUI/Reticles/M_Reticle$taskTool.uasset"
        $taskRequired += "Content/UI/MiningGUI/Reticles/T_Reticle$taskTool.uasset"
    }
    $taskErrors = @()
    foreach ($taskPath in $taskRequired) {
        if ($taskTracked -notcontains $taskPath) { $taskErrors += "Not committed: $taskPath" }
    }
    $taskMarkers = @{
        'Source/BrainMining/BrainMiningCharacter.cpp' = @('TEXT("BombMotion")','SM_DrillStylized','SM_StickyBomb')
        'Source/BrainMining/MiningBombActor.cpp' = @('MiningBombPresentationComponent','bStuck = true')
        'Source/BrainMining/MiningGUIWidgets.cpp' = @('ReticlePickaxe','ReticleDrill','ReticleBomb')
    }
    foreach ($taskPath in $taskMarkers.Keys) {
        $taskText = (& git show "HEAD:$taskPath") -join "`n"
        foreach ($taskMarker in $taskMarkers[$taskPath]) {
            if (!$taskText.Contains($taskMarker)) { $taskErrors += "Missing committed code: $taskPath / $taskMarker" }
        }
    }
    if ($taskErrors.Count) { throw ($taskErrors -join "`n") }
    Write-Output 'Tool publish check passed: committed assets and runtime code are complete.'
} finally { Pop-Location }
