OutFile setup.exe
Unicode true
RequestExecutionLevel admin

!define APPNAME "Magic.TXD"
!define APPNAME_FRIENDLY "Magic TXD"

!define MUI_ICON "..\inst.ico"

!include "MUI2.nsh"
!include "Sections.nsh"
!include "x64.nsh"

InstallDir "$PROGRAMFILES\${APPNAME_FRIENDLY}"
Name "${APPNAME}"

var StartMenuFolder

!define COMPONENT_REG_PATH  "Software\Magic.TXD"

!define MUI_STARTMENUPAGE_REGISTRY_ROOT HKLM
!define MUI_STARTMENUPAGE_REGISTRY_KEY ${COMPONENT_REG_PATH}
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "SMFolder"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!define MUI_PAGE_CUSTOMFUNCTION_PRE pre_startmenu
!insertmacro MUI_PAGE_STARTMENU SM_PAGE_ID $StartMenuFolder
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

!include LogicLib.nsh

!macro CHECK_ADMIN
UserInfo::GetAccountType
pop $0
${If} $0 != "admin"
    MessageBox mb_iconstop "Please run this setup using administrator rights."
    SetErrorLevel 740
    Quit
${EndIf}
!macroend

!macro UPDATE_INSTDIR
${If} ${RunningX64}
    StrCpy $INSTDIR "$PROGRAMFILES64\${APPNAME_FRIENDLY}"
${Else}
    StrCpy $INSTDIR "$PROGRAMFILES\${APPNAME_FRIENDLY}"
${EndIf}
!macroend

!macro INCLUDE_FORMATS dir
File "${dir}\a8.magf"
File "${dir}\v8u8.magf"
!macroend

!macro SHARED_INSTALL
setOutPath $INSTDIR\resources
File /r "..\..\resources\*"
${If} ${RunningX64}
    setOutPath $INSTDIR\formats_x64
    !insertmacro INCLUDE_FORMATS "..\..\output\formats_x64"
${Else}
    setOutPath $INSTDIR\formats
    !insertmacro INCLUDE_FORMATS "..\..\output\formats"
${EndIf}
setOutPath $INSTDIR
${If} ${RunningX64}
    File "..\..\rwlib\vendor\pvrtexlib\Windows_x86_64\Dynamic\PVRTexLib.dll"
${Else}
    File "..\..\rwlib\vendor\pvrtexlib\Windows_x86_32\Dynamic\PVRTexLib.dll"
${EndIf}
File /r "..\..\releasefiles\*"
!macroend

!macro REG_INIT
# Access the proper registry view.
${If} ${RunningX64}
    SetRegView 64
${Else}
    SetRegView 32
${EndIf}
!macroend

VAR DoStartMenu

!define INST_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Magic.TXD"
!define SM_PATH "$SMPROGRAMS\$StartMenuFolder"

!define REDIST32_120 "https://download.microsoft.com/download/2/E/6/2E61CFA4-993B-4DD4-91DA-3737CD5CD6E3/vcredist_x86.exe"
!define REDIST64_120 "https://download.microsoft.com/download/2/E/6/2E61CFA4-993B-4DD4-91DA-3737CD5CD6E3/vcredist_x64.exe"

!define REDIST32_120_KEY "SOFTWARE\Wow6432Node\Microsoft\DevDiv\vc\Servicing\12.0\RuntimeMinimum"
!define REDIST64_120_KEY "SOFTWARE\Microsoft\DevDiv\vc\Servicing\12.0\RuntimeMinimum"

!define REDIST32_140 "https://download.microsoft.com/download/9/3/F/93FCF1E7-E6A4-478B-96E7-D4B285925B00/vc_redist.x86.exe"
!define REDIST64_140 "https://download.microsoft.com/download/9/3/F/93FCF1E7-E6A4-478B-96E7-D4B285925B00/vc_redist.x64.exe"

!define REDIST32_140_KEY "SOFTWARE\Wow6432Node\Microsoft\DevDiv\vc\Servicing\14.0\RuntimeMinimum"
!define REDIST64_140_KEY "SOFTWARE\Microsoft\DevDiv\vc\Servicing\14.0\RuntimeMinimum"

