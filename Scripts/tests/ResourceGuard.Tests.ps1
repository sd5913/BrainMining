$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $projectRoot 'Scripts\ResourceGuard.ps1')
$testDirectory = Join-Path $projectRoot ('Saved\Performance\GuardTests\' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
$powershell = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
$originalProbe = ${function:Get-MiningResourceSnapshot}
$originalWorkloadLookup = ${function:Get-MiningUnrealWorkloads}
$originalProcessLookup = Get-Command Get-Process
$existingEditorIds = @(Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
$passed = 0
function Assert-GuardTest([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAIL: $Message" }
    ++$script:passed
    Write-Host "PASS: $Message"
}
function Get-MiningResourceSnapshot {
    [pscustomobject]@{ Time = [DateTime]::UtcNow.ToString('o'); AvailablePhysicalGB = 8; AvailableCommitGB = 12 }
}
try {
    function Get-MiningUnrealWorkloads { @() }
    Assert-MiningLaunchResources
    Assert-GuardTest $true 'Adequate headroom passes without starting anything'
    function Get-MiningResourceSnapshot {
        [pscustomobject]@{ Time = [DateTime]::UtcNow.ToString('o'); AvailablePhysicalGB = 3; AvailableCommitGB = 12 }
    }
    $blocked = $false
    try { Assert-MiningLaunchResources } catch { $blocked = $_.Exception.Message -like 'Insufficient resource headroom*' }
    Assert-GuardTest $blocked 'Low physical memory blocks launch'
    function Get-MiningResourceSnapshot {
        [pscustomobject]@{ Time = [DateTime]::UtcNow.ToString('o'); AvailablePhysicalGB = 8; AvailableCommitGB = 3 }
    }
    $blocked = $false
    try { Assert-MiningLaunchResources } catch { $blocked = $_.Exception.Message -like 'Insufficient resource headroom*' }
    Assert-GuardTest $blocked 'Low commit headroom blocks launch'
    function Get-MiningUnrealWorkloads { [pscustomobject]@{ ProcessName = 'UnrealEditor'; Id = 12345 } }
    $blocked = $false
    try { Assert-MiningLaunchResources } catch { $blocked = $_.Exception.Message -like 'Another Unreal workload*' }
    Assert-GuardTest $blocked 'An existing editor blocks a second workload'
    ${function:Get-MiningUnrealWorkloads} = $originalWorkloadLookup
    function Get-MiningResourceSnapshot {
        [pscustomobject]@{ Time = [DateTime]::UtcNow.ToString('o'); AvailablePhysicalGB = 8; AvailableCommitGB = 12 }
    }
    $helper = Join-Path $testDirectory 'argument helper.ps1'
    @'
param([string]$OutputPath)
$args | ConvertTo-Json -Compress | Set-Content -LiteralPath $OutputPath -Encoding UTF8
Start-Sleep -Seconds 1
'@ | Set-Content -LiteralPath $helper -Encoding UTF8
    $outputPath = Join-Path $testDirectory 'arguments.json'
    $expected = @('path with spaces', 'quoted "value"', 'C:\trailing space\', '-ExecCmds=t.MaxFPS 60,sg.ShadowQuality 2')
    Start-MiningGuardedProcess -Executable $powershell -Arguments (@('-NoProfile', '-File', $helper, $outputPath) + $expected) -ReportPath (Join-Path $testDirectory 'normal.csv')
    $actual = Get-Content -LiteralPath $outputPath -Raw | ConvertFrom-Json
    Assert-GuardTest ($actual.Count -eq $expected.Count) 'All child arguments arrive'
    for ($index = 0; $index -lt $expected.Count; ++$index) {
        Assert-GuardTest ($actual[$index] -ceq $expected[$index]) "Argument $index survives Windows quoting"
    }
    $waitHelper = Join-Path $testDirectory 'wait.ps1'
    'Start-Sleep -Seconds 20' | Set-Content -LiteralPath $waitHelper -Encoding UTF8
    $stopped = $false
    try {
        Start-MiningGuardedProcess -Executable $powershell -Arguments @('-NoProfile', '-File', $waitHelper) -ReportPath (Join-Path $testDirectory 'timeout.csv') -MaximumSeconds 1
    } catch { $stopped = $_.Exception.Message -like 'The bounded run reached*' }
    Assert-GuardTest $stopped 'Timeout terminates only the launched helper'
    function Get-MiningResourceSnapshot {
        [pscustomobject]@{ Time = [DateTime]::UtcNow.ToString('o'); AvailablePhysicalGB = .5; AvailableCommitGB = .5 }
    }
    $stopped = $false
    try {
        Start-MiningGuardedProcess -Executable $powershell -Arguments @('-NoProfile', '-File', $waitHelper) -ReportPath (Join-Path $testDirectory 'low-memory.csv')
    } catch { $stopped = $_.Exception.Message -like 'Resource headroom fell*' }
    Assert-GuardTest $stopped 'Simulated critical memory terminates the launched helper without allocating memory'
    $remainingIds = @(& $originalProcessLookup -Name UnrealEditor -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
    foreach ($editorId in $existingEditorIds) {
        Assert-GuardTest ($remainingIds -contains $editorId) "Existing editor $editorId remains running"
    }
    Write-Host "$passed checks passed. No Unreal process was launched or terminated."
} finally {
    ${function:Get-MiningResourceSnapshot} = $originalProbe
    ${function:Get-MiningUnrealWorkloads} = $originalWorkloadLookup
}
