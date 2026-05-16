@echo off
rem ==========================================================================
rem  Mcaster1Studio Installer Build Script
rem ==========================================================================
rem
rem  Usage:
rem    build-installer.bat [Release|Debug]
rem
rem  This script:
rem    1. Locates the built Mcaster1Studio.exe
rem    2. Runs windeployqt to collect Qt runtime DLLs
rem    3. Copies vcpkg / third-party DLLs into a staging directory
rem    4. Copies themes, plugins, documentation, and signing cert
rem    5. Creates portable config directory structure
rem    6. Invokes makensis to produce the final installer .exe
rem    7. Signs the installer .exe with the Mcaster1 code signing cert
rem
rem  Prerequisites:
rem    - Qt 6.8.3 installed at C:\Qt\6.8.3\msvc2022_64
rem    - NSIS installed and makensis.exe on PATH (or at default location)
rem    - Mcaster1Studio successfully built via CMake / VS2022
rem
rem ==========================================================================

setlocal enabledelayedexpansion

rem --------------------------------------------------------------------------
rem  Configuration
rem --------------------------------------------------------------------------
set "BUILD_CONFIG=%~1"
if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Release"

set "PROJECT_ROOT=%~dp0.."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "BIN_DIR=%BUILD_DIR%\bin\%BUILD_CONFIG%"
set "STAGING_DIR=%BUILD_DIR%\installer-staging"
set "QT_DIR=C:\Qt\6.8.3\msvc2022_64"
set "INSTALLER_DIR=%~dp0"
set "SIGNING_KEYS=C:\Users\dstjohn\dev\00_mcaster1.com\SIGNING-KEYS"

echo.
echo ============================================================
echo  Mcaster1Studio Portable Installer Builder
echo  Configuration: %BUILD_CONFIG%
echo ============================================================
echo.

rem --------------------------------------------------------------------------
rem  Step 0: Verify the executable exists
rem --------------------------------------------------------------------------
if not exist "%BIN_DIR%\Mcaster1Studio.exe" (
    echo ERROR: Cannot find %BIN_DIR%\Mcaster1Studio.exe
    echo        Build the project first, then re-run this script.
    echo        Usage: build-installer.bat [Release^|Debug]
    exit /b 1
)

echo [1/7] Found Mcaster1Studio.exe in %BIN_DIR%

rem --------------------------------------------------------------------------
rem  Step 1: Prepare staging directory (clean start)
rem --------------------------------------------------------------------------
echo [2/7] Preparing staging directory...
if exist "%STAGING_DIR%" (
    rmdir /s /q "%STAGING_DIR%"
)
mkdir "%STAGING_DIR%"

rem Copy the main executable
copy /y "%BIN_DIR%\Mcaster1Studio.exe" "%STAGING_DIR%\" >nul

rem --------------------------------------------------------------------------
rem  Step 2: Run windeployqt
rem --------------------------------------------------------------------------
echo [3/7] Running windeployqt to collect Qt dependencies...

set "WINDEPLOYQT=%QT_DIR%\bin\windeployqt.exe"
if not exist "%WINDEPLOYQT%" (
    echo WARNING: windeployqt not found at %WINDEPLOYQT%
    echo          Attempting to find windeployqt on PATH...
    where windeployqt >nul 2>&1
    if errorlevel 1 (
        echo ERROR: windeployqt not found. Ensure Qt is installed.
        exit /b 1
    )
    set "WINDEPLOYQT=windeployqt"
)

"%WINDEPLOYQT%" ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --no-compiler-runtime ^
    "%STAGING_DIR%\Mcaster1Studio.exe"

if errorlevel 1 (
    echo WARNING: windeployqt returned an error. Continuing anyway...
)

rem --------------------------------------------------------------------------
rem  Step 3: Copy ALL runtime DLLs and libraries — NOTHING LEFT OUT
rem --------------------------------------------------------------------------
echo [4/7] Copying ALL runtime DLLs, themes, plugins, and docs...