!macro INSTALL_REDIST name friendlyname url32 url64 key32 key64
    # Check whether the user has to install a redistributable and which.
    StrCpy $3 0
    StrCpy $4 "HTTP://REDIST.PATH/"
    
    ${If} ${RunningX64}
        ReadRegDWORD $3 HKLM "${key64}" "Install"
        StrCpy $4 ${url64}
    ${Else}
        ReadRegDWORD $3 HKLM "${key32}" "Install"
        StrCpy $4 ${url32}
    ${EndIf}
    
    ${If} $3 == 0
    ${OrIf} $3 == ""
        # If required, install the redistributable.
        StrCpy $1 "$TEMP\${name}"
        
        NSISdl::download $4 $1
        
        pop $0
        ${If} $0 != "success"
            MessageBox mb_iconstop "Failed to download the ${friendlyname} redistributable. Please verify connection to the Internet and try again."
            SetErrorLevel 2
            Quit
        ${EndIf}
        
        # Run the redistributable installer.
        ExecWait "$1 /install /passive" $0
        
        # Delete the installer once we are done.
        Delete $1
        
        ${If} $0 != 0
            MessageBox mb_iconstop "Installation of the ${friendlyname} redistributable was not successful. Please try installing again."
            Quit
        ${EndIf}
    ${EndIf}
!macroend

Section "-Magic.TXD core"
    # Install all required redistributables.
    !insertmacro INSTALL_REDIST "vcredist_2013.exe" "VS2013" ${REDIST32_120} ${REDIST64_120} ${REDIST32_120_KEY} ${REDIST64_120_KEY}
    !insertmacro INSTALL_REDIST "vcredist_2015.exe" "VS2015" ${REDIST32_140} ${REDIST64_140} ${REDIST32_140_KEY} ${REDIST64_140_KEY}

    # Install the main program.
    setOutPath $INSTDIR
    ${If} ${RunningX64}
        File /oname=magictxd.exe "..\..\output\magictxd_x64.exe"
    ${Else}
        File /oname=magictxd.exe "..\..\output\magictxd.exe"
    ${EndIf}
    !insertmacro SHARED_INSTALL
    WriteUninstaller $INSTDIR\uinst.exe
    
    # Information for add/remove programs.
    WriteRegStr HKLM ${INST_REG_KEY} "DisplayName" "Magic.TXD"
    WriteRegStr HKLM ${INST_REG_KEY} "DisplayIcon" "$INSTDIR\magictxd.exe"
    WriteRegStr HKLM ${INST_REG_KEY} "DisplayVersion" "1.0"
    WriteRegDWORD HKLM ${INST_REG_KEY} "NoModify" 1
    WriteRegDWORD HKLM ${INST_REG_KEY} "NoRepair" 1
    WriteRegStr HKLM ${INST_REG_KEY} "Publisher" "GTA community"
    WriteRegStr HKLM ${INST_REG_KEY} "UninstallString" "$\"$INSTDIR\uinst.exe$\""
    
    # Write meta information.
    WriteRegStr HKLM ${COMPONENT_REG_PATH} "InstallDir" "$INSTDIR"
SectionEnd

Section /o "Startmenu Shortcuts" STARTMEN_SEC_ID
    !insertmacro MUI_STARTMENU_WRITE_BEGIN SM_PAGE_ID
        createDirectory "${SM_PATH}"
        createShortcut "${SM_PATH}\Magic.TXD.lnk" "$INSTDIR\magictxd.exe"
        createShortcut "${SM_PATH}\Remove Magic.TXD.lnk" "$INSTDIR\uinst.exe"
    !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

!define TXD_ASSOC_KEY ".txd"
!define MGTXD_TXD_ASSOC "${APPNAME}.txd"

Section "Associate .txd files" ASSOC_TXD_ID
    # Write some registry settings for .TXD files.
    WriteRegStr HKCR "${TXD_ASSOC_KEY}" "" "${MGTXD_TXD_ASSOC}"
    WriteRegStr HKCR "${TXD_ASSOC_KEY}" "Content Type" "image/dict"
    WriteRegStr HKCR "${TXD_ASSOC_KEY}" "PerceivedType" "image"
    WriteRegStr HKCR "${TXD_ASSOC_KEY}\OpenWithProgids" "${MGTXD_TXD_ASSOC}" ""
    
    # Now write Magic.TXD association information.
    WriteRegStr HKCR "${MGTXD_TXD_ASSOC}\DefaultIcon" "" "$INSTDIR\magictxd.exe"
    WriteRegStr HKCR "${MGTXD_TXD_ASSOC}\shell\open\command" "" "$\"$INSTDIR\magictxd.exe$\" $\"%1$\""
