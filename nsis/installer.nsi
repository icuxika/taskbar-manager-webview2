Unicode True
ManifestDPIAware true

;-------------------------------- 
; Product info
!define PRODUCT_RESOURCE "SourceDir"
!define PRODUCT_NAME "TaskbarManager"
!define PRODUCT_FILE "taskbar-manager-webview2"
!define PRODUCT_URI_SCHEME "TaskbarManager"

NAME "${PRODUCT_NAME}"
OutFile "Install ${PRODUCT_NAME}.exe"

;--------------------------------
!define MUI_ICON icon.ico
!define MUI_UNICON icon.ico
;--------------------------------
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY "${UNINSTKEY}"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME "CurrentUser"
!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_INSTDIR "$(^Name)"
!define MULTIUSER_USE_PROGRAMFILES64
!define MULTIUSER_INSTALLMODE_COMMANDLINE

;--------------------------------
;Language Selection Dialog Settings
!define MUI_LANGDLL_ALWAYSSHOW
!define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
!define MUI_LANGDLL_REGISTRY_KEY "Software\${PRODUCT_NAME}" 
!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

!define MUI_CUSTOMFUNCTION_GUIINIT myGuiInit

!include MultiUser.nsh
!include MUI2.nsh
!include LogicLib.nsh

;RequestExecutionLevel user
InstallDir ""

!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_SHOW addLicense
!insertmacro MUI_PAGE_LICENSE "License.rtf"
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

    !define MUI_FINISHPAGE_NOAUTOCLOSE
    !define MUI_FINISHPAGE_RUN
    !define MUI_FINISHPAGE_RUN_NOTCHECKED
    !define MUI_FINISHPAGE_RUN_FUNCTION "LaunchLink"
    !define MUI_FINISHPAGE_SHOWREADME $INSTDIR\readme.txt
    !define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
    !define MUI_FINISHPAGE_LINK "https://www.aprillie.com/cache/"
    !define MUI_FINISHPAGE_LINK_LOCATION "https://www.aprillie.com/cache/"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

!include "languages\SimpChinese.nsh"
!include "languages\English.nsh"

!macro UninstallExisting exitcode uninstcommand
Push `${uninstcommand}`
Call UninstallExisting
Pop ${exitcode}
!macroend
Function UninstallExisting
Exch $1 ; uninstcommand
Push $2 ; Uninstaller
Push $3 ; Len
StrCpy $3 ""
StrCpy $2 $1 1
StrCmp $2 '"' qloop sloop
sloop:
	StrCpy $2 $1 1 $3
	IntOp $3 $3 + 1
	StrCmp $2 "" +2
	StrCmp $2 ' ' 0 sloop
	IntOp $3 $3 - 1
	Goto run
qloop:
	StrCmp $3 "" 0 +2
	StrCpy $1 $1 "" 1 ; Remove initial quote
	IntOp $3 $3 + 1
	StrCpy $2 $1 1 $3
	StrCmp $2 "" +2
	StrCmp $2 '"' 0 qloop
run:
	StrCpy $2 $1 $3 ; Path to uninstaller
	StrCpy $1 161 ; ERROR_BAD_PATHNAME
	GetFullPathName $3 "$2\.." ; $InstDir
	IfFileExists "$2" 0 +4
	ExecWait '"$2" /S _?=$3' $1 ; This assumes the existing uninstaller is a NSIS uninstaller, other uninstallers don't support /S nor _?=
	IntCmp $1 0 "" +2 +2 ; Don't delete the installer if it was aborted
	Delete "$2" ; Delete the uninstaller
	RMDir "$3" ; Try to delete $InstDir
	RMDir "$3\.." ; (Optional) Try to delete the parent of $InstDir
Pop $3
Pop $2
Exch $1 ; exitcode
FunctionEnd

Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
  !insertmacro MULTIUSER_INIT

  File "/oname=$PluginsDir\License-SimpChinese.txt" "licenses\License-SimpChinese.txt"
  File "/oname=$PluginsDir\License-English.txt" "licenses\License-English.txt"
FunctionEnd

Function myGUIInit
  ReadRegStr $0 ShCtx "${UNINSTKEY}" "UninstallString"
  ${If} $0 != ""
  ${AndIf} ${Cmd} `MessageBox MB_YESNO|MB_ICONQUESTION "$(uninstallPreviousVersion)" /SD IDYES IDYES`
    !insertmacro UninstallExisting $0 $0
    ${If} $0 <> 0
      MessageBox MB_YESNO|MB_ICONSTOP "$(failedToUninstallPreviousVersion)" /SD IDYES IDYES +2
        Abort
    ${EndIf}
  ${EndIf}
FunctionEnd

Function un.onInit
  !insertmacro MUI_UNGETLANGUAGE
  !insertmacro MULTIUSER_UNINIT
FunctionEnd

;-------------------------------- 
;Installer Sections     

