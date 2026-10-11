@echo off
setlocal
if not defined UE_EDITOR set "UE_EDITOR=C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UE_EDITOR%" (
  echo Set UE_EDITOR to your Unreal Engine 5.8 UnrealEditor.exe path.
  pause
  exit /b 1
)
start "" "%UE_EDITOR%" "%~dp0BrainMining.uproject" /Game/CellUniverse/Maps/Lvl_CellUniverse_Demo -game -windowed -ResX=1600 -ResY=900 -nosplash -DisablePlugins=AllToolsets,ModelContextProtocol
