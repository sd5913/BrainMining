Set-StrictMode -Version Latest

if (-not ('BrainMiningResourceProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class BrainMiningResourceProbe
{
    [StructLayout(LayoutKind.Sequential)]
    public struct MemoryStatus
    {
        public uint Length, Load;
        public ulong TotalPhysical, AvailablePhysical, TotalPageFile, AvailablePageFile;
        public ulong TotalVirtual, AvailableVirtual, AvailableExtendedVirtual;
    }
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GlobalMemoryStatusEx(ref MemoryStatus status);
    [DllImport("psapi.dll", SetLastError = true)]
    public static extern bool EmptyWorkingSet(IntPtr process);
    public static MemoryStatus Read()
    {
        MemoryStatus status = new MemoryStatus();
        status.Length = (uint)Marshal.SizeOf(typeof(MemoryStatus));
        if (!GlobalMemoryStatusEx(ref status)) throw new System.ComponentModel.Win32Exception();
        return status;
    }
}
'@
}

function Get-MiningResourceSnapshot {
    $status = [BrainMiningResourceProbe]::Read()
    [pscustomobject]@{
        Time = [DateTime]::UtcNow.ToString('o')
        AvailablePhysicalGB = $status.AvailablePhysical / 1GB
        AvailableCommitGB = $status.AvailablePageFile / 1GB
    }
}

function Get-MiningUnrealWorkloads {
    $existing = @(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,ShaderCompileWorker,UnrealBuildTool,AutomationTool,BrainMining,BrainMining-Win64-Shipping,BrainMining-Win64-Development -ErrorAction SilentlyContinue)
    $existing
    Get-CimInstance Win32_Process -Filter "Name = 'dotnet.exe'" | Where-Object {
        $_.CommandLine -match '(UnrealBuildTool|AutomationTool)\.dll'
    } | ForEach-Object { [pscustomobject]@{ ProcessName = 'Unreal .NET build'; Id = $_.ProcessId } }
}

function Assert-MiningLaunchResources {
    param([double]$MinimumPhysicalGB = 4, [double]$MinimumCommitGB = 6)
    $existing = @(Get-MiningUnrealWorkloads)
    if ($existing.Count) {
        $names = ($existing | ForEach-Object { '{0} (PID {1})' -f $_.ProcessName, $_.Id }) -join ', '
        throw "Another Unreal workload is already running: $names. Close it manually before starting this task."
    }
    $snapshot = Get-MiningResourceSnapshot
    if ($snapshot.AvailablePhysicalGB -lt $MinimumPhysicalGB -or $snapshot.AvailableCommitGB -lt $MinimumCommitGB) {
        throw ('Insufficient resource headroom: {0:N1} GB physical / {1:N1} GB commit available; need {2:N1} / {3:N1} GB. Nothing was launched.' -f $snapshot.AvailablePhysicalGB, $snapshot.AvailableCommitGB, $MinimumPhysicalGB, $MinimumCommitGB)
    }
    Write-Host ('Available: {0:N1} GB physical, {1:N1} GB commit.' -f $snapshot.AvailablePhysicalGB, $snapshot.AvailableCommitGB)
}

function ConvertTo-MiningProcessArgument {
    param([AllowEmptyString()][string]$Argument)
    if ($Argument.Length -gt 0 -and $Argument -notmatch '[\s"]') { return $Argument }
    $escaped = [regex]::Replace($Argument, '(\\*)"', '$1$1\"')
    '"' + [regex]::Replace($escaped, '(\\+)$', '$1$1') + '"'
}

function Start-MiningGuardedProcess {
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$ReportPath,
        [switch]$EditorSession,
        [switch]$BelowNormal,
        [switch]$TrimCookWorkingSet,
        [int]$MaximumSeconds = 0
    )
    if (-not $EditorSession) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $ReportPath) -Force | Out-Null
    }
    $encoded = ($Arguments | ForEach-Object { ConvertTo-MiningProcessArgument $_ }) -join ' '
    $process = Start-Process -FilePath $Executable -ArgumentList $encoded -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
    $null = $process.Handle
    if ($EditorSession) {
        Write-Host "Editor started (PID $($process.Id)). Runtime termination guard is disabled for editable sessions to preserve unsaved work."
        return
    }
    $started = [DateTime]::UtcNow
    $lowSamples = 0
    $stopReason = ''
    $lastTrim = [DateTime]::MinValue
    Write-Host "Resource report: $ReportPath"
    try {
        if ($BelowNormal -and -not $process.HasExited) { $process.PriorityClass = 'BelowNormal' }
        while (-not $process.HasExited) {
            $snapshot = Get-MiningResourceSnapshot
            $process.Refresh()
            if ($process.HasExited) { break }
            if ($TrimCookWorkingSet -and $snapshot.AvailablePhysicalGB -lt 3.5 -and $snapshot.AvailableCommitGB -gt 8 -and ([DateTime]::UtcNow - $lastTrim).TotalSeconds -ge 10) {
                $cookChildren = @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $($process.Id) AND Name = 'UnrealEditor-Cmd.exe'" | Where-Object { $_.CommandLine -match '(?i)-run=Cook\b' })
                foreach ($child in $cookChildren) {
                    $cookProcess = Get-Process -Id $child.ProcessId -ErrorAction SilentlyContinue
                    if ($cookProcess -and $cookProcess.WorkingSet64 -gt 1GB) {
                        try {
                            if ([BrainMiningResourceProbe]::EmptyWorkingSet($cookProcess.Handle)) {
                                Write-Host "Released pageable cook working-set pages (PID $($cookProcess.Id)); resource limits remain active."
                            }
                        } catch { Write-Warning "Could not trim the launched cook process: $_" }
                    }
                }
                $lastTrim = [DateTime]::UtcNow
                $snapshot = Get-MiningResourceSnapshot
            }
            [pscustomobject]@{
                Time = $snapshot.Time
                ElapsedSeconds = ([DateTime]::UtcNow - $started).TotalSeconds
                AvailablePhysicalGB = $snapshot.AvailablePhysicalGB
                AvailableCommitGB = $snapshot.AvailableCommitGB
                ProcessWorkingSetGB = $process.WorkingSet64 / 1GB
                ProcessPrivateGB = $process.PrivateMemorySize64 / 1GB
            } | Export-Csv -LiteralPath $ReportPath -Append -NoTypeInformation -Encoding UTF8
            if ($snapshot.AvailablePhysicalGB -lt 1.5 -or $snapshot.AvailableCommitGB -lt 2) { ++$lowSamples } else { $lowSamples = 0 }
            if ($snapshot.AvailablePhysicalGB -lt .75 -or $snapshot.AvailableCommitGB -lt 1 -or $lowSamples -ge 2) {
                $stopReason = 'Resource headroom fell below the safety margin.'
                break
            }
            if ($MaximumSeconds -gt 0 -and ([DateTime]::UtcNow - $started).TotalSeconds -ge $MaximumSeconds) {
                $stopReason = 'The bounded run reached its time limit.'
                break
            }
            Start-Sleep -Seconds 2
        }
    }
    finally {
        if (-not $process.HasExited) {
            Write-Warning "Stopping only the process tree started by this script (PID $($process.Id)). $stopReason"
            & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F | Out-Null
            if (-not $process.WaitForExit(10000)) { Write-Warning 'The launched process did not exit within ten seconds.' }
        }
    }
    if ($stopReason) { throw "$stopReason Report: $ReportPath" }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "Process exited with code $($process.ExitCode). Report: $ReportPath" }
}