Section "MainSection" 
  SetOutPath $INSTDIR
  File /r "${PRODUCT_RESOURCE}\*.*"
  File readme.txt
  File index.html

  ;create desktop shortcut
  CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_FILE}.exe" ""

  ;create start-menu items
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_FILE}.exe" "" "$INSTDIR\${PRODUCT_FILE}.exe" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Call ${PRODUCT_NAME}.lnk" "$INSTDIR\index.html"

  ;write uninstall information to the registry
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr ShCtx "${UNINSTKEY}" DisplayName "$(^Name)"
  WriteRegStr ShCtx "${UNINSTKEY}" DisplayIcon '"$INSTDIR\${PRODUCT_FILE}.exe"'
  WriteRegStr ShCtx "${UNINSTKEY}" UninstallString '"$INSTDIR\Uninstall.exe"'
  WriteRegStr ShCtx "${UNINSTKEY}" $MultiUser.InstallMode 1

  ;registering the application handling the custom uri scheme
  DeleteRegKey HKCR "${PRODUCT_URI_SCHEME}"
  WriteRegStr HKCR "${PRODUCT_URI_SCHEME}" "" "URL:${PRODUCT_NAME} Protocol"
  WriteRegStr HKCR "${PRODUCT_URI_SCHEME}" "URL Protocol" ""
  WriteRegStr HKCR "${PRODUCT_URI_SCHEME}\DefaultIcon" "" "$INSTDIR\${PRODUCT_FILE}.exe"
  WriteRegStr HKCR "${PRODUCT_URI_SCHEME}\shell" "" ""
  WriteRegStr HKCR "${PRODUCT_URI_SCHEME}\shell\open" "" ""
  WriteRegStr HKCR "${PRODUCT_URI_SCHEME}\shell\open\command" "" "$INSTDIR\${PRODUCT_FILE}.exe %1"

  ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
  ${If} $0 != 1
    IfFileExists "$INSTDIR\VC_redist.x64.exe" vcredist_found vcredist_missing
    vcredist_found:
      DetailPrint "正在安装 Microsoft Visual C++ 可再发行程序包"
      ExecWait '"$INSTDIR\VC_redist.x64.exe" /install /quiet /norestart' $1
      Delete "$INSTDIR\VC_redist.x64.exe"
      DetailPrint "Microsoft Visual C++ 可再发行程序包安装完成"
      Goto vcredist_end
    vcredist_missing:
      DetailPrint "电脑中未安装 Microsoft Visual C++ 可再发行程序包"
      DetailPrint "应用程序安装中不包含 VC_redist.x64.exe"
    vcredist_end:
  ${Else}
    IfFileExists "$INSTDIR\VC_redist.x64.exe" file_exists file_not_exists
    file_exists:
      Delete "$INSTDIR\VC_redist.x64.exe"
      Goto done
    file_not_exists:
    done:
  ${EndIf}
SectionEnd

;--------------------------------    
;Uninstaller Section  
Section "Uninstall"
 
  ;Delete Files 
  RMDir /r "$INSTDIR\*.*"    
 
  ;Remove the installation directory
  RMDir "$INSTDIR"
 
  ;Delete Start Menu Shortcuts
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\*.*"
  RmDir  "$SMPROGRAMS\${PRODUCT_NAME}"
 
  ;Delete Uninstaller And Unistall Registry Entries
  DeleteRegKey ShCtx "${UNINSTKEY}"

  ;Delete the Custom URI Scheme
  DeleteRegKey HKCR "${PRODUCT_URI_SCHEME}"
SectionEnd

;--------------------------------    
;MessageBox Section

Function .onInstSuccess
  MessageBox MB_OK "$(alreadyInstalled)"
FunctionEnd
 
Function un.onUninstSuccess
  MessageBox MB_OK "$(alreadyUninstalled)"
FunctionEnd

Function LaunchLink
  ExecShell "" "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
FunctionEnd

Function addLicense
  ;use !echo and DetailPrint -> https://nsis.sourceforge.io/Reference/!echo

  ClearErrors

  ${If} $LANGUAGE == "2052"
    ;current language is SimpChinese
    FileOpen $0 "$PluginsDir\License-SimpChinese.txt" r
    IfErrors exit
  ${Else}
    ;current language is English
    FileOpen $0 "$PluginsDir\License-English.txt" r
    IfErrors exit
  ${EndIf}

  FindWindow $1 "#32770" "" $HWNDPARENT
  GetDlgItem $1 $1 1000
  SendMessage $1 ${EM_SETLIMITTEXT} 0 0
  SendMessage $1 ${WM_SETTEXT} 0 "STR:"

  ${Do}
    FILEREADUTF16LE $0 $2
    ${If} $2 == ""
      ${ExitDo}
    ${EndIf}
    SendMessage $1 ${EM_REPLACESEL} 0 "STR:$2"
  ${Loop}
  FileClose $0
exit:
FunctionEnd