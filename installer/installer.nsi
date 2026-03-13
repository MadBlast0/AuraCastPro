; =============================================================================
; AuraCastPro NSIS Installer Script
; Generates setup.exe via: makensis installer.nsi
;
; This installer:
;   1. Installs AuraCastPro.exe and all required DLLs
;   2. Installs and registers the virtual camera (MirrorCam.dll)
;   3. Installs the virtual audio driver (AudioVirtualCable.sys)
;   4. Adds Windows Firewall rules for AirPlay and Cast ports
;   5. Creates Start Menu and Desktop shortcuts
;   6. Registers uninstaller in Windows Add/Remove Programs
; =============================================================================

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

; ─── Installer metadata ───────────────────────────────────────────────────────
Name "AuraCastPro"
OutFile "AuraCastPro_Setup.exe"
InstallDir "$PROGRAMFILES64\AuraCastPro"
InstallDirRegKey HKLM "Software\AuraCastPro" "InstallDir"
RequestExecutionLevel admin
Unicode True

; Compression
SetCompressor /SOLID lzma
SetCompressorDictSize 64

; Version info for the .exe file properties
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName"      "AuraCastPro"
VIAddVersionKey "CompanyName"      "AuraCastPro"
VIAddVersionKey "FileDescription"  "AuraCastPro Setup"
VIAddVersionKey "FileVersion"      "${VERSION}"
VIAddVersionKey "LegalCopyright"   "Copyright 2025 AuraCastPro"

; ─── MUI Interface settings ───────────────────────────────────────────────────
!define MUI_ABORTWARNING
; !define MUI_ICON    "..\assets\textures\icons\png\app_icon.ico"
; !define MUI_UNICON  "..\assets\textures\icons\png\app_icon.ico"
; !define MUI_WELCOMEFINISHPAGE_BITMAP "..\assets\textures\8K_Splashes\installer_side.bmp"
; !define MUI_HEADERIMAGE
; !define MUI_HEADERIMAGE_BITMAP "..\assets\textures\8K_Splashes\installer_header.bmp"

; ─── Installer pages ─────────────────────────────────────────────────────────
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "EULA.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; ─── Uninstaller pages ───────────────────────────────────────────────────────
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; ─── Sections ─────────────────────────────────────────────────────────────────

Section "AuraCastPro Core" SecCore
    SectionIn RO  ; Required — cannot be deselected

    SetOutPath "$INSTDIR"

    ; Recursive copy of everything in Release folder, excluding build artifacts
    File /r /x *.exp /x *.lib /x *.pdb /x *.ilk "..\build\Release\*.*"

    ; Store install dir in registry
    WriteRegStr HKLM "Software\AuraCastPro" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\AuraCastPro" "Version"    "${VERSION}"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Add to Windows Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro" \
        "DisplayName" "AuraCastPro"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro" \
        "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro" \
        "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro" \
        "Publisher" "AuraCastPro"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro" \
        "DisplayIcon" "$INSTDIR\AuraCastPro.exe"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro" \
        "NoRepair" 1

SectionEnd

Section "Virtual Camera (OBS/Zoom/Teams)" SecVCam
    SetOutPath "$INSTDIR"

    ; MirrorCam virtual camera DLL (DirectShow / Media Foundation filter)
    File "..\drivers\MirrorCam.dll"
    File "..\drivers\MirrorCam_Driver.cat"
    File "..\drivers\MirrorCam_Installer.inf"

    ; Register the DirectShow filter so it appears in OBS/Zoom device lists
    ExecWait 'regsvr32.exe /s "$INSTDIR\MirrorCam.dll"'

    DetailPrint "Virtual camera registered."
SectionEnd

Section "Virtual Audio Device" SecVAudio
    SetOutPath "$INSTDIR"

    File "..\drivers\AudioVirtualCable.sys"
    File "..\drivers\Audio_Bridge.cat"
    File "..\drivers\Audio_Installer.inf"

    ; ADB tools — required for Android mirroring (Task 216)
    SetOutPath "$INSTDIR\adb"
    File "..\build\Release\adb\adb.exe"
    File "..\build\Release\adb\AdbWinApi.dll"
    File "..\build\Release\adb\AdbWinUsbApi.dll"

    ; Install the kernel driver via PnPUtil
    ExecWait 'pnputil.exe /add-driver "$INSTDIR\Audio_Installer.inf" /install'
    DetailPrint "Virtual audio driver installed."
SectionEnd

Section "Firewall Rules" SecFirewall
    ; AirPlay
    ExecWait 'netsh advfirewall firewall add rule name="AuraCastPro AirPlay TCP" \
        dir=in action=allow protocol=TCP localport=7236 program="$INSTDIR\AuraCastPro.exe"'
    ExecWait 'netsh advfirewall firewall add rule name="AuraCastPro AirPlay UDP" \
        dir=in action=allow protocol=UDP localport=7236 program="$INSTDIR\AuraCastPro.exe"'

    ; Google Cast
    ExecWait 'netsh advfirewall firewall add rule name="AuraCastPro Cast TCP" \
        dir=in action=allow protocol=TCP localport=8009 program="$INSTDIR\AuraCastPro.exe"'
    ExecWait 'netsh advfirewall firewall add rule name="AuraCastPro Cast UDP" \
        dir=in action=allow protocol=UDP localport=8010 program="$INSTDIR\AuraCastPro.exe"'

    DetailPrint "Firewall rules added."
SectionEnd

Section "Desktop Shortcut" SecDesktop
    CreateShortcut "$DESKTOP\AuraCastPro.lnk" \
        "$INSTDIR\AuraCastPro.exe" "" "$INSTDIR\AuraCastPro.exe" 0
SectionEnd

Section "Start Menu Shortcut" SecStartMenu
    CreateDirectory "$SMPROGRAMS\AuraCastPro"
    CreateShortcut "$SMPROGRAMS\AuraCastPro\AuraCastPro.lnk" \
        "$INSTDIR\AuraCastPro.exe" "" "$INSTDIR\AuraCastPro.exe" 0
    CreateShortcut "$SMPROGRAMS\AuraCastPro\Uninstall.lnk" \
        "$INSTDIR\Uninstall.exe"
SectionEnd

; ─── Section descriptions (shown in component selection page) ─────────────────
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCore}      "AuraCastPro main application (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVCam}      "Virtual camera for use in OBS Studio, Zoom, and Microsoft Teams"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVAudio}    "Virtual audio device for capturing device audio in streaming software"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ─── Uninstaller Logic ───────────────────────────────────────────────────────
Section "Uninstall"
    ; Remove files
    RMDir /r "$INSTDIR"

    ; Remove shortcuts
    Delete "$DESKTOP\AuraCastPro.lnk"
    RMDir /r "$SMPROGRAMS\AuraCastPro"

    ; Remove firewall rules
    ExecWait 'netsh advfirewall firewall delete rule name="AuraCastPro AirPlay TCP"'
    ExecWait 'netsh advfirewall firewall delete rule name="AuraCastPro AirPlay UDP"'
    ExecWait 'netsh advfirewall firewall delete rule name="AuraCastPro Cast TCP"'
    ExecWait 'netsh advfirewall firewall delete rule name="AuraCastPro Cast UDP"'

    ; Unregister virtual camera
    ExecWait 'regsvr32.exe /u /s "$INSTDIR\MirrorCam.dll"'

    ; Remove registry keys
    DeleteRegKey HKLM "Software\AuraCastPro"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuraCastPro"

    SetAutoClose true
SectionEnd
