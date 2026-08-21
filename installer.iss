[Setup]
AppName=Dockless
AppVersion=1.0
UninstallDisplayName=Dockless ( Octaoss )
WizardStyle=modern
DefaultDirName={autopf}\Dockless
DefaultGroupName=Dockless
OutputDir=Output
OutputBaseFilename=Dockless_Installer
SetupIconFile=app\src\icon.ico
LicenseFile=LICENSE
PrivilegesRequired=admin
ChangesEnvironment=yes
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
; Ignore all .py files and others explicitly per requirements, but we use explicit inclusion so they are ignored by default.
Source: "app\src\icon.ico"; DestDir: "{app}\app\src"; Flags: ignoreversion
Source: "bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "driver\*"; DestDir: "{app}\driver"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "kernel\*"; DestDir: "{app}\kernel"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "vm\alpine.qcow2"; DestDir: "{app}\vm"; Flags: ignoreversion
Source: "gen-tap.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "uninstall-tap.ps1"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Dockless"; Filename: "{app}\bin\Dockless.exe"; WorkingDir: "{app}\bin"; IconFilename: "{app}\app\src\icon.ico"
Name: "{commondesktop}\Dockless"; Filename: "{app}\bin\Dockless.exe"; WorkingDir: "{app}\bin"; IconFilename: "{app}\app\src\icon.ico"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
; Run gen-tap.ps1
Filename: "powershell.exe"; Parameters: "-ExecutionPolicy Bypass -File ""{app}\gen-tap.ps1"""; Flags: waituntilterminated runhidden
; Fix permissions for SSH key to prevent "unprotected private key file" error
Filename: "icacls.exe"; Parameters: """{app}\bin\scripts\sshkeys_vm"" /inheritance:r /grant:r ""Administrators:(F)"" /grant:r ""SYSTEM:(F)"""; Flags: waituntilterminated runhidden
; Run Dockless.exe after install
Filename: "{app}\bin\Dockless.exe"; Description: "{cm:LaunchProgram,Dockless}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Run uninstall-tap.ps1
Filename: "powershell.exe"; Parameters: "-ExecutionPolicy Bypass -File ""{app}\uninstall-tap.ps1"""; Flags: waituntilterminated runhidden; RunOnceId: "UninstallTap"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: string; ValueName: "DOCKER_HOST"; ValueData: "tcp://192.168.100.2:2375"; Flags: createvalueifdoesntexist uninsdeletevalue
Root: HKLM; Subkey: "Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers"; ValueType: string; ValueName: "{app}\bin\Dockless.exe"; ValueData: "~ RUNASADMIN"; Flags: uninsdeletevalue

[Code]
const
  HostsFile = '{sys}\drivers\etc\hosts';

procedure CurStepChanged(CurStep: TSetupStep);
var
  Paths: string;
  AppDockerPath: string;
  HostsLine: AnsiString;
  HostsContent: AnsiString;
begin
  if CurStep = ssPostInstall then
  begin
    // Add to PATH
    AppDockerPath := ExpandConstant('{app}\bin\docker');
    if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', Paths) then
    begin
      if Pos(';' + AppDockerPath + ';', ';' + Paths + ';') = 0 then
      begin
        Paths := Paths + ';' + AppDockerPath;
        RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', Paths);
      end;
    end;

    // Add to hosts
    HostsLine := '192.168.100.2 dockless.local';
    LoadStringFromFile(ExpandConstant(HostsFile), HostsContent);
    if Pos(string(HostsLine), string(HostsContent)) = 0 then
    begin
      SaveStringToFile(ExpandConstant(HostsFile), HostsContent + #13#10 + HostsLine + #13#10, True);
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Paths: string;
  AppDockerPath: string;
  HostsLine: AnsiString;
  HostsContent: AnsiString;
  HostsLines: TArrayOfString;
  i: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    // Remove from PATH
    AppDockerPath := ExpandConstant('{app}\bin\docker');
    if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', Paths) then
    begin
      if Pos(';' + AppDockerPath + ';', ';' + Paths + ';') > 0 then
      begin
        StringChangeEx(Paths, ';' + AppDockerPath + ';', ';', True);
        StringChangeEx(Paths, AppDockerPath + ';', '', True);
        StringChangeEx(Paths, ';' + AppDockerPath, '', True);
        RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', Paths);
      end;
    end;

    // Remove from hosts
    HostsLine := '192.168.100.2 dockless.local';
    LoadStringFromFile(ExpandConstant(HostsFile), HostsContent);
    if Pos(string(HostsLine), string(HostsContent)) > 0 then
    begin
      if LoadStringsFromFile(ExpandConstant(HostsFile), HostsLines) then
      begin
        HostsContent := '';
        for i := 0 to GetArrayLength(HostsLines) - 1 do
        begin
          if Trim(HostsLines[i]) <> string(HostsLine) then
          begin
            if HostsContent <> '' then HostsContent := HostsContent + #13#10;
            HostsContent := HostsContent + AnsiString(HostsLines[i]);
          end;
        end;
        SaveStringToFile(ExpandConstant(HostsFile), HostsContent, False);
      end;
    end;
  end;
end;
