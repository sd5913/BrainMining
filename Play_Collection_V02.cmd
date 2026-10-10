@echo off
setlocal
set "UE_EDITOR=C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UE_EDITOR%" (
  echo Unreal Engine 5.8 not found. Update UE_EDITOR in this launcher.
  pause
  exit /b 1
)
start "" "%UE_EDITOR%" "%~dp0BrainMining.uproject" /Game/CollectionPrototype/Maps/Lvl_Collection_V02 -game -windowed -ResX=1600 -ResY=900 -nosplash -DisablePlugins=AllToolsets,ModelContextProtocol
