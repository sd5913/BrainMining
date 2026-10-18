@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\Play_MainGameCollectionsPreview.ps1"
if errorlevel 1 pause
