; ==========================================================================
; Mcaster1Studio NSIS Installer Script
; ==========================================================================
;
; Product:   Mcaster1Studio — Broadcast Automation Software Suite
; Version:   0.3.0
; Publisher: Mcaster1
; Website:   https://mcaster1.com
;
; This installer places Mcaster1Studio into the user's profile directory
; (no administrator elevation required).  Default install path:
;   C:\Users\<username>\Mcaster1\Mcaster1Studio
;
; Build prerequisites:
;   1. Compile Mcaster1Studio (Release or Debug) via CMake / VS2022
;   2. Run windeployqt against the built .exe
;   3. Run build-installer.bat  (or call makensis directly on this file)
;
; ==========================================================================

; ---------------------------------------------------------------------------
; Compiler flags & includes
; ---------------------------------------------------------------------------
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

; ---------------------------------------------------------------------------
; Build-time defines  (override with /D on makensis command line)
; ---------------------------------------------------------------------------
; STAGING_DIR is where build-installer.bat copies everything before NSIS runs
!ifndef STAGING_DIR
  !define STAGING_DIR "..\build\installer-staging"
!endif

; BUILD_CONFIG selects Release vs Debug artefacts when staging hasn't
; already been done.  The build-installer.bat script sets this.
!ifndef BUILD_CONFIG
  !define BUILD_CONFIG "Release"
!endif

; ---------------------------------------------------------------------------
; Product metadata
; ---------------------------------------------------------------------------
!define PRODUCT_NAME        "Mcaster1Studio"
!define PRODUCT_VERSION     "0.3.0"
!define PRODUCT_PUBLISHER   "Mcaster1"
!define PRODUCT_WEB_SITE    "https://mcaster1.com"
!define PRODUCT_DIR_REGKEY  "Software\${PRODUCT_PUBLISHER}\${PRODUCT_NAME}"
!define PRODUCT_UNINST_KEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

; ---------------------------------------------------------------------------
; General installer settings
; ---------------------------------------------------------------------------
Name        "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile     "Mcaster1Studio-${PRODUCT_VERSION}-setup.exe"

; Install into the user profile — no admin rights needed
InstallDir  "$PROFILE\Mcaster1\Mcaster1Studio"
InstallDirRegKey HKCU "${PRODUCT_DIR_REGKEY}" "InstallDir"

; Do NOT request administrator privileges
RequestExecutionLevel user

; Branding
BrandingText "${PRODUCT_NAME} ${PRODUCT_VERSION} Installer"

; Application icon (used for the installer .exe and the uninstaller)
!define APP_ICON "..\UI\resources\appicon\app-icon.ico"
!ifdef APP_ICON
  Icon "${APP_ICON}"
  UninstallIcon "${APP_ICON}"
!endif

; Compression
SetCompressor /SOLID lzma
SetCompressorDictSize 64

; ---------------------------------------------------------------------------
; Variables
; ---------------------------------------------------------------------------
Var StartMenuGroup

; ---------------------------------------------------------------------------
; Modern UI configuration
; ---------------------------------------------------------------------------
!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING

; Use the app icon for the installer header if available
!ifdef APP_ICON
  !define MUI_ICON   "${APP_ICON}"
  !define MUI_UNICON "${APP_ICON}"
!endif

; Welcome page text
!define MUI_WELCOMEPAGE_TITLE "Welcome to ${PRODUCT_NAME} Setup"
!define MUI_WELCOMEPAGE_TEXT  "This wizard will guide you through the installation of ${PRODUCT_NAME} ${PRODUCT_VERSION}.$\r$\n$\r$\n${PRODUCT_NAME} is a professional broadcast automation suite supporting live radio, podcast production, church services, and more.$\r$\n$\r$\nClick Next to continue."

; Finish page — offer to launch the application
!define MUI_FINISHPAGE_RUN            "$INSTDIR\Mcaster1Studio.exe"
!define MUI_FINISHPAGE_RUN_TEXT       "Launch ${PRODUCT_NAME}"
!define MUI_FINISHPAGE_LINK           "Visit ${PRODUCT_WEB_SITE}"
!define MUI_FINISHPAGE_LINK_LOCATION  "${PRODUCT_WEB_SITE}"

