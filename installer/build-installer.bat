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
rem    4. Copies themes, plugins, and documentation
rem    5. Invokes makensis to produce the final installer .exe
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

echo.
echo ============================================================
echo  Mcaster1Studio Installer Builder
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

echo [1/5] Found Mcaster1Studio.exe in %BIN_DIR%

rem --------------------------------------------------------------------------
rem  Step 1: Prepare staging directory (clean start)
rem --------------------------------------------------------------------------
echo [2/5] Preparing staging directory...
if exist "%STAGING_DIR%" (
    rmdir /s /q "%STAGING_DIR%"
)
mkdir "%STAGING_DIR%"

rem Copy the main executable
copy /y "%BIN_DIR%\Mcaster1Studio.exe" "%STAGING_DIR%\" >nul

rem --------------------------------------------------------------------------
rem  Step 2: Run windeployqt
rem --------------------------------------------------------------------------
echo [3/5] Running windeployqt to collect Qt dependencies...

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
rem  Step 3: Copy additional runtime DLLs and libraries
rem --------------------------------------------------------------------------
echo [4/5] Copying runtime DLLs, themes, plugins, and docs...

rem -- Mcaster1 Core library --
if exist "%BIN_DIR%\Mcaster1Core.dll" (
    copy /y "%BIN_DIR%\Mcaster1Core.dll" "%STAGING_DIR%\" >nul
)

rem -- Third-party / vcpkg DLLs --
for %%F in (
    portaudio.dll
    sqlite3.dll
    libmariadb.dll
    LIBPQ.dll
    tag.dll
    FLAC.dll
    ogg.dll
    opus.dll
    opusenc.dll
    vorbis.dll
    vorbisenc.dll
    libcrypto-3-x64.dll
    libssl-3-x64.dll
    opengl32sw.dll
    dxcompiler.dll
    dxil.dll
    libmp3lame.dll
) do (
    if exist "%BIN_DIR%\%%F" (
        copy /y "%BIN_DIR%\%%F" "%STAGING_DIR%\" >nul
    )
)

rem -- FFmpeg DLLs (versioned filenames) --
for %%F in ("%BIN_DIR%\avcodec-*.dll") do copy /y "%%F" "%STAGING_DIR%\" >nul 2>&1
for %%F in ("%BIN_DIR%\avformat-*.dll") do copy /y "%%F" "%STAGING_DIR%\" >nul 2>&1
for %%F in ("%BIN_DIR%\avutil-*.dll") do copy /y "%%F" "%STAGING_DIR%\" >nul 2>&1
for %%F in ("%BIN_DIR%\swresample-*.dll") do copy /y "%%F" "%STAGING_DIR%\" >nul 2>&1
for %%F in ("%BIN_DIR%\swscale-*.dll") do copy /y "%%F" "%STAGING_DIR%\" >nul 2>&1
for %%F in ("%BIN_DIR%\libx264-*.dll") do copy /y "%%F" "%STAGING_DIR%\" >nul 2>&1
for %%F in ("%BIN_DIR%\zlib*.dll") do copy /y "%%F" "%STAGING_DIR%\" >nul 2>&1

rem -- Themes (QSS stylesheets) --
mkdir "%STAGING_DIR%\themes" 2>nul
if exist "%PROJECT_ROOT%\UI\themes\dark.qss" (
    copy /y "%PROJECT_ROOT%\UI\themes\dark.qss"    "%STAGING_DIR%\themes\" >nul
    copy /y "%PROJECT_ROOT%\UI\themes\classic.qss"  "%STAGING_DIR%\themes\" >nul
    copy /y "%PROJECT_ROOT%\UI\themes\light.qss"    "%STAGING_DIR%\themes\" >nul
)

rem -- Plugins: modules --
mkdir "%STAGING_DIR%\plugins\modules" 2>nul
if exist "%BIN_DIR%\SampleModule.dll" (
    copy /y "%BIN_DIR%\SampleModule.dll" "%STAGING_DIR%\plugins\modules\" >nul
)
rem Copy any other module plugin DLLs placed in a plugins\modules build output
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
if exist "%PROJECT_ROOT%\README.md" (
    copy /y "%PROJECT_ROOT%\README.md" "%STAGING_DIR%\" >nul
)

rem -- VU Meter module DLL (built as standalone DLL) --
if exist "%BIN_DIR%\VUMeterModule.dll" (
    copy /y "%BIN_DIR%\VUMeterModule.dll" "%STAGING_DIR%\" >nul
)

echo        Staging complete. Contents:
dir /b "%STAGING_DIR%"

rem --------------------------------------------------------------------------
rem  Step 5: Build the NSIS installer
rem --------------------------------------------------------------------------
echo [5/5] Building NSIS installer...

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

echo.
echo ============================================================
echo  SUCCESS!  Installer created:
echo  %INSTALLER_DIR%Mcaster1Studio-0.3.0-setup.exe
echo ============================================================
echo.

endlocal
