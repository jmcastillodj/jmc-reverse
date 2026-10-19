@echo off
rem Removes JMC REVERSE. Presets and captures are kept unless you pass /purge.
setlocal
rmdir /s /q "%CommonProgramFiles%\VST3\JMC REVERSE.vst3" >nul 2>&1
rmdir /s /q "%LOCALAPPDATA%\Programs\Common\VST3\JMC REVERSE.vst3" >nul 2>&1
echo Plugin removed.
if /i "%~1"=="/purge" (
  rmdir /s /q "%APPDATA%\JMC\JMC REVERSE" >nul 2>&1
  echo Presets and captures removed.
) else (
  echo Kept: %%APPDATA%%\JMC\JMC REVERSE  (presets, captures). Use "uninstall.bat /purge" to delete them.
)
pause
