$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$projectRoot = Split-Path -Parent $PSScriptRoot
$report = Join-Path $projectRoot 'Saved/Review/HeavenDuckGush'
$session = Get-Date -Format 'yyyyMMdd-HHmmss'
New-Item -ItemType Directory -Path $report -Force | Out-Null
$runArgs = @(
    "$projectRoot/BrainMining.uproject", '/Engine/Maps/Entry',
    '-unattended', '-nosplash', '-NoLiveCoding', '-NoSteam', '-NoAsyncLoadingThread',
    '-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities',
    '-DisablePlugins=AllToolsets,ModelContextProtocol',
    "-UserDir=$report/User-$session", "-abslog=$report/editor-$session.log",
    "-ExecutePythonScript=$projectRoot/Scripts/record_heaven_duck_gush.py",
    '-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720',
    '-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()',
    '-ExecCmds=t.MaxFPS 30,r.Streaming.PoolSize 1024'
)
Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $runArgs -ReportPath "$report/resources-$session.csv" -MaximumSeconds 480
$take = Get-Content -LiteralPath "$report/latest-take.txt" -Raw
$result = Get-Content -LiteralPath (Join-Path $take 'recording.json') -Raw | ConvertFrom-Json
if (-not $result.passed) { throw $result.error }
Write-Host "PASS: $take"