; ---------------------------------------------------------------------------
; Installer pages
; ---------------------------------------------------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\README.md"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; ---------------------------------------------------------------------------
; Uninstaller pages
; ---------------------------------------------------------------------------
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; ---------------------------------------------------------------------------
; Language
; ---------------------------------------------------------------------------
!insertmacro MUI_LANGUAGE "English"

; ==========================================================================
;  SECTIONS
; ==========================================================================

; --------------------------------------------------------------------------
; Section: Core Application
; --------------------------------------------------------------------------
Section "Mcaster1Studio (required)" SEC_CORE
  SectionIn RO   ; read-only — user cannot deselect

  SetOutPath "$INSTDIR"

  ; ------------------------------------------------------------------
  ; Main executable
  ; ------------------------------------------------------------------
  File "${STAGING_DIR}\Mcaster1Studio.exe"

  ; ------------------------------------------------------------------
  ; Qt6 DLLs (deployed by windeployqt into staging)
  ; ------------------------------------------------------------------
  File /nonfatal "${STAGING_DIR}\Qt6*.dll"

  ; ------------------------------------------------------------------
  ; Mcaster1 Core library
  ; ------------------------------------------------------------------
  File /nonfatal "${STAGING_DIR}\Mcaster1Core.dll"

  ; ------------------------------------------------------------------
  ; vcpkg / third-party runtime DLLs
  ; ------------------------------------------------------------------
  File /nonfatal "${STAGING_DIR}\portaudio.dll"
  File /nonfatal "${STAGING_DIR}\sqlite3.dll"
  File /nonfatal "${STAGING_DIR}\libmariadb.dll"
  File /nonfatal "${STAGING_DIR}\LIBPQ.dll"
  File /nonfatal "${STAGING_DIR}\tag.dll"
  File /nonfatal "${STAGING_DIR}\FLAC.dll"
  File /nonfatal "${STAGING_DIR}\ogg.dll"
  File /nonfatal "${STAGING_DIR}\opus.dll"
  File /nonfatal "${STAGING_DIR}\opusenc.dll"
  File /nonfatal "${STAGING_DIR}\vorbis.dll"
  File /nonfatal "${STAGING_DIR}\vorbisenc.dll"
  File /nonfatal "${STAGING_DIR}\libcrypto-3-x64.dll"
  File /nonfatal "${STAGING_DIR}\libssl-3-x64.dll"
  File /nonfatal "${STAGING_DIR}\opengl32sw.dll"
  File /nonfatal "${STAGING_DIR}\dxcompiler.dll"
  File /nonfatal "${STAGING_DIR}\dxil.dll"
  File /nonfatal "${STAGING_DIR}\zlib*.dll"

  ; FFmpeg libraries
  File /nonfatal "${STAGING_DIR}\avcodec-*.dll"
  File /nonfatal "${STAGING_DIR}\avformat-*.dll"
  File /nonfatal "${STAGING_DIR}\avutil-*.dll"
  File /nonfatal "${STAGING_DIR}\swresample-*.dll"
  File /nonfatal "${STAGING_DIR}\swscale-*.dll"
  File /nonfatal "${STAGING_DIR}\libx264-*.dll"

  ; LAME MP3 encoder (external, may not be present)
  File /nonfatal "${STAGING_DIR}\libmp3lame.dll"

  ; ------------------------------------------------------------------
  ; Qt platform plugins (deployed by windeployqt)
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR\platforms"
  File /nonfatal "${STAGING_DIR}\platforms\*.dll"

  SetOutPath "$INSTDIR\styles"
  File /nonfatal "${STAGING_DIR}\styles\*.dll"

  SetOutPath "$INSTDIR\imageformats"
  File /nonfatal "${STAGING_DIR}\imageformats\*.dll"

  SetOutPath "$INSTDIR\multimedia"
  File /nonfatal "${STAGING_DIR}\multimedia\*.dll"

  SetOutPath "$INSTDIR\networkinformation"
  File /nonfatal "${STAGING_DIR}\networkinformation\*.dll"

  SetOutPath "$INSTDIR\tls"
  File /nonfatal "${STAGING_DIR}\tls\*.dll"

  SetOutPath "$INSTDIR\generic"
  File /nonfatal "${STAGING_DIR}\generic\*.dll"

  ; ------------------------------------------------------------------
  ; Themes (QSS stylesheets)
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR\themes"
  File /nonfatal "${STAGING_DIR}\themes\dark.qss"
  File /nonfatal "${STAGING_DIR}\themes\classic.qss"
  File /nonfatal "${STAGING_DIR}\themes\light.qss"

  ; ------------------------------------------------------------------
  ; Plugins — modules and effects
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR\plugins\modules"
  File /nonfatal "${STAGING_DIR}\plugins\modules\*.dll"

  SetOutPath "$INSTDIR\plugins\effects"
  File /nonfatal "${STAGING_DIR}\plugins\effects\*.dll"

  ; ------------------------------------------------------------------
  ; Documentation
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR\docs"
  File /nonfatal "${STAGING_DIR}\docs\GettingStarted.html"
  File /nonfatal "${STAGING_DIR}\docs\index.html"

  SetOutPath "$INSTDIR"
  File /nonfatal "${STAGING_DIR}\README.md"

  ; ------------------------------------------------------------------
  ; Mcaster1AudioPipes placeholder
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR\Mcaster1AudioPipes"
  ; Create a readme explaining AudioPipe availability
  FileOpen  $0 "$INSTDIR\Mcaster1AudioPipes\README.txt" w
  FileWrite $0 "Mcaster1AudioPipes$\r$\n"
  FileWrite $0 "==================$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "This directory is reserved for Mcaster1AudioPipe virtual audio$\r$\n"
  FileWrite $0 "devices.  AudioPipe lets you route audio between Mcaster1Studio$\r$\n"
  FileWrite $0 "surfaces and other applications on your system.$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "AudioPipe is available as a separate download from:$\r$\n"
  FileWrite $0 "  https://mcaster1.com/audiopipe$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "After installing AudioPipe, its driver files will appear here.$\r$\n"
  FileClose $0

  ; ------------------------------------------------------------------
  ; Write the uninstaller
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; ------------------------------------------------------------------
  ; Registry — install location
  ; ------------------------------------------------------------------
  WriteRegStr HKCU "${PRODUCT_DIR_REGKEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${PRODUCT_DIR_REGKEY}" "Version"    "${PRODUCT_VERSION}"

  ; ------------------------------------------------------------------
  ; Registry — Add / Remove Programs
  ; ------------------------------------------------------------------
  WriteRegStr   HKCU "${PRODUCT_UNINST_KEY}" "DisplayName"     "${PRODUCT_NAME} ${PRODUCT_VERSION}"
  WriteRegStr   HKCU "${PRODUCT_UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr   HKCU "${PRODUCT_UNINST_KEY}" "DisplayIcon"     "$INSTDIR\Mcaster1Studio.exe,0"
  WriteRegStr   HKCU "${PRODUCT_UNINST_KEY}" "Publisher"       "${PRODUCT_PUBLISHER}"
  WriteRegStr   HKCU "${PRODUCT_UNINST_KEY}" "URLInfoAbout"    "${PRODUCT_WEB_SITE}"
  WriteRegStr   HKCU "${PRODUCT_UNINST_KEY}" "DisplayVersion"  "${PRODUCT_VERSION}"
  WriteRegDWORD HKCU "${PRODUCT_UNINST_KEY}" "NoModify"        1
  WriteRegDWORD HKCU "${PRODUCT_UNINST_KEY}" "NoRepair"        1

  ; Compute and write estimated install size (KB)
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${PRODUCT_UNINST_KEY}" "EstimatedSize" $0

