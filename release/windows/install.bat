@echo off
rem Installs JMC REVERSE (VST3, 64-bit Windows). Right-click > Run as administrator for the system-wide folder.
setlocal
set "HERE=%~dp0"
set "DEST=%CommonProgramFiles%\VST3"
echo Installing to "%DEST%" ...
mkdir "%DEST%" >nul 2>&1
rmdir /s /q "%DEST%\JMC REVERSE.vst3" >nul 2>&1
xcopy "%HERE%JMC REVERSE.vst3" "%DEST%\JMC REVERSE.vst3\" /E /I /Y /Q >nul
if errorlevel 1 (
  echo.
  echo Could not write to "%DEST%". Run this file as administrator,
  echo or copy the "JMC REVERSE.vst3" folder manually to:
  echo   %%LOCALAPPDATA%%\Programs\Common\VST3   (current user, if your DAW scans it)
  pause
  exit /b 1
)
echo Done. Restart your DAW or rescan plug-ins; look for JMC REVERSE under Fx / Reverb.
pause