rem -- Copy EVERY DLL from the build output directory (catches everything) --
echo        Copying all DLLs from build output...
copy /y "%BIN_DIR%\*.dll" "%STAGING_DIR%\" >nul 2>&1
rem Remove SDK example plugin DLLs from root (they belong in plugins/ subdirs)
if exist "%STAGING_DIR%\SampleModule.dll" del /q "%STAGING_DIR%\SampleModule.dll"
if exist "%STAGING_DIR%\SampleEffect.dll" del /q "%STAGING_DIR%\SampleEffect.dll"

rem -- libmp3lame.dll: external LAME encoder DLL (not from vcpkg) --
rem    The import lib is at external/lame/lib/libmp3lame.lib but the runtime
rem    DLL must be deployed separately.  Check multiple known locations.
if exist "%STAGING_DIR%\libmp3lame.dll" goto :found_lame
echo        libmp3lame.dll not in build output, searching known locations...
if exist "%PROJECT_ROOT%\external\lame\bin\libmp3lame.dll" (
    copy /y "%PROJECT_ROOT%\external\lame\bin\libmp3lame.dll" "%STAGING_DIR%\" >nul
    goto :found_lame
)
if exist "%PROJECT_ROOT%\external\lame\lib\libmp3lame.dll" (
    copy /y "%PROJECT_ROOT%\external\lame\lib\libmp3lame.dll" "%STAGING_DIR%\" >nul
    goto :found_lame
)
if exist "%PROJECT_ROOT%\build\bin\DEBUG\libmp3lame.dll" (
    echo        Found in Debug build output
    copy /y "%PROJECT_ROOT%\build\bin\DEBUG\libmp3lame.dll" "%STAGING_DIR%\" >nul
    goto :found_lame
)
if exist "C:\Users\dstjohn\dev\00_mcaster1.com\Mcaster1AMP\build\windows-release\Release\libmp3lame.dll" (
    echo        Found in Mcaster1AMP build
    copy /y "C:\Users\dstjohn\dev\00_mcaster1.com\Mcaster1AMP\build\windows-release\Release\libmp3lame.dll" "%STAGING_DIR%\" >nul
    goto :found_lame
)
echo        WARNING: libmp3lame.dll not found! MP3 encoding will not work.
:found_lame

rem -- MSVC C++ Runtime Redistributable (required on machines without VS installed) --
echo        Copying MSVC C++ runtime DLLs...
set "VCREDIST=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC\14.42.34433\x64\Microsoft.VC143.CRT"
if not exist "%VCREDIST%" set "VCREDIST=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.42.34433\x64\Microsoft.VC143.CRT"
if not exist "%VCREDIST%" set "VCREDIST=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC\14.40.33807\x64\Microsoft.VC143.CRT"
if not exist "%VCREDIST%" set "VCREDIST=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.40.33807\x64\Microsoft.VC143.CRT"

if exist "%VCREDIST%" (
    echo        Using: %VCREDIST%
    copy /y "%VCREDIST%\msvcp140.dll"              "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\msvcp140_1.dll"             "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\msvcp140_2.dll"             "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\msvcp140_atomic_wait.dll"   "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\msvcp140_codecvt_ids.dll"   "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\vcruntime140.dll"           "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\vcruntime140_1.dll"         "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\vcruntime140_threads.dll"   "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\concrt140.dll"              "%STAGING_DIR%\" >nul 2>&1
    copy /y "%VCREDIST%\vccorlib140.dll"            "%STAGING_DIR%\" >nul 2>&1
) else (
    echo        WARNING: MSVC redistributable not found. App may not run without VS installed.
)

