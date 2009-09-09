; This script need to use a special Inno Setup version located here : http://jrsoftware.org/isdl.php#qsp

[CustomMessages]
BEAVER_NAME=Beaver Debugger
BEAVER_COPYRIGHTS=2009 Andrei Kopats & The Monkey Studio Team
BEAVER_URL=http://beaverdbg.googlecode.com

#define BEAVER_VERSION "1.0.0.1"
#define BEAVER_REVISION "11"
#define BEAVER_SETUP_NAME "setup_beaver_" +BEAVER_VERSION +"-svn" +BEAVER_REVISION +"-win32"
#define QT_PATH "C:\Qt\2009.02"

[Setup]
OutputDir=.
SourceDir=..\..\
OutputBaseFilename={#BEAVER_SETUP_NAME}
VersionInfoVersion={#BEAVER_VERSION}
VersionInfoCompany=Monkey Studio Team
VersionInfoDescription=Free cross-platform debugger based on the Qt Creator
VersionInfoTextVersion={#BEAVER_VERSION}
VersionInfoCopyright={cm:BEAVER_COPYRIGHTS}
AppCopyright={cm:BEAVER_COPYRIGHTS}
AppName={cm:BEAVER_NAME}
AppVerName={cm:BEAVER_NAME} {#BEAVER_VERSION}
InfoAfterFile=README.txt
InfoBeforeFile=README.txt
LicenseFile=LICENSE.LGPL
ChangesAssociations=true
PrivilegesRequired=none
DefaultDirName={pf}\{cm:BEAVER_NAME}
EnableDirDoesntExistWarning=false
AllowNoIcons=true
DefaultGroupName={cm:BEAVER_NAME}
AlwaysUsePersonalGroup=true
;SetupIconFile=..\monkey\src\resources\icons\application\monkey2.ico
AppPublisher={cm:BEAVER_COPYRIGHTS}
AppPublisherURL={cm:BEAVER_URL}
AppSupportURL=
AppVersion={#BEAVER_VERSION}
AppComments=Thanks using {cm:BEAVER_NAME}
AppContact={cm:BEAVER_URL}
UninstallDisplayName={cm:BEAVER_NAME}
ShowLanguageDialog=yes

[_ISTool]
UseAbsolutePaths=false

[Files]
; MkS related files
Source: bin\beaverdbg.exe; DestDir: {app}; Flags: confirmoverwrite promptifolder
Source: README; DestDir: {app}; Flags: confirmoverwrite promptifolder isreadme
Source: README.qtcreator; DestDir: {app}; Flags: confirmoverwrite promptifolder isreadme
Source: LICENSE.LGPL; DestDir: {app}; Flags: confirmoverwrite promptifolder isreadme
Source: LGPL_EXCEPTION.TXT; DestDir: {app}; Flags: confirmoverwrite promptifolder isreadme
; Qt related files
Source: {#QT_PATH}\qt\bin\QtCore4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtGui4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtScript4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtWebKit4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtSvg4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtNetwork4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtXml4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtSql4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtCLucene4.dll; DestDir: {app}; Flags: confirmoverwrite
Source: {#QT_PATH}\qt\bin\QtHelp4.dll; DestDir: {app}; Flags: confirmoverwrite
; MinGW related files
Source: {#QT_PATH}\qt\bin\mingwm10.dll; DestDir: {app}; Flags: confirmoverwrite

[Icons]
Name: {group}\{cm:BEAVER_NAME}; Filename: {app}\beaverdbg.exe; WorkingDir: {app}; IconFilename: {app}\beaverdbg.exe
Name: {userdesktop}\{cm:BEAVER_NAME}; Filename: {app}\beaverdbg.exe; WorkingDir: {app}; IconFilename: {app}\beaverdbg.exe
Name: {group}\Home page; Filename: {app}\Home page.url; WorkingDir: {app}

[INI]
Filename: {app}\Home Page.url; Section: InternetShortcut; Key: URL; String: {cm:BEAVER_URL}; Flags: createkeyifdoesntexist uninsdeleteentry uninsdeletesectionifempty; Components: 

[UninstallDelete]
Name: {app}\Home Page.url; Type: files
Name: {app}\*.ini; Type: files
Name: {app}; Type: dirifempty
Name: {app}\beaverdbg.exe; Type: files
