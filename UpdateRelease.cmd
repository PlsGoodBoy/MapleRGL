@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_release.ps1"
if errorlevel 1 (
  echo Release update failed. See the error above.
) else (
  echo Release is ready. Zip the release folder to package it.
)
pause