SectionEnd

; --------------------------------------------------------------------------
; Section: Shortcuts
; --------------------------------------------------------------------------
Section "Shortcuts" SEC_SHORTCUTS

  ; Desktop shortcut
  CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" \
    "$INSTDIR\Mcaster1Studio.exe" "" "$INSTDIR\Mcaster1Studio.exe" 0

  ; Start Menu folder
  StrCpy $StartMenuGroup "Mcaster1"
  CreateDirectory "$SMPROGRAMS\$StartMenuGroup"

  ; Start Menu: main application
  CreateShortCut "$SMPROGRAMS\$StartMenuGroup\${PRODUCT_NAME}.lnk" \
    "$INSTDIR\Mcaster1Studio.exe" "" "$INSTDIR\Mcaster1Studio.exe" 0

  ; Start Menu: Getting Started documentation
  CreateShortCut "$SMPROGRAMS\$StartMenuGroup\Getting Started.lnk" \
    "$INSTDIR\docs\GettingStarted.html" "" "" 0

  ; Start Menu: Uninstall
  CreateShortCut "$SMPROGRAMS\$StartMenuGroup\Uninstall ${PRODUCT_NAME}.lnk" \
    "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0

SectionEnd

; --------------------------------------------------------------------------
; Section: AudioPipe notice (informational only)
; --------------------------------------------------------------------------
Section "-AudioPipeNotice"
  ; Show a brief note about AudioPipe availability on the details log
  DetailPrint ""
  DetailPrint "NOTE: Mcaster1AudioPipe virtual audio devices are available"
  DetailPrint "as a separate download from https://mcaster1.com/audiopipe"
  DetailPrint ""
