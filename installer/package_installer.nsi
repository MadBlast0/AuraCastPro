; ==============================================================================
; package_installer.nsi — NSIS installer for AuraCastPro
;
; Creates a signed Windows setup.exe that:
;   1. Installs the application binaries
;   2. Installs the virtual camera driver (MirrorCam.dll)
;   3. Installs the virtual audio driver (AudioVirtualCable.sys)
;   4. Registers COM components
;   5. Adds firewall rules
;   6. Installs Bonjour service (if not present)
;   7. Creates desktop shortcut and Start Menu entry
;   8. Registers uninstaller
;
; Requirements:
;   NSIS 3.x with the following plugins:
;     - NSISdl (download during install — for Bonjour)
;     - nsProcess
;
; Build command:
;   makensis /DVERSION=0.1.0 installer\package_installer.nsi
; ==============================================================================

!define APPNAME     "AuraCastPro"
!define APPVERSION  "${VERSION}"   ; Passed via /D on command line
!define PUBLISHER   "AuraCastPro"
!define WEBSITE     "https://auracastpro.com"
!define INSTALL_DIR "$PROGRAMFILES64\AuraCastPro"
!define UNINSTALL_REG "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro"

; Installer settings
Name                "${APPNAME} ${APPVERSION}"
OutFile             "..\build\release_x64\AuraCastPro-${APPVERSION}-Setup.exe"
InstallDir          "${INSTALL_DIR}"
InstallDirRegKey    HKLM "Software\AuraCastPro" "InstallDir"
RequestExecutionLevel admin    ; Required for driver installation
SetCompressor       /SOLID lzma
ShowInstDetails     show
ShowUnInstDetails   show

; Modern UI
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\textures\icons\png\app_icon.ico"
!define MUI_UNICON "..\assets\textures\icons\png\app_icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\assets\textures\8K_Splashes\installer_banner.bmp"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ==============================================================================
; VERSION INFORMATION (embedded in setup.exe properties)
; ==============================================================================
VIProductVersion    "${APPVERSION}.0"
VIAddVersionKey     "ProductName"     "${APPNAME}"
VIAddVersionKey     "ProductVersion"  "${APPVERSION}"
VIAddVersionKey     "CompanyName"     "${PUBLISHER}"
VIAddVersionKey     "LegalCopyright"  "© 2025 ${PUBLISHER}"
VIAddVersionKey     "FileDescription" "${APPNAME} Installer"
VIAddVersionKey     "FileVersion"     "${APPVERSION}"

; ==============================================================================
; INSTALLER SECTIONS
; ==============================================================================

Section "AuraCastPro Application" SecMain
    SectionIn RO  ; Required — cannot be deselected

    SetOutPath "$INSTDIR"

    ; Main executable and DLLs
    File "..\build\release_x64\AuraCastPro.exe"
    File "..\build\release_x64\MirrorCam.dll"

    ; Virtual camera driver and INF
    File "..\drivers\MirrorCam_Installer.inf"
    File "..\drivers\MirrorCam_Driver.cat"

    ; Runtime scripts
    File "..\scripts\setup.bat"
    File "..\scripts\register_dlls.bat"
    File "..\scripts\add_firewall_rules.ps1"

    ; Legal documents
    File "..\installer\EULA.txt"
    File "..\THIRD_PARTY_LICENSES.txt"

    ; Compiled shader objects
    SetOutPath "$INSTDIR\Shaders"
    File "..\build\release_x64\Shaders\nv12_to_rgb.cso"
    File "..\build\release_x64\Shaders\hdr10_tonemap.cso"
    File "..\build\release_x64\Shaders\lanczos8.cso"
    File "..\build\release_x64\Shaders\temporal_frame_pacing.cso"
    File "..\build\release_x64\Shaders\chroma_upsample.cso"
    File "..\build\release_x64\Shaders\fullscreen_vs.cso"

    ; Qt runtime DLLs
    SetOutPath "$INSTDIR"
    File "..\build\release_x64\Qt6Core.dll"
    File "..\build\release_x64\Qt6Gui.dll"
    File "..\build\release_x64\Qt6Quick.dll"
    File "..\build\release_x64\Qt6Charts.dll"
    File "..\build\release_x64\Qt6Network.dll"
    File "..\build\release_x64\Qt6Qml.dll"
    File "..\build\release_x64\Qt6QmlModels.dll"
    File "..\build\release_x64\Qt6QmlWorkerScript.dll"
    File "..\build\release_x64\Qt6QuickControls2.dll"
    File "..\build\release_x64\Qt6QuickControls2Impl.dll"
    File "..\build\release_x64\Qt6QuickTemplates2.dll"
    File "..\build\release_x64\Qt6ChartsQml.dll"

    ; OpenSSL
    File "..\build\release_x64\libssl-3-x64.dll"
    File "..\build\release_x64\libcrypto-3-x64.dll"

    ; Create logs directory
    CreateDirectory "$INSTDIR\logs"

    ; Task 216: Bundle ADB (Android Debug Bridge) for Android mirroring
    SetOutPath "$INSTDIR\adb"
    File "installer\adb_bundle\adb.exe"
    File "installer\adb_bundle\AdbWinApi.dll"
    File "installer\adb_bundle\AdbWinUsbApi.dll"
    DetailPrint "ADB bundled for Android mirroring support."

    ; Register COM components and virtual camera
    nsExec::ExecToLog '"$INSTDIR\scripts\register_dlls.bat" "$INSTDIR"'

    ; Add firewall rules
    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\scripts\add_firewall_rules.ps1"'

    ; Create desktop shortcut
    CreateShortcut "$DESKTOP\AuraCastPro.lnk" "$INSTDIR\AuraCastPro.exe"

    ; Start Menu entry
    CreateDirectory "$SMPROGRAMS\AuraCastPro"
    CreateShortcut "$SMPROGRAMS\AuraCastPro\AuraCastPro.lnk" "$INSTDIR\AuraCastPro.exe"
    CreateShortcut "$SMPROGRAMS\AuraCastPro\Uninstall AuraCastPro.lnk" "$INSTDIR\Uninstall.exe"

    ; Registry entries
    WriteRegStr   HKLM "Software\AuraCastPro" "InstallDir" "$INSTDIR"
    WriteRegStr   HKLM "Software\AuraCastPro" "Version"    "${APPVERSION}"

    ; Uninstaller registration
    WriteRegStr   HKLM "${UNINSTALL_REG}" "DisplayName"          "${APPNAME}"
    WriteRegStr   HKLM "${UNINSTALL_REG}" "DisplayVersion"       "${APPVERSION}"
    WriteRegStr   HKLM "${UNINSTALL_REG}" "Publisher"            "${PUBLISHER}"
    WriteRegStr   HKLM "${UNINSTALL_REG}" "URLInfoAbout"         "${WEBSITE}"
    WriteRegStr   HKLM "${UNINSTALL_REG}" "UninstallString"      "$INSTDIR\Uninstall.exe"
    WriteRegStr   HKLM "${UNINSTALL_REG}" "DisplayIcon"          "$INSTDIR\AuraCastPro.exe"
    WriteRegDWORD HKLM "${UNINSTALL_REG}" "NoModify"             1
    WriteRegDWORD HKLM "${UNINSTALL_REG}" "NoRepair"             1

    WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Virtual Audio Driver" SecAudio
    SectionIn 1  ; Optional — selected by default

    SetOutPath "$INSTDIR\drivers"
    File "..\drivers\AudioVirtualCable.sys"
    File "..\drivers\Audio_Bridge.cat"
    File "..\drivers\Audio_Installer.inf"

    ; Install kernel driver via pnputil
    nsExec::ExecToLog 'pnputil /add-driver "$INSTDIR\drivers\Audio_Installer.inf" /install'

    DetailPrint "Virtual audio driver installed."
