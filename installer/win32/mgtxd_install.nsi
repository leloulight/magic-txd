OutFile setup.exe
Unicode true
RequestExecutionLevel admin

!define APPNAME "Magic.TXD"

!define MUI_ICON "..\inst.ico"

!include "MUI2.nsh"
!include "Sections.nsh"
!include "x64.nsh"

InstallDir $PROGRAMFILES\${APPNAME}
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
    StrCpy $INSTDIR $PROGRAMFILES64\${APPNAME}
${Else}
    StrCpy $INSTDIR $PROGRAMFILES\${APPNAME}
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

!define REDIST32 "https://download.microsoft.com/download/9/3/F/93FCF1E7-E6A4-478B-96E7-D4B285925B00/vc_redist.x86.exe"
!define REDIST64 "https://download.microsoft.com/download/9/3/F/93FCF1E7-E6A4-478B-96E7-D4B285925B00/vc_redist.x64.exe"

!define REDIST32_KEY "SOFTWARE\Wow6432Node\Microsoft\DevDiv\vc\Servicing\14.0\RuntimeMinimum"
!define REDIST64_KEY "SOFTWARE\Microsoft\DevDiv\vc\Servicing\14.0\RuntimeMinimum"

Section "-Magic.TXD core"
    # Check whether the user has to install a redistributable and which.
    StrCpy $3 0
    StrCpy $4 "HTTP://REDIST.PATH/"
    
    ${If} ${RunningX64}
        ReadRegDWORD $3 HKLM "${REDIST64_KEY}" "Install"
        StrCpy $4 ${REDIST64}
    ${Else}
        ReadRegDWORD $3 HKLM "${REDIST32_KEY}" "Install"
        StrCpy $4 ${REDIST32}
    ${EndIf}
    
    ${If} $3 == 0
    ${OrIf} $3 == ""
        # If required, install the Visual Studio 2015 redistributable.
        GetTempFileName $1
        
        NSISdl::download $4 $1
        
        pop $0
        ${If} $0 != "success"
            MessageBox mb_iconstop "Failed to download the VS2015 redistributable. Please verify connection to the Internet and try again."
            SetErrorLevel 2
            Quit
        ${EndIf}
        
        # Run the redistributable installer.
        ExecWait "$1 /install /passive" $0
        
        # Delete the installer once we are done.
        Delete $1
        
        ${If} $0 != 0
            MessageBox mb_iconstop "Installation of the VS2015 redistributable was not successful. Please try installing again."
            Quit
        ${EndIf}
    ${EndIf}

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
        createShortcut "${SM_PATH}\Uninstall.lnk" "$INSTDIR\uinst.exe"
    !insertmacro MUI_STARTMENU_WRITE_END
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
LangString DESC_ShellInt ${LANG_ENGLISH} "Provides thumbnails and context menu extensions for TXD files."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${STARTMEN_SEC_ID} $(DESC_SMShortcut)
    !insertmacro MUI_DESCRIPTION_TEXT ${SHELL_INT_ID} $(DESC_ShellInt)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onInit
    !insertmacro CHECK_ADMIN
    !insertmacro REG_INIT
    !insertmacro UPDATE_INSTDIR
    
    StrCpy $DoStartMenu "false"
FunctionEnd

Function un.onInit
    !insertmacro CHECK_ADMIN
    !insertmacro REG_INIT
    !insertmacro UPDATE_INSTDIR
FunctionEnd

Section un.defUninst
    ; TODO: startmenu shortcuts
    
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
        Delete "${SM_PATH}\Uninstall.lnk"
        RMDIR "${SM_PATH}"
    ${EndIf}
        
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