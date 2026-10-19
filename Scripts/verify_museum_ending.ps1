$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$projectRoot = Split-Path -Parent $PSScriptRoot
$report = Join-Path $projectRoot 'Saved/Validation/MuseumEnding'
New-Item -ItemType Directory -Path $report -Force | Out-Null
$runArgs = @(
    "$projectRoot/BrainMining.uproject", '/Engine/Maps/Entry',
    '-unattended', '-nosplash', '-nosound', '-nullrhi', '-NoLiveCoding',
    '-ExecCmds=Automation RunTests BrainMining.Campaign.MousePaintingEndingTravel',
    '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$report", "-abslog=$report/runtime.log",
    "-LocalDataCachePath=$projectRoot/Saved/DerivedDataCache"
)
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $runArgs -ReportPath "$report/resources.csv" -MaximumSeconds 600
$result = Get-Content -LiteralPath "$report/index.json" -Raw | ConvertFrom-Json
if (($result.succeeded + $result.succeededWithWarnings) -ne 1 -or $result.failed -ne 0 -or $result.tests[0].state -ne 'Success') { throw 'Museum ending travel regression failed. See Saved/Validation/MuseumEnding.' }
Write-Host 'PASS: MainGame mouse painting opens the emergence ending.'