SectionEnd

Section "Shell Integration" SHELL_INT_ID
    ${If} ${RunningX64}
        File /oname=rwshell.dll "..\..\thumbnail\bin\rwshell_x64.dll"
    ${Else}
        File /oname=rwshell.dll "..\..\thumbnail\bin\rwshell.dll"
    ${EndIf}
    
    ; Do the intergration.
    ExecWait 'regsvr32.exe /s "$INSTDIR\rwshell.dll"'
SectionEnd

LangString DESC_SMShortcut ${LANG_ENGLISH} "Creates shortcuts to Magic.TXD in the startmenu."
LangString DESC_TXDAssoc ${LANG_ENGLISH} "Associates .txd files in Windows Explorer with Magic.TXD."
LangString DESC_ShellInt ${LANG_ENGLISH} "Provides thumbnails and context menu extensions for TXD files."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${STARTMEN_SEC_ID} $(DESC_SMShortcut)
    !insertmacro MUI_DESCRIPTION_TEXT ${ASSOC_TXD_ID} $(DESC_TXDAssoc)
    !insertmacro MUI_DESCRIPTION_TEXT ${SHELL_INT_ID} $(DESC_ShellInt)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onInit
    !insertmacro CHECK_ADMIN
    !insertmacro REG_INIT
    !insertmacro UPDATE_INSTDIR
    
    # Check through the registry whether Magic.TXD is installed already.
    # We do not want to install twice.
    StrCpy $0 ""
    ReadRegStr $0 HKLM ${COMPONENT_REG_PATH} "InstallDir"
    
    ${If} $0 != ""
        MessageBox MB_ICONINFORMATION|MB_OK "Setup has detected through the registry that Magic.TXD has already been installed at $\"$0$\".$\n$\n Please uninstall Magic.TXD before installing it again."
        Quit
    ${EndIf}
    
    StrCpy $DoStartMenu "false"
FunctionEnd

Function un.onInit
    !insertmacro CHECK_ADMIN
    !insertmacro REG_INIT

    # Try to fetch installation directory.
    StrCpy $0 ""
    ReadRegStr $0 HKLM ${COMPONENT_REG_PATH} "InstallDir"
    
    ${If} $0 != ""
        StrCpy $INSTDIR $0
    ${EndIf}
FunctionEnd

Section un.defUninst
    ; Unregister file association if present.
    DeleteRegKey HKCR "${MGTXD_TXD_ASSOC}"
    DeleteRegValue HKCR "${TXD_ASSOC_KEY}\OpenWithProgids" "${MGTXD_TXD_ASSOC}"
    
    IfFileExists $INSTDIR\rwshell.dll 0 uninstmain
    
    ; Unregister shell extension.
    ExecWait 'regsvr32.exe /u /s "$INSTDIR\rwshell.dll"'
    Delete /REBOOTOK $INSTDIR\rwshell.dll

uninstmain:
    RMDIR /r $INSTDIR\resources
    RMDIR /r $INSTDIR\licenses
    RMDIR /r /REBOOTOK $INSTDIR\formats
    RMDIR /r /REBOOTOK $INSTDIR\formats_x64
    Delete /REBOOTOK $INSTDIR\PVRTexLib.dll
    Delete $INSTDIR\magictxd.exe
    Delete $INSTDIR\uinst.exe
    RMDIR $INSTDIR
    
    !insertmacro MUI_STARTMENU_GETFOLDER SM_PAGE_ID $StartMenuFolder
    
    ${If} $StartMenuFolder != ""
        ; Delete shortcut stuff.
        Delete "${SM_PATH}\Magic.TXD.lnk"
        Delete "${SM_PATH}\Remove Magic.TXD.lnk"
        RMDIR "${SM_PATH}"
    ${EndIf}
    
    # Remove the registry key that is responsible for shell stuff.
    DeleteRegKey /ifempty HKCR "${TXD_ASSOC_KEY}"
        
    DeleteRegKey HKLM ${INST_REG_KEY}
    DeleteRegKey HKLM ${COMPONENT_REG_PATH}
SectionEnd

Function pre_startmenu
    SectionGetFlags ${STARTMEN_SEC_ID} $R0
    IntOp $R0 $R0 & ${SF_SELECTED}

    ${If} $R0 != ${SF_SELECTED}
        abort
    ${EndIf}
FunctionEnd