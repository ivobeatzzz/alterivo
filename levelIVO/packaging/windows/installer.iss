; Instalador de Windows para levelIVO (Inno Setup 6.3 o superior)
; Se compila con:  ISCC packaging\windows\installer.iss
; Resultado:       dist\levelIVO-Windows-Instalador.exe

#define AppName    "levelIVO"
#define AppVersion "0.1.0"
#define Publisher  "IVO Beatz"
#define BuildDir   "..\..\build\LevelIvo_artefacts\Release\VST3"

[Setup]
AppId={{27184883-163B-496D-A2CD-5479A2307E26}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL=https://ivobeatz.com
DefaultDirName={commoncf64}\VST3
DirExistsWarning=no
AppendDefaultDirName=no
DisableProgramGroupPage=yes
UninstallFilesDir={commonpf64}\{#Publisher}\{#AppName}
UninstallDisplayName={#AppName} ({#Publisher})
OutputDir=..\..\dist
OutputBaseFilename=levelIVO-Windows-Instalador
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=ivo-logo.ico
UninstallDisplayIcon={commonpf64}\{#Publisher}\{#AppName}\ivo-logo.ico
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
spanish.SelectDirLabel3=Elige la carpeta de plugins VST3. La normal es C:\Program Files\Common Files\VST3.
english.SelectDirLabel3=Choose your VST3 plug-in folder. The standard one is C:\Program Files\Common Files\VST3.

[Types]
Name: "full";   Description: "Instalación completa"
Name: "custom"; Description: "Personalizada"; Flags: iscustom

[Components]
Name: "vst3"; Description: "levelIVO VST3 (Ableton Live, FL Studio, Cubase, Studio One, Reaper…)"; Types: full custom

[Files]
Source: "{#BuildDir}\levelIVO.vst3\*"; DestDir: "{app}\levelIVO.vst3"; Components: vst3; \
    Flags: ignoreversion recursesubdirs createallsubdirs
Source: "ivo-logo.ico"; DestDir: "{commonpf64}\{#Publisher}\{#AppName}"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}\levelIVO.vst3"
