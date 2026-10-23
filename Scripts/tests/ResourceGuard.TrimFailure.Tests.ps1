$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $projectRoot 'Scripts/ResourceGuard.ps1')
$report = Join-Path $projectRoot ('Saved/Validation/SizeOptimization/GuardTrimTest-' + [Guid]::NewGuid().ToString('N'))
$powershell = "$env:SystemRoot/System32/WindowsPowerShell/v1.0/powershell.exe"
function Start-Process {
    param($FilePath, $ArgumentList, $WorkingDirectory, $WindowStyle, [switch]$PassThru)
    $script:launchedHelper = Microsoft.PowerShell.Management\Start-Process @PSBoundParameters
    $script:launchedHelper
}
function Get-CimInstance {
    param($ClassName, $Filter, $OperationTimeoutSec, $ErrorAction)
    throw 'Simulated WMI lookup cancellation'
}
function Get-MiningResourceSnapshot {
    [pscustomobject]@{Time=[DateTime]::UtcNow.ToString('o');AvailablePhysicalGB=3;AvailableCommitGB=12}
}
Start-MiningGuardedProcess -Executable $powershell -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Seconds 2; exit 0') -ReportPath "$report/recovered.csv" -TrimCookWorkingSet -MaximumSeconds 15
function Get-MiningResourceSnapshot {
    [pscustomobject]@{Time=[DateTime]::UtcNow.ToString('o');AvailablePhysicalGB=.5;AvailableCommitGB=12}
}
$stopped = $false
try {
    Start-MiningGuardedProcess -Executable $powershell -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Seconds 20') -ReportPath "$report/critical.csv" -TrimCookWorkingSet -MaximumSeconds 15
} catch {
    $stopped = $_.Exception.Message -like 'Resource headroom fell*'
}
if (-not $stopped) { throw 'Critical memory must still stop the launched helper after a WMI lookup failure.' }
if (-not $script:launchedHelper.HasExited) { throw 'Guard reported termination but the helper is still alive.' }
Write-Host 'PASS: WMI trim failure is recoverable; critical-memory termination remains enforced.'
