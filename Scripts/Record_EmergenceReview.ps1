param([ValidateSet('pigeon','subway')][string]$Item='pigeon')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources -MinimumPhysicalGB 5 -MinimumCommitGB 7
$projectRoot=Split-Path -Parent $PSScriptRoot
$reviewRoot=Join-Path $projectRoot 'Saved/Review/EmergenceFix'
$session=Get-Date -Format 'yyyyMMdd-HHmmss'
New-Item -ItemType Directory -Force $reviewRoot | Out-Null
$previousMode=$env:EMERGENCE_REVIEW_ITEM
$env:EMERGENCE_REVIEW_ITEM=$Item
try {
    $reviewArgs=@("$projectRoot/BrainMining.uproject",'/Engine/Maps/Entry','-unattended','-nosplash','-NoLiveCoding','-NoSteam','-MiningQA','-NoAsyncLoadingThread','-EnablePlugins=GameFeatures,PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=AllToolsets,ModelContextProtocol',"-UserDir=$reviewRoot/User-$Item-$session","-abslog=$reviewRoot/editor-$Item-$session.log","-ExecutePythonScript=$projectRoot/Scripts/record_emergence_review.py",'-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()','-ExecCmds=t.MaxFPS 30,r.Streaming.PoolSize 1024')
    Start-MiningGuardedProcess -Executable 'C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -Arguments $reviewArgs -ReportPath "$reviewRoot/resources-$Item-$session.csv" -MaximumSeconds 540
    $take=(Get-Content -LiteralPath "$reviewRoot/latest-$Item.txt" -Raw).Trim()
    $result=Get-Content -LiteralPath (Join-Path $take 'recording.json') -Raw | ConvertFrom-Json
    if(-not $result.passed){throw $result.error}
    Write-Host "PASS: $take"
} finally { $env:EMERGENCE_REVIEW_ITEM=$previousMode }
