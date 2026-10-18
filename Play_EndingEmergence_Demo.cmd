@echo off
setlocal
if not defined UE_EDITOR set "UE_EDITOR=E:\UE5\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UE_EDITOR%" (
  echo Set UE_EDITOR to your Unreal Engine 5.8 UnrealEditor.exe path.
  pause
  exit /b 1
)
start "" "%UE_EDITOR%" "%~dp0BrainMining.uproject" /Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo -game -windowed -ResX=1600 -ResY=900 -nosplash -ForceTextureEncodeSpeed=FinalIfAvailable -asynctexturecompilationmaxconcurrency=1 -DisablePlugins=AllToolsets,ModelContextProtocol