SectionEnd

; ==============================================================================
; UNINSTALLER
; ==============================================================================

Section "Uninstall"
    Delete "$INSTDIR\setup.bat"
    Delete "$INSTDIR\register_dlls.bat"
    Delete "$INSTDIR\add_firewall_rules.ps1"
    Delete "$INSTDIR\EULA.txt"
    Delete "$INSTDIR\THIRD_PARTY_LICENSES.txt"
    Delete "$INSTDIR\MirrorCam_Installer.inf"
    Delete "$INSTDIR\MirrorCam_Driver.cat"

    ; Remove firewall rules
    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\scripts\add_firewall_rules.ps1" -Remove'

    ; Unregister COM components
    ExecWait 'regsvr32 /u /s "$INSTDIR\MirrorCam.dll"'

    ; Remove virtual audio driver
    nsExec::ExecToLog 'pnputil /delete-driver "$INSTDIR\drivers\Audio_Installer.inf" /uninstall'

    ; Remove files
    RMDir /r "$INSTDIR\Shaders"
    RMDir /r "$INSTDIR\drivers"
    RMDir /r "$INSTDIR\scripts"
    Delete   "$INSTDIR\Shaders\fullscreen_vs.cso"
    Delete   "$INSTDIR\Qt6Qml.dll"
    Delete   "$INSTDIR\Qt6QmlModels.dll"
    Delete   "$INSTDIR\Qt6QmlWorkerScript.dll"
    Delete   "$INSTDIR\Qt6QuickControls2.dll"
    Delete   "$INSTDIR\Qt6QuickControls2Impl.dll"
    Delete   "$INSTDIR\Qt6QuickTemplates2.dll"
    Delete   "$INSTDIR\Qt6ChartsQml.dll"
    Delete   "$INSTDIR\AuraCastPro.exe"
    Delete   "$INSTDIR\MirrorCam.dll"
    Delete   "$INSTDIR\*.dll"
    Delete   "$INSTDIR\Uninstall.exe"
    ; Do NOT delete logs or user settings — may contain recordings path config
    ; RMDir "$INSTDIR"  ; Only removes if empty

    ; Remove shortcuts
    Delete "$DESKTOP\AuraCastPro.lnk"
    Delete "$SMPROGRAMS\AuraCastPro\*.*"
    RMDir  "$SMPROGRAMS\AuraCastPro"

    ; Remove registry keys
    DeleteRegKey HKLM "Software\AuraCastPro"
    DeleteRegKey HKLM "${UNINSTALL_REG}"

    MessageBox MB_OK "AuraCastPro has been uninstalled.$\nYour recordings and settings in $INSTDIR have been preserved."
SectionEnd
