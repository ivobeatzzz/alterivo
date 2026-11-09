; Instalador de Windows para alterIVO (Inno Setup 6.3 o superior)
; Se compila con:  ISCC packaging\windows\installer.iss
; Resultado:       dist\alterIVO-Windows-Instalador.exe

#define AppName    "alterIVO"
#define AppVersion "1.0.0"
#define Publisher  "IVO Beatz"
#define BuildDir   "..\..\build\AlterIvo_artefacts\Release\VST3"

[Setup]
AppId={{7C3F1D2A-4B8E-4E61-9A57-1F0B2C6D9E41}
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
OutputBaseFilename=alterIVO-Windows-Instalador
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
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
Name: "vst3"; Description: "alterIVO VST3 (Ableton Live, FL Studio, Cubase, Studio One, Reaper…)"; Types: full custom

[Files]
Source: "{#BuildDir}\alterIVO.vst3\*"; DestDir: "{app}\alterIVO.vst3"; Components: vst3; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\alterIVO.vst3"
