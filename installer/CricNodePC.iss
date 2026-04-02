; CricNode PC Recorder - Inno Setup Installer Script

[Setup]
AppName=CricNode PC Recorder
AppVersion=1.0.0
AppPublisher=CricNode
AppPublisherURL=https://github.com/krisavickcricket-code/obs-studio
DefaultDirName={localappdata}\CricNode PC Recorder
DefaultGroupName=CricNode PC Recorder
UninstallDisplayIcon={app}\bin\64bit\obs64.exe
OutputDir=Output
OutputBaseFilename=CricNodePC-Setup
SetupIconFile=cricnode.ico
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
WizardStyle=modern
DisableProgramGroupPage=yes
PrivilegesRequired=lowest

[Files]
Source: "..\build_x64\rundir\RelWithDebInfo\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\CricNode PC Recorder"; Filename: "{app}\bin\64bit\obs64.exe"; Parameters: "--portable"; WorkingDir: "{app}\bin\64bit"; IconFilename: "{app}\bin\64bit\obs64.exe"
Name: "{group}\CricNode PC Recorder"; Filename: "{app}\bin\64bit\obs64.exe"; Parameters: "--portable"; WorkingDir: "{app}\bin\64bit"; IconFilename: "{app}\bin\64bit\obs64.exe"
Name: "{group}\Uninstall CricNode PC Recorder"; Filename: "{uninstallexe}"

[Dirs]
Name: "{app}\bin\64bit\config\obs-studio\basic\profiles"
Name: "{app}\bin\64bit\config\obs-studio\basic\scenes"
Name: "{app}\bin\64bit\config\obs-studio\logs"
Name: "{app}\bin\64bit\config\obs-studio\crashes"

[Run]
Filename: "{app}\bin\64bit\obs64.exe"; Parameters: "--portable"; WorkingDir: "{app}\bin\64bit"; Description: "Launch CricNode PC Recorder"; Flags: nowait postinstall skipifsilent
