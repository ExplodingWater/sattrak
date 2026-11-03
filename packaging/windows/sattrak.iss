; Build through the CMake windows_installer target after a Release build.
#ifndef SourceDir
  #error SourceDir must point to the CMake Runtime staging directory.
#endif
#ifndef AudioHelper
  #error AudioHelper must point to the CMake-built installer audio helper.
#endif
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\build\installer"
#endif

[Setup]
AppId={{B06EC57D-96F9-419D-B250-65D07DE148DE}
AppName=sattrak
AppVersion={#AppVersion}
AppPublisher=Martin Hafizi
AppPublisherURL=https://github.com/ExplodingWater/sattrak
AppSupportURL=https://github.com/ExplodingWater/sattrak/issues
DefaultDirName={localappdata}\Programs\sattrak
DefaultGroupName=sattrak
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DisableDirPage=no
DisableProgramGroupPage=yes
DisableWelcomePage=no
UsePreviousAppDir=yes
WizardStyle=modern
WizardImageFile=..\..\assets\branding\wizard-side.bmp
WizardSmallImageFile=..\..\assets\branding\wizard-small.bmp
WizardImageStretch=yes
LicenseFile=..\..\LICENSE
InfoBeforeFile=..\..\TERMS.md
UninstallDisplayName=sattrak
UninstallDisplayIcon={app}\ag.exe
SetupIconFile=..\..\assets\branding\sattrak.ico
OutputDir={#OutputDir}
OutputBaseFilename=sattrak-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; Keep the temporary soundtrack first so Welcome does not decompress the app payload.
Source: "{#AudioHelper}"; DestName: "sattrak-installer-audio.exe"; Flags: dontcopy noencryption
Source: "{#SourceDir}\assets\music\2003 Toyota Corolla.mp3"; DestName: "installer-music.mp3"; Flags: dontcopy noencryption
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\sattrak"; Filename: "{app}\ag.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\sattrak"; Filename: "{app}\ag.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ag.exe"; Description: "Launch sattrak"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel1=Welcome to sattrak
WelcomeLabel2=Explore Earth from orbit!%n%nThis wizard installs sattrak for your Windows account,%nincluding the spacecraft models and offline satellite catalog.

; User-created files (local/, recordings and preferences) are deliberately not
; listed in UninstallDelete. Inno removes only the installed payload and shortcuts.

#include "installer-music.iss"
