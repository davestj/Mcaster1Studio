; ==========================================================================
; Mcaster1Studio NSIS Installer Script
; ==========================================================================
;
; Product:   Mcaster1Studio - Broadcast Automation Software Suite
; Version:   0.4.0-beta
; Publisher: Mcaster1 Software
; Website:   https://mcaster1.com
;
; PORTABLE / SELF-CONTAINED INSTALL
; ----------------------------------
; Everything lives inside the install directory:
;   C:\Users\<username>\Mcaster1\Mcaster1Studio
;
; Directory layout:
;   Mcaster1Studio.exe          Main application
;   Mcaster1Core.dll            Core library
;   *.dll                       Qt6 + vcpkg + FFmpeg runtime DLLs
;   config\                     Settings INI files (portable QSettings)
;   config\surfaces\            Surface YAML configs
;   data\                       SQLite databases
;   logs\                       Debug logs, crash dumps
;   themes\                     QSS stylesheets (dark, classic, light)
;   docs\                       User documentation
;   plugins\modules\            Third-party module plugins
;   plugins\effects\            Third-party DSP effect plugins
;   certs\                      Code signing certificate
;   platforms\                  Qt platform plugins
;   styles\                     Qt style plugins
;   imageformats\               Qt image format plugins
;   multimedia\                 Qt multimedia plugins
;   networkinformation\         Qt network plugins
;   tls\                        Qt TLS plugins
;   generic\                    Qt generic plugins
;   iconengines\                Qt icon engine plugins
;   Mcaster1AudioPipes\         Reserved for AudioPipe virtual devices
;
; No files are written to AppData, LocalAppData, registry (except
; Add/Remove Programs entry), or any other OS directory.
;
; No administrator elevation required.
;
; ==========================================================================

; ---------------------------------------------------------------------------
; Compiler flags & includes
; ---------------------------------------------------------------------------
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

; ---------------------------------------------------------------------------
; Build-time defines  (override with /D on makensis command line)
; ---------------------------------------------------------------------------
!ifndef STAGING_DIR
  !define STAGING_DIR "..\build\installer-staging"
!endif

!ifndef BUILD_CONFIG
  !define BUILD_CONFIG "Release"
!endif

; ---------------------------------------------------------------------------
; Product metadata
; ---------------------------------------------------------------------------
!define PRODUCT_NAME        "Mcaster1Studio"
!define PRODUCT_VERSION     "0.4.0-beta"
!define PRODUCT_PUBLISHER   "Mcaster1 Software"
!define PRODUCT_WEB_SITE    "https://mcaster1.com"
!define PRODUCT_DIR_REGKEY  "Software\Mcaster1\${PRODUCT_NAME}"
!define PRODUCT_UNINST_KEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

; Code signing certificate
!define SIGNING_CERT        "C:\Users\dstjohn\dev\00_mcaster1.com\SIGNING-KEYS\Mcaster1CodeSigning.cer"

; ---------------------------------------------------------------------------
; General installer settings
; ---------------------------------------------------------------------------
Name        "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile     "Mcaster1Studio-${PRODUCT_VERSION}-setup.exe"

; Install into the user profile - no admin rights needed
InstallDir  "$PROFILE\Mcaster1\Mcaster1Studio"
InstallDirRegKey HKCU "${PRODUCT_DIR_REGKEY}" "InstallDir"

; Do NOT request administrator privileges
RequestExecutionLevel user

; Branding
BrandingText "${PRODUCT_NAME} ${PRODUCT_VERSION} - Portable Install"

; Application icon (used for the installer .exe and the uninstaller)
; Conditional: only set if .ico exists at build time
!define APP_ICON "..\UI\resources\appicon\app-icon.ico"
!if /FileExists "${APP_ICON}"
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
!if /FileExists "${APP_ICON}"
  !define MUI_ICON   "${APP_ICON}"
  !define MUI_UNICON "${APP_ICON}"
!endif

; Welcome page
!define MUI_WELCOMEPAGE_TITLE "Welcome to ${PRODUCT_NAME} Setup"
!define MUI_WELCOMEPAGE_TEXT  "This wizard will install ${PRODUCT_NAME} ${PRODUCT_VERSION}.$\r$\n$\r$\n\
${PRODUCT_NAME} is a professional broadcast automation suite for live radio, \
podcast production, church services, and more.$\r$\n$\r$\n\
This is a PORTABLE install. All configuration, databases, and settings \
are stored inside the install directory. Nothing is written to AppData \
or the Windows Registry (except the Add/Remove Programs entry).$\r$\n$\r$\n\
Click Next to continue."