rem -- Themes (QSS stylesheets) --
mkdir "%STAGING_DIR%\themes" 2>nul
if exist "%PROJECT_ROOT%\UI\themes\enterprise-pro.qss" (
    copy /y "%PROJECT_ROOT%\UI\themes\enterprise-pro.qss" "%STAGING_DIR%\themes\" >nul
    copy /y "%PROJECT_ROOT%\UI\themes\classic.qss"         "%STAGING_DIR%\themes\" >nul
)

rem -- Plugins: modules --
mkdir "%STAGING_DIR%\plugins\modules" 2>nul
if exist "%BIN_DIR%\SampleModule.dll" (
    copy /y "%BIN_DIR%\SampleModule.dll" "%STAGING_DIR%\plugins\modules\" >nul
)
if exist "%BIN_DIR%\plugins\modules\*.dll" (
    copy /y "%BIN_DIR%\plugins\modules\*.dll" "%STAGING_DIR%\plugins\modules\" >nul
)

rem -- Plugins: effects --
mkdir "%STAGING_DIR%\plugins\effects" 2>nul
if exist "%BIN_DIR%\SampleEffect.dll" (
    copy /y "%BIN_DIR%\SampleEffect.dll" "%STAGING_DIR%\plugins\effects\" >nul
)
if exist "%BIN_DIR%\plugins\effects\*.dll" (
    copy /y "%BIN_DIR%\plugins\effects\*.dll" "%STAGING_DIR%\plugins\effects\" >nul
)

rem -- Documentation --
mkdir "%STAGING_DIR%\docs" 2>nul
if exist "%PROJECT_ROOT%\docs\GettingStarted.html" (
    copy /y "%PROJECT_ROOT%\docs\GettingStarted.html" "%STAGING_DIR%\docs\" >nul
)
if exist "%PROJECT_ROOT%\docs\index.html" (
    copy /y "%PROJECT_ROOT%\docs\index.html" "%STAGING_DIR%\docs\" >nul
)
if exist "%PROJECT_ROOT%\docs\FEATURES.html" (
    copy /y "%PROJECT_ROOT%\docs\FEATURES.html" "%STAGING_DIR%\docs\" >nul
)
if exist "%PROJECT_ROOT%\README.md" (
    copy /y "%PROJECT_ROOT%\README.md" "%STAGING_DIR%\" >nul
)

rem -- VU Meter module DLL (built as standalone DLL) --
if exist "%BIN_DIR%\VUMeterModule.dll" (
    copy /y "%BIN_DIR%\VUMeterModule.dll" "%STAGING_DIR%\" >nul
)

rem --------------------------------------------------------------------------
rem  Step 4: Copy code signing certificate
rem --------------------------------------------------------------------------
echo [5/7] Copying code signing certificate...
mkdir "%STAGING_DIR%\certs" 2>nul
if exist "%SIGNING_KEYS%\Mcaster1CodeSigning.cer" (
    copy /y "%SIGNING_KEYS%\Mcaster1CodeSigning.cer" "%STAGING_DIR%\certs\" >nul
    echo        Certificate copied to staging.
) else (
    echo        WARNING: Signing certificate not found at %SIGNING_KEYS%
    echo                 Installer will be built without certificate import.
)

echo.
echo        Staging complete. Contents:
dir /b "%STAGING_DIR%"

rem --------------------------------------------------------------------------
rem  Step 5: Sign the main executable (if signtool + PFX available)
rem --------------------------------------------------------------------------
echo [6/7] Signing executable...

set "SIGNTOOL="
set "PFX_FILE=%SIGNING_KEYS%\Mcaster1CodeSigning.pfx"

rem Try to find signtool
for %%D in (
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64"
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\x64"
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64"
) do (
    if exist "%%~D\signtool.exe" (
        set "SIGNTOOL=%%~D\signtool.exe"
        goto :found_signtool
    )
)
where signtool >nul 2>&1
if not errorlevel 1 (
    for /f "delims=" %%P in ('where signtool') do set "SIGNTOOL=%%P"
)