SectionEnd

; ==========================================================================
;  SECTION DESCRIPTIONS
; ==========================================================================
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE}      "Install the ${PRODUCT_NAME} application and all required runtime files."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SHORTCUTS}  "Create desktop and Start Menu shortcuts."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ==========================================================================
;  UNINSTALLER
; ==========================================================================
Section "Uninstall"

  ; ------------------------------------------------------------------
  ; Remove shortcuts
  ; ------------------------------------------------------------------
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

  StrCpy $StartMenuGroup "Mcaster1"
  Delete "$SMPROGRAMS\$StartMenuGroup\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\$StartMenuGroup\Getting Started.lnk"
  Delete "$SMPROGRAMS\$StartMenuGroup\Uninstall ${PRODUCT_NAME}.lnk"
  RMDir  "$SMPROGRAMS\$StartMenuGroup"   ; remove folder if empty

  ; ------------------------------------------------------------------
  ; Remove installed files — sub-directories first
  ; ------------------------------------------------------------------
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\imageformats"
  RMDir /r "$INSTDIR\multimedia"
  RMDir /r "$INSTDIR\networkinformation"
  RMDir /r "$INSTDIR\tls"
  RMDir /r "$INSTDIR\generic"
  RMDir /r "$INSTDIR\themes"
  RMDir /r "$INSTDIR\plugins"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\Mcaster1AudioPipes"

  ; Remove all DLLs and executables from install root
  Delete "$INSTDIR\Mcaster1Studio.exe"
  Delete "$INSTDIR\Mcaster1Core.dll"
  Delete "$INSTDIR\Qt6*.dll"
  Delete "$INSTDIR\portaudio.dll"
  Delete "$INSTDIR\sqlite3.dll"
  Delete "$INSTDIR\libmariadb.dll"
  Delete "$INSTDIR\LIBPQ.dll"
  Delete "$INSTDIR\tag.dll"
  Delete "$INSTDIR\FLAC.dll"
  Delete "$INSTDIR\ogg.dll"
  Delete "$INSTDIR\opus.dll"
  Delete "$INSTDIR\opusenc.dll"
  Delete "$INSTDIR\vorbis.dll"
  Delete "$INSTDIR\vorbisenc.dll"
  Delete "$INSTDIR\libcrypto-3-x64.dll"
  Delete "$INSTDIR\libssl-3-x64.dll"
  Delete "$INSTDIR\opengl32sw.dll"
  Delete "$INSTDIR\dxcompiler.dll"
  Delete "$INSTDIR\dxil.dll"
  Delete "$INSTDIR\zlib*.dll"
  Delete "$INSTDIR\avcodec-*.dll"
  Delete "$INSTDIR\avformat-*.dll"
  Delete "$INSTDIR\avutil-*.dll"
  Delete "$INSTDIR\swresample-*.dll"
  Delete "$INSTDIR\swscale-*.dll"
  Delete "$INSTDIR\libx264-*.dll"
  Delete "$INSTDIR\libmp3lame.dll"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\Uninstall.exe"

  ; Remove install directory (only if empty after above deletions)
  RMDir "$INSTDIR"

  ; Remove parent Mcaster1 folder if empty
  RMDir "$INSTDIR\.."

  ; ------------------------------------------------------------------
  ; Remove registry entries
  ; ------------------------------------------------------------------
  DeleteRegKey HKCU "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKCU "${PRODUCT_DIR_REGKEY}"

SectionEnd
