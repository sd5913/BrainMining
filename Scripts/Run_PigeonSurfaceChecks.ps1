param([string]$RunName='Runtime',[switch]$Native,[string]$Exe='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ResourceGuard.ps1')
Assert-MiningLaunchResources
$pigeonRoot=Split-Path -Parent $PSScriptRoot
$pigeonOut=Join-Path $pigeonRoot "Saved/Validation/PigeonSurface/$RunName"
New-Item -ItemType Directory -Path $pigeonOut -Force | Out-Null
$pigeonArgs=@("$pigeonRoot/BrainMining.uproject",'/Game/MainGame/Maps/Lvl_MainGame','-NoLiveCoding','-unattended','-nosplash','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol',"-UserDir=$pigeonOut/profile","-abslog=$pigeonOut/game.log",'-ini:Engine:[/Script/Engine.UserInterfaceSettings]:HardwareCursors=()')
$pigeonExecutable='C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
if($Exe){
 if($Native){throw 'Native automation requires the Editor build.'}
 $pigeonExecutable=(Resolve-Path -LiteralPath $Exe).Path
 $pigeonArgs=@('-unattended','-nosplash',"-UserDir=$pigeonOut/profile","-abslog=$pigeonOut/game.log")
}
if($Native){
 $pigeonArgs[1]='/Engine/Maps/Entry'
 $pigeonArgs+=@('-nullrhi','-nosound','-ExecCmds=mining.SaveEnabled 0,Automation RunTests BrainMining.Progression.RewardsAndResources+BrainMining.Collection','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$pigeonOut/Native")
}else{
 $pigeonArgs+=@('-game','-RenderOffscreen','-windowed','-ResX=1376','-ResY=960','-MiningQA')
 $clientArgs=@("$PSScriptRoot/verify_pigeon_surface.py",$pigeonOut,'--attach')
 $encodedClient=($clientArgs | ForEach-Object { ConvertTo-MiningProcessArgument $_ }) -join ' '
 $client=Start-Process -FilePath (Get-Command python).Source -ArgumentList $encodedClient -WorkingDirectory $pigeonRoot -WindowStyle Hidden -RedirectStandardOutput "$pigeonOut/checks.log" -RedirectStandardError "$pigeonOut/checks-errors.log" -PassThru
}
Start-MiningGuardedProcess -Executable $pigeonExecutable -Arguments $pigeonArgs -ReportPath "$pigeonOut/resources.csv" -MaximumSeconds 600
if($Native){
 $nativeResult=Get-Content "$pigeonOut/Native/index.json" -Raw | ConvertFrom-Json
 if($nativeResult.failed -gt 0 -or $nativeResult.notRun -gt 0 -or $nativeResult.succeeded -lt 1){throw 'Pigeon/energy native regression failed.'}
}else{
 if(-not $client.WaitForExit(30000)){throw 'Runtime verification client did not finish.'}
 $runtimeResult=Get-Content "$pigeonOut/results.json" -Raw | ConvertFrom-Json
 if(-not $runtimeResult.passed){throw "Runtime verification failed: $($runtimeResult.error)"}
}
