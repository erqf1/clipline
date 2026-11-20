; Clipline installer (Inno Setup 6/7). Build: iscc /DMyAppVersion=1.0.0 packaging\windows\clipline.iss
; Expects the portable build in dist\Clipline (see build.bat / release workflow).
#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#define MyAppId "{4F2C9A61-7D3B-4E8A-A5C2-9B1E6D0F3A77}"

[Setup]
AppId={{#MyAppId}
AppName=Clipline
AppVersion={#MyAppVersion}
AppPublisher=Clipline
DefaultDirName={autopf}\Clipline
DefaultGroupName=Clipline
UninstallDisplayIcon={app}\Clipline.exe
SetupIconFile=..\..\assets\icon.ico
OutputDir=..\..\dist
OutputBaseFilename=Clipline-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=yes
DisableDirPage=no
UsePreviousAppDir=yes
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
; Desktop-Symbol nur bei der Erst-Installation anbieten. Bei Updates (auch still über den eingebauten Updater)
; bleibt das Symbol, wie es ist: kein neues Anlegen, kein Springen an eine andere Stelle, gelöscht bleibt gelöscht.
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked; Check: IsFirstInstall

[Files]
Source: "..\..\dist\Clipline\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Clipline"; Filename: "{app}\Clipline.exe"; AppUserModelID: "WSoftware.Clipline"
Name: "{autodesktop}\Clipline"; Filename: "{app}\Clipline.exe"; AppUserModelID: "WSoftware.Clipline"; Tasks: desktopicon; Check: IsFirstInstall

[Run]
Filename: "{app}\Clipline.exe"; Description: "Start Clipline"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Laufende Instanz beenden, bevor Dateien entfernt werden
Filename: "{sys}\taskkill.exe"; Parameters: "/f /im Clipline.exe"; Flags: runhidden; RunOnceId: "StopClipline"

[Registry]
; Autostart-Eintrag (legt Clipline selbst an) beim Deinstallieren entfernen
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "Clipline"; Flags: uninsdeletevalue dontcreatekey

[Code]
var
  FirstInstall: Boolean;

// Vorherige Installation? Inno Setup trägt sich unter HKLM (alle Benutzer) oder HKCU (nur ich) ein.
// Das Desktop-Symbol der Erst-Installation steht im Deinstallations-Protokoll und wird beim echten
// Deinstallieren entfernt.
function InitializeSetup(): Boolean;
var
  Key: String;
begin
  Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppId}_is1';
  FirstInstall := not (RegKeyExists(HKLM64, Key) or RegKeyExists(HKLM32, Key) or RegKeyExists(HKCU, Key));
  Result := True;
end;

function IsFirstInstall(): Boolean;
begin
  Result := FirstInstall;
end;
