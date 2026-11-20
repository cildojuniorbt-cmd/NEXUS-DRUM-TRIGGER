; Instalador do Nexus Drum Trigger para Windows (Inno Setup 6)
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SrcExe
  #define SrcExe "..\build\NexusDrumTrigger_artefacts\Release\Nexus Drum Trigger.exe"
#endif

[Setup]
AppId={{6E2B4C1A-9D57-4F1E-A3B8-5C0D7E92F4A1}
AppName=Nexus Drum Trigger
AppVersion={#AppVersion}
AppVerName=Nexus Drum Trigger {#AppVersion}
AppPublisher=Nexus Works Digital
AppPublisherURL=https://nexusworksdigital.github.io/nexus-drum-trigger/
AppSupportURL=https://nexusworksdigital.github.io/suporte.html
DefaultDirName={autopf}\Nexus Works Digital\Nexus Drum Trigger
DefaultGroupName=Nexus Works Digital
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=NexusDrumTrigger-Setup
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\Nexus Drum Trigger.exe
UninstallDisplayName=Nexus Drum Trigger
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na área de trabalho"; GroupDescription: "Atalhos:"

[Files]
Source: "{#SrcExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Nexus Drum Trigger"; Filename: "{app}\Nexus Drum Trigger.exe"
Name: "{autodesktop}\Nexus Drum Trigger"; Filename: "{app}\Nexus Drum Trigger.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Nexus Drum Trigger.exe"; Description: "Abrir o Nexus Drum Trigger agora"; Flags: nowait postinstall skipifsilent
