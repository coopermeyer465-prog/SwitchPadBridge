@echo off
title SwitchPad Windows Relay
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0SwitchPadRelay.ps1" %*
if errorlevel 1 pause