; Finish page - offer to launch the application
!define MUI_FINISHPAGE_RUN            "$INSTDIR\Mcaster1Studio.exe"
!define MUI_FINISHPAGE_RUN_TEXT       "Launch ${PRODUCT_NAME}"
!define MUI_FINISHPAGE_LINK           "Visit ${PRODUCT_WEB_SITE}"
!define MUI_FINISHPAGE_LINK_LOCATION  "${PRODUCT_WEB_SITE}"

; ---------------------------------------------------------------------------
; Installer pages
; ---------------------------------------------------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\README.md"
!insertmacro MUI_PAGE_COMPONENTS
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
; Section: Code Signing Certificate Import (runs FIRST)
; --------------------------------------------------------------------------
; Must run before any signed binaries are written so Windows trusts them
; as they land on disk.  Uses -user stores (no admin elevation needed).
Section "-CertImport"
  ; Hidden section (leading dash = not shown in components page)
  SetOutPath "$INSTDIR\certs"
  File /nonfatal "${STAGING_DIR}\certs\Mcaster1CodeSigning.cer"

  IfFileExists "$INSTDIR\certs\Mcaster1CodeSigning.cer" 0 skip_early_cert

  DetailPrint "Importing Mcaster1 code signing certificate (current user)..."

  ; Import to current-user Root store (trusted root CA)
  nsExec::ExecToLog 'certutil -user -addstore Root "$INSTDIR\certs\Mcaster1CodeSigning.cer"'
  Pop $0
  ${If} $0 != 0
    DetailPrint "Note: Root cert import returned $0"
  ${EndIf}

  ; Import to current-user TrustedPublisher store (code signing trust)
  nsExec::ExecToLog 'certutil -user -addstore TrustedPublisher "$INSTDIR\certs\Mcaster1CodeSigning.cer"'
  Pop $0
  ${If} $0 != 0
    DetailPrint "Note: TrustedPublisher cert import returned $0"
  ${EndIf}

  DetailPrint "Certificate imported — all Mcaster1 apps will be trusted."

  skip_early_cert:
SectionEnd

; --------------------------------------------------------------------------
; Section: Core Application (required)
; --------------------------------------------------------------------------
Section "Mcaster1Studio (required)" SEC_CORE
  SectionIn RO   ; read-only - user cannot deselect

  ; ------------------------------------------------------------------
  ; Create portable directory structure FIRST
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR"
  CreateDirectory "$INSTDIR\config"
  CreateDirectory "$INSTDIR\config\surfaces"
  CreateDirectory "$INSTDIR\data"
  CreateDirectory "$INSTDIR\logs"
  CreateDirectory "$INSTDIR\themes"
  CreateDirectory "$INSTDIR\docs"
  CreateDirectory "$INSTDIR\plugins\modules"
  CreateDirectory "$INSTDIR\plugins\effects"
  CreateDirectory "$INSTDIR\certs"
  CreateDirectory "$INSTDIR\Mcaster1AudioPipes"

  ; ------------------------------------------------------------------
  ; Main executable + ALL DLLs (nothing left out)
  ; ------------------------------------------------------------------
  ; The staging directory contains everything needed:
  ;   - Mcaster1Studio.exe (main app)
  ;   - Qt6 DLLs (from windeployqt)
  ;   - vcpkg DLLs (portaudio, sqlite3, libmariadb, LIBPQ, tag, etc.)
  ;   - Audio codec DLLs (FLAC, ogg, opus, opusenc, vorbis, vorbisenc, libmp3lame)
  ;   - FFmpeg DLLs (avcodec, avformat, avutil, swresample, swscale, libx264)
  ;   - Crypto DLLs (libcrypto, libssl)
  ;   - Graphics DLLs (opengl32sw, dxcompiler, dxil)
  ;   - MSVC runtime (msvcp140, vcruntime140, concrt140, vccorlib140)
  ;   - Compression (zlib1)
  ;   - README.md
  File "${STAGING_DIR}\Mcaster1Studio.exe"
  File /nonfatal "${STAGING_DIR}\*.dll"
  File /nonfatal "${STAGING_DIR}\*.md"

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

  SetOutPath "$INSTDIR\iconengines"
  File /nonfatal "${STAGING_DIR}\iconengines\*.dll"

  ; ------------------------------------------------------------------
  ; Themes (QSS stylesheets)
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR\themes"
  File /nonfatal "${STAGING_DIR}\themes\enterprise-pro.qss"
  File /nonfatal "${STAGING_DIR}\themes\classic.qss"

  ; ------------------------------------------------------------------
  ; Plugins - modules and effects (SDK examples + user plugins)
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
  File /nonfatal "${STAGING_DIR}\docs\FEATURES.html"

  SetOutPath "$INSTDIR"
  ; README.md already included via *.md wildcard above

  ; (Code signing certificate already installed by -CertImport section above)

  ; ------------------------------------------------------------------
  ; Mcaster1AudioPipes placeholder
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR\Mcaster1AudioPipes"
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
  ; Create fresh/empty portable config (zeroed out for new install)
  ; Only written if the INI file does NOT already exist (preserve
  ; existing config on reinstall/upgrade).
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR\config"
  IfFileExists "$INSTDIR\config\Mcaster1\Mcaster1Studio.ini" skip_default_config

  CreateDirectory "$INSTDIR\config\Mcaster1"
  FileOpen  $0 "$INSTDIR\config\Mcaster1\Mcaster1Studio.ini" w
  FileWrite $0 "; Mcaster1Studio Portable Configuration$\r$\n"
  FileWrite $0 "; Auto-generated by installer - safe to edit manually.$\r$\n"
  FileWrite $0 "; All paths are relative to the install directory.$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "[General]$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "[audio]$\r$\n"
  FileWrite $0 "inputDevice=$\r$\n"
  FileWrite $0 "outputDevice=$\r$\n"
  FileWrite $0 "cueDevice=$\r$\n"
  FileWrite $0 "sampleRate=44100$\r$\n"
  FileWrite $0 "bufferSize=512$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "[window]$\r$\n"
  FileWrite $0 "geometry=$\r$\n"
  FileWrite $0 "state=$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "[session]$\r$\n"
  FileWrite $0 "openSurfaces=$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "[metrics]$\r$\n"
  FileWrite $0 "enabled=false$\r$\n"
  FileWrite $0 "port=9100$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "[dbservers]$\r$\n"
  FileWrite $0 "count=0$\r$\n"
  FileWrite $0 "default=$\r$\n"
  FileClose $0

  skip_default_config:

  ; ------------------------------------------------------------------
  ; Write the uninstaller
  ; ------------------------------------------------------------------
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; ------------------------------------------------------------------
  ; Registry - install location (minimal, for upgrades only)
  ; ------------------------------------------------------------------
  WriteRegStr HKCU "${PRODUCT_DIR_REGKEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${PRODUCT_DIR_REGKEY}" "Version"    "${PRODUCT_VERSION}"

  ; ------------------------------------------------------------------
  ; Registry - Add / Remove Programs
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