:found_signtool
if "%SIGNTOOL%"=="" (
    echo        WARNING: signtool.exe not found. Skipping code signing.
    goto :skip_exe_sign
)
if not exist "%PFX_FILE%" (
    echo        WARNING: PFX not found at %PFX_FILE%. Skipping code signing.
    goto :skip_exe_sign
)

echo        Signing Mcaster1Studio.exe...
rem Disable delayed expansion so the ! in the password is not consumed
setlocal disabledelayedexpansion
"%SIGNTOOL%" sign /fd SHA256 /f "%PFX_FILE%" /p "Mcaster1Dev2026!" "%STAGING_DIR%\Mcaster1Studio.exe"
if errorlevel 1 (
    echo        WARNING: Signing failed. Continuing without signature.
) else (
    echo        Mcaster1Studio.exe signed successfully.
)

rem Also sign the core DLL
if exist "%STAGING_DIR%\Mcaster1Core.dll" (
    echo        Signing Mcaster1Core.dll...
    "%SIGNTOOL%" sign /fd SHA256 /f "%PFX_FILE%" /p "Mcaster1Dev2026!" "%STAGING_DIR%\Mcaster1Core.dll"
)
endlocal

:skip_exe_sign

rem --------------------------------------------------------------------------
rem  Step 6: Build the NSIS installer
rem --------------------------------------------------------------------------
echo [7/7] Building NSIS installer...

rem Try to find makensis
set "MAKENSIS="
if exist "C:\Program Files (x86)\NSIS\makensis.exe" (
    set "MAKENSIS=C:\Program Files (x86)\NSIS\makensis.exe"
) else if exist "C:\Program Files\NSIS\makensis.exe" (
    set "MAKENSIS=C:\Program Files\NSIS\makensis.exe"
) else (
    where makensis >nul 2>&1
    if not errorlevel 1 (
        for /f "delims=" %%P in ('where makensis') do set "MAKENSIS=%%P"
    )
)

if "%MAKENSIS%"=="" (
    echo ERROR: makensis.exe not found.
    echo        Install NSIS from https://nsis.sourceforge.io/
    echo        Staging directory is ready at: %STAGING_DIR%
    exit /b 1
)

echo        Using: %MAKENSIS%

"%MAKENSIS%" /V3 ^
    /DSTAGING_DIR="%STAGING_DIR%" ^
    /DBUILD_CONFIG="%BUILD_CONFIG%" ^
    "%INSTALLER_DIR%installer.nsi"

if errorlevel 1 (
    echo.
    echo ERROR: NSIS compilation failed.
    exit /b 1
)

rem --------------------------------------------------------------------------
rem  Sign the final installer .exe
rem --------------------------------------------------------------------------
set "INSTALLER_EXE=%INSTALLER_DIR%Mcaster1Studio-0.4.0-beta-setup.exe"
if not "%SIGNTOOL%"=="" (
    if exist "%PFX_FILE%" (
        echo        Signing installer executable...
        setlocal disabledelayedexpansion
        "%SIGNTOOL%" sign /fd SHA256 /f "%PFX_FILE%" /p "Mcaster1Dev2026!" "%INSTALLER_EXE%"
        if errorlevel 1 (
            echo        WARNING: Installer signing failed.
        ) else (
            echo        Installer signed successfully.
        )
        endlocal
    )
)

echo.
echo ============================================================
echo  SUCCESS!  Portable installer created:
echo  %INSTALLER_EXE%
echo.
echo  Install directory layout:
echo    config\             Settings (INI, portable)
echo    config\surfaces\    Surface YAML configs
echo    data\               SQLite databases
echo    logs\               Debug logs
echo    themes\             QSS stylesheets
echo    docs\               Documentation
echo    plugins\            Third-party plugins
echo    certs\              Code signing certificate
echo.
echo  NO files written to AppData, Registry, or Roaming.
echo ============================================================
echo.

endlocal
