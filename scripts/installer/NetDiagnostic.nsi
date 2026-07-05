;=============================================================================
; NetDiagnostic - Windows Installer (NSIS)
; Input:  dist/installer/ (populated by windeployqt)
; Output: dist/NetDiagnostic-Windows-x86_64.exe
;=============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"

; --- Application metadata ---
!define PRODUCT_NAME     "NetDiagnostic"
!define PRODUCT_VERSION  "0.0.1"
!define PRODUCT_PUBLISHER "Otis"
!define PRODUCT_WEB_SITE "https://github.com/robinhoo1973/NetDiagnostics"
!define PRODUCT_DIR_REGKEY "Software\${PRODUCT_PUBLISHER}\${PRODUCT_NAME}"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define INSTALLER_NAME   "NetDiagnostic-Windows-x86_64.exe"
!define MAIN_EXE         "net_diagnostics.exe"
!define SOURCE_DIR       "${SOURCE_DIR_WIN}"
!define OUTPUT_DIR       "${OUTDIR_WIN}"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${OUTPUT_DIR}\${INSTALLER_NAME}"
InstallDir "$PROGRAMFILES\${PRODUCT_PUBLISHER}\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
RequestExecutionLevel admin

; --- Interface settings ---
!define MUI_ABORTWARNING
!define MUI_ICON "${ICON_WIN}"
!define MUI_UNICON "${ICON_WIN}"

; --- Pages ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

;=============================================================================
; Install section
;=============================================================================
Section "NetDiagnostic" SecMain
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer

  ; Install everything windeployqt collected (exe, dlls, plugins, qml modules)
  File /r "${SOURCE_DIR}\*.*"

  ; Write uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; --- Registry (Add/Remove Programs) ---
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName"     "${PRODUCT_NAME}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString"  '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon"      '"$INSTDIR\${MAIN_EXE}"'
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion"   "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher"        "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout"     "${PRODUCT_WEB_SITE}"
  WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoRepair" 1
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR"

  ; --- Start Menu shortcuts ---
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" \
    "$INSTDIR\${MAIN_EXE}" "" "$INSTDIR\${MAIN_EXE}" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" \
    "$INSTDIR\uninstall.exe"

  ; --- Desktop shortcut ---
  CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" \
    "$INSTDIR\${MAIN_EXE}" "" "$INSTDIR\${MAIN_EXE}" 0

SectionEnd

;=============================================================================
; Uninstall section
;=============================================================================
Section "Uninstall"
  DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"

  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\${PRODUCT_NAME}"

  RMDir /r "$INSTDIR"
SectionEnd
