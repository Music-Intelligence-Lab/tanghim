@echo off
REM Tanghim standalone uninstaller (Windows) — full removal, including runtime
REM data. The installer is Uninstallable=no (no Add/Remove entry), so this script
REM is the supported way to fully remove Tanghim and test from a clean state.
REM
REM Run as administrator (required to delete the system plug-in folders):
REM   right-click this file -> "Run as administrator".

setlocal

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo   This uninstaller must be run as administrator.
    echo   Right-click uninstall.bat and choose "Run as administrator".
    echo.
    pause
    exit /b 1
)

echo Removing Tanghim...

REM --- Plug-ins (system: Common Files VST3 + CLAP) ---
rmdir /S /Q "%CommonProgramFiles%\VST3\Tanghim.vst3"          2>nul
rmdir /S /Q "%CommonProgramFiles%\VST3\Tanghim Receiver.vst3" 2>nul
del   /Q    "%CommonProgramFiles%\CLAP\Tanghim.clap"          2>nul
del   /Q    "%CommonProgramFiles%\CLAP\Tanghim Receiver.clap" 2>nul

REM --- Stale m4l staging left by older installers (now staged under ProgramData) ---
rmdir /S /Q "%CommonProgramFiles%\VST3\m4l-staging" 2>nul

REM --- M4L devices (our whole device subfolder, incl. legacy combined device) ---
rmdir /S /Q "%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim" 2>nul

REM --- MTS-ESP Max Package (current Packages + stale Library, Max 8 + Max 9) ---
rmdir /S /Q "%USERPROFILE%\Documents\Max 8\Packages\MTS-ESP-Max-Package" 2>nul
rmdir /S /Q "%USERPROFILE%\Documents\Max 9\Packages\MTS-ESP-Max-Package" 2>nul
rmdir /S /Q "%USERPROFILE%\Documents\Max 8\Library\MTS-ESP-Max-Package"  2>nul
rmdir /S /Q "%USERPROFILE%\Documents\Max 9\Library\MTS-ESP-Max-Package"  2>nul

REM --- Runtime data (cache, settings.json, presets.json, receivers, midi-export)
REM     + ProgramData staging ---
rmdir /S /Q "%APPDATA%\Tanghim"     2>nul
rmdir /S /Q "%ProgramData%\Tanghim" 2>nul

REM --- DELIBERATELY NOT removed: the MTS-ESP shared library
REM     %CommonProgramFiles%\MTS-ESP\LIBMTS.dll. It is a system-wide resource
REM     installed and shared by every MTS-ESP product (Surge, ODDSound, etc.);
REM     deleting it would break tuning for other software on this machine. The
REM     installer installs it only if absent (Inno onlyifdoesntexist) and never
REM     downgrades it. Leave it in place. ---

echo.
echo Tanghim removed. (If a DAW was open during removal, rescan plug-ins.)
echo.
pause
endlocal