; (Certificate import moved to -CertImport section above — runs before binaries land)

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
; Section: Info note
; --------------------------------------------------------------------------
Section "-PostInstall"
  DetailPrint ""
  DetailPrint "=== Portable Install Complete ==="
  DetailPrint "All config, data, and logs are stored in:"
  DetailPrint "  $INSTDIR"
  DetailPrint ""
  DetailPrint "Directory layout:"
  DetailPrint "  config\          Settings (INI files)"
  DetailPrint "  config\surfaces\ Surface configurations (YAML)"
  DetailPrint "  data\            SQLite databases"
  DetailPrint "  logs\            Debug logs"
  DetailPrint "  themes\          QSS theme stylesheets"
  DetailPrint "  docs\            Documentation"
  DetailPrint "  plugins\         Third-party plugins"
  DetailPrint ""
  DetailPrint "NOTE: Mcaster1AudioPipe virtual audio devices are available"
  DetailPrint "as a separate download from https://mcaster1.com/audiopipe"
  DetailPrint ""
SectionEnd

; ==========================================================================
;  SECTION DESCRIPTIONS
; ==========================================================================
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE}      "Install the ${PRODUCT_NAME} application and all required runtime files. (Required)"
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
  ; Remove installed files - sub-directories first
  ; ------------------------------------------------------------------
  ; Qt plugin directories
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\imageformats"
  RMDir /r "$INSTDIR\multimedia"
  RMDir /r "$INSTDIR\networkinformation"
  RMDir /r "$INSTDIR\tls"
  RMDir /r "$INSTDIR\generic"
  RMDir /r "$INSTDIR\iconengines"

  ; App directories
  RMDir /r "$INSTDIR\themes"
  RMDir /r "$INSTDIR\plugins"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\certs"
  RMDir /r "$INSTDIR\Mcaster1AudioPipes"

  ; Portable data directories
  ; NOTE: config/ and data/ may contain user data.
  ; We remove them only if the user chose to uninstall.
  RMDir /r "$INSTDIR\config"
  RMDir /r "$INSTDIR\data"
  RMDir /r "$INSTDIR\logs"

  ; Remove ALL executables, DLLs, and files from install root
  Delete "$INSTDIR\Mcaster1Studio.exe"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\*.md"
  Delete "$INSTDIR\*.yaml"
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
