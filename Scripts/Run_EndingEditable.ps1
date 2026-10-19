param([ValidateSet('Convert','Verify','Runtime')][string]$Mode = 'Verify', [switch]$Render, [switch]$AllowIndependentBuild,
    [string]$Tests = 'BrainMining.EndingEmergence.EditableScenery+BrainMining.EndingEmergence.OrganicArtRoute+BrainMining.EndingEmergence.ScaleMotion+BrainMining.EndingEmergence.TitleDrillReality')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $AllowIndependentBuild) { Assert-MiningLaunchResources }
else {
    # An unrelated checkout may compile while this editor uses already-built
    # binaries. Never share this checkout or overlap another running UE editor.
    $workloads = @(Get-MiningUnrealWorkloads)
    foreach ($workload in $workloads) {
        $processInfo = Get-CimInstance Win32_Process -Filter "ProcessId = $($workload.Id)"
        $commandLine = $processInfo.CommandLine
        if ($workload.ProcessName -ne 'Unreal .NET build' -or
            $commandLine -notmatch '(UnrealBuildTool|AutomationTool)\.dll' -or
            $commandLine -notmatch '-Project=(?:"([^"]+)"|([^\s"]+))') {
            throw 'Only an independent checkout build may overlap this run.'
        }
        $otherProject = if ($Matches[1]) { $Matches[1] } else { $Matches[2] }
        if ([IO.Path]::GetFullPath($otherProject) -eq [IO.Path]::GetFullPath("$projectRoot/BrainMining.uproject")) {
            throw 'This checkout is still building; wait before launching UE.'
        }
    }
    $headroom = Get-MiningResourceSnapshot
    if ($headroom.AvailablePhysicalGB -lt 8 -or $headroom.AvailableCommitGB -lt 10) {
        throw 'Independent-build overlap requires 8 GB physical and 10 GB commit headroom.'
    }
    Write-Host "Independent-build mode: $($headroom.AvailablePhysicalGB) GB physical / $($headroom.AvailableCommitGB) GB commit available. Runtime resource guard remains enabled."
}
$report = Join-Path $projectRoot "Saved/Validation/EndingEditable/$Mode"
New-Item -ItemType Directory -Path $report -Force | Out-Null
$runArgs = @("$projectRoot/BrainMining.uproject", '/Engine/Maps/Entry',
    '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-NoAsyncLoadingThread',
    '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol',
    '-asynctexturecompilationmaxconcurrency=1',
    "-abslog=$report/runtime.log", "-LocalDataCachePath=$projectRoot/Saved/DerivedDataCache")
if ($Mode -eq 'Runtime') {
    $runArgs = @($runArgs | Where-Object { $_ -ne '-nosound' })
    $runArgs[1] = '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
    $runArgs += @('-game', "-UserDir=$report/User", '-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()')
    $runArgs += @("-ExecCmds=Automation RunTests $Tests",
        '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$report", '-ResX=1280', '-ResY=720', '-ForceRes', '-RenderOffscreen')
    if (-not $Render) { $runArgs += '-nullrhi' }
} else {
    $script = if ($Mode -eq 'Convert') {'convert_ending_scenery.py'} else {'verify_ending_scenery_editing.py'}
    $runArgs += @('-nullrhi', "-ExecutePythonScript=$projectRoot/Scripts/$script")
}
$sourceMap = Join-Path $projectRoot 'Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap'
if ($Mode -eq 'Verify') {
    $sourceHash = (Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash
    $backupMap = Join-Path $report 'source-before-test.umap'
    Copy-Item -LiteralPath $sourceMap -Destination $backupMap -Force
}
try {
    Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -Arguments $runArgs -ReportPath "$report/resources.csv" -MaximumSeconds 900
} finally {
    if ($Mode -eq 'Verify') {
        Copy-Item -LiteralPath $backupMap -Destination $sourceMap -Force
        if ((Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash -ne $sourceHash) { throw 'Original map restoration failed; preserve source-before-test.umap.' }
    }
}
if ($Mode -eq 'Runtime') {
    $result = Get-Content -LiteralPath "$report/index.json" -Raw | ConvertFrom-Json
    if ($result.failed -ne 0 -or ($result.succeeded + $result.succeededWithWarnings) -ne $Tests.Split('+').Count) { throw 'Ending regressions failed; inspect the report.' }
} else {
    $marker = if ($Mode -eq 'Convert') {'ENDING_EDITABLE_SAVED='} else {'ENDING_EDITABLE_EDITOR_VERIFIED'}
    if (-not (Select-String -LiteralPath "$report/runtime.log" -SimpleMatch $marker -Quiet)) { throw 'Script did not finish; inspect the log.' }
    if ($Mode -eq 'Verify') {
        $verificationFile = Join-Path $projectRoot 'Saved/Validation/EndingEditable/editor-verification.json'
        $verification = Get-Content -LiteralPath $verificationFile -Raw | ConvertFrom-Json
        $verification | Add-Member -NotePropertyName source_unchanged -NotePropertyValue $true -Force
        $verification | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $verificationFile -Encoding UTF8
    }
}
