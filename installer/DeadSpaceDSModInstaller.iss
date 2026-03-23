
; DeadSpaceDSModInstaller.iss
; Installer script for Dead Space - DualSensitive Mod

#define ModName "Dead Space — DualSensitive Mod"
#define ModExeName "dualsensitive-service.exe"
#define VbsScript "launch-service.vbs"
#define INIFile "dualsense-mod.ini"
#define PluginDLL "dualsense-mod.dll"
#define ProxyDLL "dinput8.dll"
#define AppId "DeadSpace.DualSensitive.Mod"

[Setup]
AppId={#AppId}
AppName={#ModName}
AppVersion=1.0
VersionInfoVersion=1.0.0.0
VersionInfoTextVersion=1.0.0.0
VersionInfoProductVersion=1.0.0.0
DefaultDirName={autopf}\DualSensitive\{#ModName}
DefaultGroupName={#ModName}
OutputDir=.
OutputBaseFilename=DeadSpace-DualSensitive-Mod_Setup
Compression=lzma
SolidCompression=yes
SetupIconFile=assets\installer.ico
WizardSmallImageFile=assets\DualSensitive_dark.bmp
WizardImageFile=assets\installer_bg_240x459_blackfill.bmp
UninstallDisplayIcon={app}\uninstaller.ico
UninstallDisplayName={#ModName}
DisableProgramGroupPage=yes

[CustomMessages]
InstallInfoLine1=Install Dead Space — DualSensitive Mod for one or more game versions below.
InstallInfoLine2=You can select Steam, EA App, or a custom installation path. Leave unchecked any
InstallInfoLine3=version you don't want to mod.
InstallInfoLine4=To continue, click Next. If you would like to select a different directory, click Browse.

[Files]
Source: "files\{#ProxyDLL}"; DestDir: "{code:GetInstallPath}"; Flags: ignoreversion uninsrestartdelete
Source: "files\{#PluginDLL}"; DestDir: "{code:GetInstallPath}\mods"; Flags: ignoreversion uninsrestartdelete
Source: "files\{#INIFile}"; DestDir: "{code:GetInstallPath}\mods"; Flags: ignoreversion uninsrestartdelete
Source: "files\{#ModExeName}"; DestDir: "{code:GetInstallPath}\mods"; Flags: ignoreversion uninsrestartdelete
Source: "files\{#VbsScript}"; DestDir: "{code:GetInstallPath}\mods"; Flags: ignoreversion uninsrestartdelete
Source: "assets\uninstaller (1).ico"; DestDir: "{app}"; DestName: "uninstaller.ico"; Flags: ignoreversion
Source: "assets\DualSensitive_dark.png"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall {#ModName}"; Filename: "{uninstallexe}"

[Run]
Filename: "schtasks.exe"; \
  Parameters: "{code:GetSchedTaskCommand}"; \
  Flags: runhidden

[Registry]
Root: HKCU; Subkey: "Software\DualSensitive\{#AppId}"; ValueType: string; ValueName: "GameInstallPath"; ValueData: "{code:GetInstallPath}"; Flags: uninsdeletekey

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Code]
var
  DisclaimerCheckBox: TNewCheckBox;
  DisclaimerAccepted: Boolean;
  DisclaimerPage: TWizardPage;
  SteamCheckbox, EACheckbox, ManualCheckbox: TCheckBox;
  ManualPathEdit: TEdit;
  ManualBrowseButton: TButton;
  EAInstallPath: string;
  SelectedInstallPath: string;
  MyPage: TWizardPage;
  FinishExtrasCreated: Boolean;
  SteamInstallPath: string;


procedure SafeSetParent(Control: TControl; ParentCtrl: TWinControl);
begin
  Control.Parent := ParentCtrl;
end;

function IsWindows11OrNewer: Boolean;
var
  s: string;
  build: Integer;
begin
  s := '';
  if not RegQueryStringValue(HKLM,
    'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'CurrentBuildNumber', s) then
  begin
    RegQueryStringValue(HKLM,
      'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'CurrentBuild', s);
  end;
  build := StrToIntDef(s, 0);
  Result := (build >= 22000);
end;

procedure CurPageChangedCheck(Sender: TObject);
begin
  DisclaimerAccepted := TNewCheckBox(Sender).Checked;
  if not WizardSilent and Assigned(WizardForm.NextButton) then
    WizardForm.NextButton.Enabled := DisclaimerAccepted;
end;

procedure CreateDisclaimerPage();
var
  Memo: TMemo;
begin
  DisclaimerAccepted := False;
  DisclaimerPage := CreateCustomPage(
    wpWelcome,
    'Disclaimer',
    'Please read and accept the following disclaimer before continuing.'
  );

  Memo := TMemo.Create(WizardForm);
  Memo.Parent := DisclaimerPage.Surface;
  Memo.Left := ScaleX(0);
  Memo.Top := ScaleY(0);
  Memo.Width := DisclaimerPage.Surface.Width;
  Memo.Height := ScaleY(150);
  Memo.ReadOnly := True;
  Memo.ScrollBars := ssVertical;
  Memo.WordWrap := True;
  Memo.Text :=
'This mod is provided "as is" with no warranty or guarantee of performance.' + #13#10 +
'By continuing, you acknowledge that you are installing third-party software' + #13#10 +
'which may modify or interact with the game in ways not intended by its original developers.' + #13#10 +
'' + #13#10 +
'Use at your own risk. The authors and platforms are not responsible' + #13#10 +
'for any damage, data loss, or other issues caused by this software.' + #13#10 +
'' + #13#10 +
'This is a non-commercial fan-made project. All rights to the game "Dead Space"' + #13#10 +
'and its characters belong to Motive Studio and Electronic Arts.' + #13#10 +
'' + #13#10 +
'Created by Thanos Petsas - https://thanasispetsas.com';

  DisclaimerCheckBox := TNewCheckBox.Create(WizardForm);
  if Assigned(DisclaimerCheckBox) then
  begin
    DisclaimerCheckBox.Parent := DisclaimerPage.Surface;
    DisclaimerCheckBox.Top := Memo.Top + Memo.Height + ScaleY(8);
    DisclaimerCheckBox.Left := ScaleX(0);
    DisclaimerCheckBox.Width := DisclaimerPage.Surface.Width;
    DisclaimerCheckBox.Height := ScaleY(20);
    DisclaimerCheckBox.Caption := 'I have read and accept the disclaimer above.';
    DisclaimerCheckBox.OnClick := @CurPageChangedCheck;
  end;

  if not WizardSilent and Assigned(WizardForm.NextButton) then
    WizardForm.NextButton.Enabled := False;
end;


function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if Assigned(DisclaimerPage) and (PageID = DisclaimerPage.ID) then
    Result := DisclaimerAccepted;
end;

procedure OnVisitWebsiteClick(Sender: TObject);
var
  ErrCode: Integer;
begin
  ShellExec('open', 'https://www.dualsensitive.com/', '', '', SW_SHOW, ewNoWait, ErrCode);
end;

procedure CurPageChanged(CurPageID: Integer);
var
  ThankYouLabel, WebsiteLabel: TNewStaticText;
  FS: TFontStyles;
begin
  if not WizardSilent and Assigned(WizardForm.NextButton) and Assigned(DisclaimerPage) and
     (CurPageID = DisclaimerPage.ID) then
    WizardForm.NextButton.Enabled := DisclaimerAccepted;

  if (CurPageID = wpFinished) and (not FinishExtrasCreated) then
  begin
    ThankYouLabel := TNewStaticText.Create(WizardForm);
    ThankYouLabel.Parent := WizardForm.FinishedPage;
    ThankYouLabel.Caption := #13#10 +
      'Thank you for installing the Dead Space — DualSensitive Mod!' + #13#10 +
      'For news and updates, please visit:';
    ThankYouLabel.Top := WizardForm.FinishedLabel.Top + WizardForm.FinishedLabel.Height + ScaleY(16);
    ThankYouLabel.Left := WizardForm.FinishedLabel.Left;
    ThankYouLabel.AutoSize := True;

    WebsiteLabel := TNewStaticText.Create(WizardForm);
    WebsiteLabel.Parent := WizardForm.FinishedPage;
    WebsiteLabel.Caption := 'https://www.dualsensitive.com/';
    WebsiteLabel.Font.Color := clBlue;
    FS := WebsiteLabel.Font.Style;
    Include(FS, fsUnderline);
    WebsiteLabel.Font.Style := FS;
    WebsiteLabel.Cursor := crHand;
    WebsiteLabel.OnClick := @OnVisitWebsiteClick;
    WebsiteLabel.Top := ThankYouLabel.Top + ThankYouLabel.Height + ScaleY(8);
    WebsiteLabel.Left := ThankYouLabel.Left;
    WebsiteLabel.AutoSize := True;

    FinishExtrasCreated := True;
  end;
end;

procedure BrowseManualPath(Sender: TObject);
var Dir: string;
begin
  Dir := ManualPathEdit.Text;
  if BrowseForFolder('Select game folder...', Dir, false) then
    ManualPathEdit.Text := Dir;
end;

procedure ManualCheckboxClick(Sender: TObject);
var
  Enabled: Boolean;
begin
  Enabled := ManualCheckbox.Checked;
  ManualPathEdit.Enabled := Enabled;
  ManualBrowseButton.Enabled := Enabled;
end;

function GetInstallPath(Default: string): string;
begin
if SelectedInstallPath <> '' then
    Result := SelectedInstallPath
else if Assigned(ManualPathEdit) and (ManualPathEdit.Text <> '') then
    Result := ManualPathEdit.Text
else
    Result := ExpandConstant('{autopf}\DualSensitive\Dead Space — DualSensitive Mod');
end;


function GetSchedTaskCommand(Param: string): string;
var
  vbsPath, exePath: string;
begin
  vbsPath := GetInstallPath('') + '\mods\launch-service.vbs';
  exePath := GetInstallPath('') + '\mods\dualsensitive-service.exe';

  if (vbsPath = '\mods\launch-service.vbs') or
     (exePath = '\mods\dualsensitive-service.exe') then
  begin
    Result := '/Create /TN "DualSensitive Service (invalid path skipped)" /TR "cmd.exe /c exit 0" /SC ONCE /ST 00:00 /RL HIGHEST /F';
    exit;
  end;

  Result :=
    '/Create /TN "DualSensitive Service" ' +
    '/TR "wscript.exe \"' + vbsPath + '\" \"' + exePath + '\"" ' +
    '/SC ONCE /ST 00:00 /RL HIGHEST /F';

  Log('Scheduled Task Command: ' + Result);
end;

function NormalizeLibraryPath(P: string): string;
var
  i: Integer;
begin
  if (Length(P) >= 2) and (P[1] = '"') and (P[Length(P)] = '"') then
    P := Copy(P, 2, Length(P) - 2);
  if (Length(P) > 0) and (P[Length(P)] = '"') then
    Delete(P, Length(P), 1);

  i := 1;
  while i <= Length(P) do
  begin
    if (P[i] = '\') and (i < Length(P)) and (P[i + 1] = '\') then
      Delete(P, i, 1)
    else
      Inc(i);
  end;

  for i := 1 to Length(P) do
    if P[i] = '/' then P[i] := '\';

  Result := Trim(P);
end;

function FindNextQuote(const S: string; StartAt: Integer): Integer;
var
  i, L: Integer;
begin
  L := Length(S);
  if (L = 0) or (StartAt < 1) or (StartAt > L) then
  begin
    Result := 0;
    Exit;
  end;

  for i := StartAt to L do
    if S[i] = '"' then
    begin
      Result := i;
      Exit;
    end;

  Result := 0;
end;

function ExtractJsonValue(Line: string): string;
var
  i: Integer;
begin
  Result := '';
  i := Pos(':', Line);
  if i > 0 then
  begin
    Result := Trim(Copy(Line, i + 1, MaxInt));
    if (Length(Result) > 0) and (Result[1] = '"') then
    begin
      Delete(Result, 1, 1);
      i := Pos('"', Result);
      if i > 0 then
        Result := Copy(Result, 1, i - 1);
    end;
    if (Length(Result) > 0) and (Result[Length(Result)] = ',') then
      Delete(Result, Length(Result), 1);
  end;
end;


function ExtractVdfPathValue(const Line: string): string;
var
  p, q1, q2: Integer;
  val: string;
begin
  Result := '';
  p := Pos('"path"', LowerCase(Line));
  if p = 0 then Exit;

  q1 := FindNextQuote(Line, p + 6);
  if q1 = 0 then Exit;
  q2 := FindNextQuote(Line, q1 + 1);
  if q2 = 0 then Exit;

  val := Copy(Line, q1 + 1, q2 - q1 - 1);
  Result := NormalizeLibraryPath(val);
end;


function CheckRoot(const steamappsRoot: string; var OutDir: string): Boolean;
var
  commonDir, gameDir: string;
begin
  Result := False;

  commonDir := AddBackslash(steamappsRoot) + 'common';
  gameDir   := AddBackslash(commonDir) + 'Dead Space';
  if DirExists(gameDir) then
  begin
    OutDir := gameDir;
    Result := True;
    Exit;
  end;

  gameDir := AddBackslash(commonDir) + 'Dead Space (2008)';
  if DirExists(gameDir) then
  begin
    OutDir := gameDir;
    Result := True;
    Exit;
  end;

  gameDir := AddBackslash(commonDir) + 'Dead Space (2023)';
  if DirExists(gameDir) then
  begin
    OutDir := gameDir;
    Result := True;
    Exit;
  end;

  if FileExists(AddBackslash(steamappsRoot) + 'appmanifest_1693980.acf') then
  begin
    if DirExists(AddBackslash(commonDir) + 'Dead Space (2023)') then
      OutDir := AddBackslash(commonDir) + 'Dead Space (2023)'
    else if DirExists(AddBackslash(commonDir) + 'Dead Space') then
      OutDir := AddBackslash(commonDir) + 'Dead Space'
    else
      OutDir := AddBackslash(commonDir) + 'Dead Space (2023)';
    Result := True;
    Exit;
  end;
end;

function ProbeSteamRoot(const steamappsRoot: string; var OutDir: string): Boolean;
begin
  Log('Steam: probing steamapps root ' + steamappsRoot);
  Result := DirExists(steamappsRoot) and CheckRoot(steamappsRoot, OutDir);
end;

function TryFindDeadSpaceByHeuristic(var OutDir: string): Boolean;
var
  d: Integer;
  root: string;
begin
  Result := False;
  OutDir := '';

  root := ExpandConstant('{pf32}') + '\Steam\steamapps';
  if ProbeSteamRoot(root, OutDir) then begin Result := True; Exit; end;

  root := ExpandConstant('{pf}') + '\Steam\steamapps';
  if ProbeSteamRoot(root, OutDir) then begin Result := True; Exit; end;

  for d := Ord('C') to Ord('Z') do
  begin
    root := Chr(d) + ':\Steam\steamapps';
    if ProbeSteamRoot(root, OutDir) then begin Result := True; Exit; end;

    root := Chr(d) + ':\SteamLibrary\steamapps';
    if ProbeSteamRoot(root, OutDir) then begin Result := True; Exit; end;

    root := Chr(d) + ':\Games\Steam\steamapps';
    if ProbeSteamRoot(root, OutDir) then begin Result := True; Exit; end;
  end;
end;

function FileExistsInSteam(): Boolean;
var
  SteamPath, VdfPath1, VdfPath2: string;
  Lines: TArrayOfString;
  i, j, n: Integer;
  LibRoots: array of string;
  Root, GameDir, GameExe: string;
  ExistsAlready: Boolean;
begin
  Result := False;
  SteamInstallPath := '';

  try
    if RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', SteamPath) then
    begin
      SetArrayLength(LibRoots, 1);
      LibRoots[0] := SteamPath;

      VdfPath1 := AddBackslash(SteamPath) + 'steamapps\libraryfolders.vdf';
      VdfPath2 := AddBackslash(SteamPath) + 'config\libraryfolders.vdf';

      if LoadStringsFromFile(VdfPath1, Lines) and (GetArrayLength(Lines) > 0) then
        for i := 0 to GetArrayLength(Lines) - 1 do
          try
              if Pos('"path"', Lines[i]) > 0 then
              begin
                Root := ExtractVdfPathValue(Lines[i]);
                if Root <> '' then
                begin
                  ExistsAlready := False;
                  n := GetArrayLength(LibRoots);
                  for j := 0 to n - 1 do
                    if CompareText(LibRoots[j], Root) = 0 then begin ExistsAlready := True; Break; end;
                  if not ExistsAlready then
                  begin
                    SetArrayLength(LibRoots, n + 1);
                    LibRoots[n] := Root;
                    Log('Steam: library from VDF = ' + Root);
                  end;
                end;
                end;
            except
                Log('Steam exception while parsing line: ' + lines[i]);
          end;

      if LoadStringsFromFile(VdfPath2, Lines) and (GetArrayLength(Lines) > 0) then
        for i := 0 to GetArrayLength(Lines) - 1 do
          try
              if Pos('"path"', Lines[i]) > 0 then
              begin
                Root := ExtractVdfPathValue(Lines[i]);
                if Root <> '' then
                begin
                  ExistsAlready := False;
                  n := GetArrayLength(LibRoots);
                  for j := 0 to n - 1 do
                    if CompareText(LibRoots[j], Root) = 0 then begin ExistsAlready := True; Break; end;
                  if not ExistsAlready then
                  begin
                    SetArrayLength(LibRoots, n + 1);
                    LibRoots[n] := Root;
                    Log('Steam: library from VDF = ' + Root);
                  end;
                end;
                end;
            except
                Log('Steam exception while parsing line: ' + lines[i]);
          end;

      for i := 0 to GetArrayLength(LibRoots) - 1 do
      begin
        Log('Steam: probing library "' + LibRoots[i] + '"');

        GameDir := AddBackslash(LibRoots[i]) + 'steamapps\common\Dead Space';
        GameExe := GameDir + '\Dead Space.exe';
        if FileExists(GameExe) or DirExists(GameDir) then
        begin
          SteamInstallPath := GameDir;
          Result := True;
          Log('Steam: found Dead Space at ' + SteamInstallPath);
          Exit;
        end;

        GameDir := AddBackslash(LibRoots[i]) + 'steamapps\common\Dead Space (2008)';
        GameExe := GameDir + '\Dead Space.exe';
        if FileExists(GameExe) or DirExists(GameDir) then
        begin
          SteamInstallPath := GameDir;
          Result := True;
          Log('Steam: found Dead Space at ' + SteamInstallPath);
          Exit;
        end;

        GameDir := AddBackslash(LibRoots[i]) + 'steamapps\common\Dead Space (2023)';
        GameExe := GameDir + '\Dead Space.exe';
        if FileExists(GameExe) or DirExists(GameDir) then
        begin
          SteamInstallPath := GameDir;
          Result := True;
          Log('Steam: found Dead Space at ' + SteamInstallPath);
          Exit;
        end;

        if FileExists(AddBackslash(LibRoots[i]) + 'steamapps\appmanifest_1693980.acf') then
        begin
          GameDir := AddBackslash(LibRoots[i]) + 'steamapps\common\Dead Space (2023)';
          if DirExists(GameDir) then
            SteamInstallPath := GameDir
          else if DirExists(AddBackslash(LibRoots[i]) + 'steamapps\common\Dead Space') then
            SteamInstallPath := AddBackslash(LibRoots[i]) + 'steamapps\common\Dead Space'
          else
            SteamInstallPath := AddBackslash(LibRoots[i]) + 'steamapps\common\Dead Space (2023)';
          Result := True;
          Log('Steam: found Dead Space via appmanifest at ' + SteamInstallPath);
          Exit;
        end;
      end;
    end
    else
      Log('Steam: SteamPath not found in registry.');
  except
    Log('Steam: EXCEPTION while parsing VDF; falling back to drive scan.');
  end;

  if not Result then
  begin
    Log('Steam: starting drive-scan fallback.');
    if TryFindDeadSpaceByHeuristic(SteamInstallPath) then
    begin
      Result := True;
      Log('Steam: heuristic found Dead Space at ' + SteamInstallPath);
    end
    else
    begin
      Log('Steam: heuristic did not find Dead Space.');
    end;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  SteamPath: string;
begin
  Result := True;

  if CurPageID = MyPage.ID then
  begin
    if not DisclaimerAccepted then
    begin
      MsgBox('You must accept the disclaimer to continue.', mbError, MB_OK);
      Result := False;
    end;

    if SteamCheckbox <> nil then
    begin
      if SteamCheckbox.Checked then
      begin
        if FileExistsInSteam() and (SteamInstallPath <> '') then
          SelectedInstallPath := SteamInstallPath
        else
          SelectedInstallPath := '';
        Log('Using Steam path: ' + SelectedInstallPath);
      end;
    end;

    if EACheckbox <> nil then
    begin
      if EACheckbox.Checked then
      begin
        SelectedInstallPath := EAInstallPath;
        Log('Using EA App path: ' + SelectedInstallPath);
      end;
    end;

    if ManualCheckbox.Checked then
    begin
      SelectedInstallPath := ManualPathEdit.Text;
      Log('Using manual path: ' + SelectedInstallPath);
    end;

    if not DirExists(SelectedInstallPath) then
    begin
      if not CreateDir(SelectedInstallPath) then
      begin
        MsgBox('Failed to create folder: ' + SelectedInstallPath, mbError, MB_OK);
        Result := False;
        exit;
       end;
    end;

    Log('SelectedInstallPath: ' + SelectedInstallPath);

  end;
end;

function StripQuotesAndKeyPrefix(S, Key: string): string;
var
  i: Integer;
begin
  Result := Trim(S);
  if Pos(Key, Result) > 0 then
    Delete(Result, 1, Length(Key));
  Result := Trim(Result);

  i := 1;
  while i <= Length(Result) do
  begin
    if Result[i] = '"' then
      Delete(Result, i, 1)
    else
      i := i + 1;
  end;
end;

var
  DeleteLogsCheckbox: TNewCheckBox;
  LogPaths: TStringList;

procedure CheckAndAddPath(BasePath: string);
var
  DualSensitiveDir: string;
begin
DualSensitiveDir := BasePath + '\mods\DualSensitive';
if FileExists(DualSensitiveDir + '\dualsensitive-service.log') or
   FileExists(DualSensitiveDir + '\dualsensitive-client.log') then
  LogPaths.Add(DualSensitiveDir);
end;

procedure DetectLogFiles();
var
  SteamPath: string;

begin
  if (SteamInstallPath <> '') then
    CheckAndAddPath(SteamInstallPath);

  if EAInstallPath <> '' then
    CheckAndAddPath(EAInstallPath);

  CheckAndAddPath(ExpandConstant('{app}'));
end;

function FileExistsInEA(): Boolean;
var
  FindRec: TFindRec;
  ManifestDir, FilePath, InstallLoc: string;
  Content: AnsiString;
  startedFind: Boolean;
begin
  Result := False;
  EAInstallPath := '';
  startedFind := False;

  try
    ManifestDir := ExpandConstant('{localappdata}') +
                   '\Electronic Arts\EA Desktop\Manifests';
    Log('Checking if the game is installed via EA App');

    if not DirExists(ManifestDir) then
      Exit;

    if FindFirst(ManifestDir + '\*.mfst', FindRec) then
    begin
      startedFind := True;
      repeat
        FilePath := ManifestDir + '\' + FindRec.Name;
        try
          if LoadStringFromFile(FilePath, Content) then
          begin
            if (Pos(UpperCase('"displayName": "Dead Space"'), UpperCase(Content)) > 0) and
               (Pos('"installLocation":', Content) > 0) then
            begin
              InstallLoc := Copy(Content, Pos('"installLocation":', Content) + 18, MaxInt);
              if Pos('"', InstallLoc) > 0 then
              begin
                InstallLoc := Copy(InstallLoc, Pos('"', InstallLoc) + 1, MaxInt);
                if Pos('"', InstallLoc) > 0 then
                  InstallLoc := Copy(InstallLoc, 1, Pos('"', InstallLoc) - 1);
              end;
              EAInstallPath := Trim(InstallLoc);
              Result := EAInstallPath <> '';
              if Result then
              begin
                Log('Found EA App path: ' + EAInstallPath);
                Exit;
              end;
            end;
          end;
        except
          Log('Warning: failed to read/parse "' + FilePath + '"; skipping.');
        end;
      until not FindNext(FindRec);
    end;
  except
    Log('Warning: exception in FileExistsInEA(); treating as not installed.');
    Result := False;
  end;

  if startedFind then
  begin
    try
      FindClose(FindRec);
    except
      { ignore }
    end;
  end;
end;

procedure CreateLogDeletePrompt();
var
  answer: Integer;
begin
  if LogPaths.Count = 0 then Exit;

  answer := MsgBox(
    'Log files from DualSensitive were found in one or more Dead Space installations.' + #13#10#13#10 +
    'Do you also want to delete these log folders (including Steam/EA App paths)?',
    mbConfirmation, MB_YESNO);

  if answer = IDYES then
  begin
    DeleteLogsCheckbox := TNewCheckBox.Create(nil);
    DeleteLogsCheckbox.Checked := True;
  end;
end;

procedure InitializeUninstallProgressForm();
begin
  LogPaths := TStringList.Create;
  DetectLogFiles();
  if LogPaths.Count > 0 then
  begin
    if MsgBox(
         'Log files from DualSensitive were found in one or more Dead Space installations.' + #13#10#13#10 +
         'Do you want to delete these log folders (including Steam/EA App paths)?',
         mbConfirmation, MB_YESNO) = IDYES
    then
    begin
      DeleteLogsCheckbox := TNewCheckBox.Create(nil);
      DeleteLogsCheckbox.Checked := True;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
  begin
    if (SteamCheckbox <> nil) and SteamCheckbox.Checked then
    begin
      if FileExistsInSteam() and (SteamInstallPath <> '') then
        SelectedInstallPath := SteamInstallPath;
      Log('CurStepChanged: Using Steam path: ' + SelectedInstallPath);
    end
    else if (EACheckbox <> nil) and EACheckbox.Checked then
    begin
      SelectedInstallPath := EAInstallPath;
      Log('CurStepChanged: Using EA App path: ' + SelectedInstallPath);
    end
    else if (ManualCheckbox <> nil) and ManualCheckbox.Checked then
    begin
      SelectedInstallPath := ManualPathEdit.Text;
      Log('CurStepChanged: Using manual path: ' + SelectedInstallPath);
    end;

    Log('CurStepChanged: Final SelectedInstallPath = ' + SelectedInstallPath);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  i: Integer;
  GamePath: string;
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Log('Uninstall: Deleting scheduled task');
    Exec('schtasks.exe', '/Delete /TN "DualSensitive Service" /F', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

    if RegQueryStringValue(HKCU, 'Software\DualSensitive\{#AppId}', 'GameInstallPath', GamePath) then
    begin
      Log('Uninstall: Found game path in registry: ' + GamePath);

      if FileExists(GamePath + '\{#ProxyDLL}') then
      begin
        DeleteFile(GamePath + '\{#ProxyDLL}');
        Log('Uninstall: Deleted ' + GamePath + '\{#ProxyDLL}');
      end;

      if FileExists(GamePath + '\mods\{#PluginDLL}') then
      begin
        DeleteFile(GamePath + '\mods\{#PluginDLL}');
        Log('Uninstall: Deleted ' + GamePath + '\mods\{#PluginDLL}');
      end;

      if FileExists(GamePath + '\mods\{#INIFile}') then
      begin
        DeleteFile(GamePath + '\mods\{#INIFile}');
        Log('Uninstall: Deleted ' + GamePath + '\mods\{#INIFile}');
      end;
      
      if FileExists(GamePath + '\mods\{#ModExeName}') then
      begin
        DeleteFile(GamePath + '\mods\{#ModExeName}');
        Log('Uninstall: Deleted ' + GamePath + '\mods\{#ModExeName}');
      end;
      
      if FileExists(GamePath + '\mods\{#VbsScript}') then
      begin
        DeleteFile(GamePath + '\mods\{#VbsScript}');
        Log('Uninstall: Deleted ' + GamePath + '\mods\{#VbsScript}');
      end;
      
      DelTree(GamePath + '\mods', True, True, False);
      RemoveDir(GamePath + '\mods');
      Log('Uninstall: Cleaned up mod directories');
    end
    else
      Log('Uninstall: Could not find game path in registry');
  end;
  
  if (CurUninstallStep = usPostUninstall) and
     (LogPaths <> nil) and
     (DeleteLogsCheckbox <> nil) and
     DeleteLogsCheckbox.Checked then
  begin
    for i := 0 to LogPaths.Count - 1 do
    begin
      Log('Deleting: ' + LogPaths[i]);
      DelTree(LogPaths[i], True, True, True);
    end;
  end;
end;


procedure InitializeWizard;
var
  InfoLabel1, InfoLabel2, InfoLabel3, InfoLabel4: TLabel;
  IsSteamInstalled, IsEAInstalled: Boolean;
  CurrentTop: Integer;
begin
  Log('IW: start');
  CreateDisclaimerPage();
  Log('IW: disclaimer page created');

  MyPage := CreateCustomPage(
    wpSelectDir,
    'Choose Game Versions',
    'Select which game versions to install the mod for.'
  );
  Log('IW: custom page created');

  Log('IW: steam detection done');
  try
    IsSteamInstalled := FileExistsInSteam();
  except
    Log('IW: EXCEPTION in FileExistsInSteam; treating as not installed (using manual).');
    IsSteamInstalled := False;
  end;

  try
    IsEAInstalled := FileExistsInEA();
  except
    Log('IW: EA detect raised, treating as not installed');
    IsEAInstalled := False;
  end;
  Log('IW: EA detection done');

  InfoLabel1 := TLabel.Create(WizardForm);
  InfoLabel1.Parent := MyPage.Surface;
  InfoLabel1.Top := ScaleY(0);
  InfoLabel1.Left := ScaleX(0);
  InfoLabel1.Font.Style := [fsBold];
  InfoLabel1.Caption := CustomMessage('InstallInfoLine1');

  InfoLabel2 := TLabel.Create(WizardForm);
  InfoLabel2.Parent := MyPage.Surface;
  InfoLabel2.Top := InfoLabel1.Top + ScaleY(20);
  InfoLabel2.Left := ScaleX(0);
  InfoLabel2.Caption := CustomMessage('InstallInfoLine2');

  InfoLabel3 := TLabel.Create(WizardForm);
  InfoLabel3.Parent := MyPage.Surface;
  InfoLabel3.Top := InfoLabel2.Top + ScaleY(20);
  InfoLabel3.Left := ScaleX(0);
  InfoLabel3.Caption := CustomMessage('InstallInfoLine3');

  InfoLabel4 := TLabel.Create(WizardForm);
  InfoLabel4.Parent := MyPage.Surface;
  InfoLabel4.Top := InfoLabel3.Top + ScaleY(30);
  InfoLabel4.Left := ScaleX(0);
  InfoLabel4.Caption := CustomMessage('InstallInfoLine4');

  CurrentTop := InfoLabel4.Top + ScaleY(24);

  try
    Log('IW: creating Manual checkbox');
    ManualCheckbox := TCheckBox.Create(WizardForm);
    ManualCheckbox.Parent := MyPage.Surface;
    ManualCheckbox.Top := CurrentTop;
    ManualCheckbox.Left := ScaleX(0);
    ManualCheckbox.Width := ScaleX(300);
    ManualCheckbox.Height := ScaleY(20);
    ManualCheckbox.Caption := 'Install to custom path:';
    ManualCheckbox.OnClick := @ManualCheckboxClick;
    ManualCheckbox.Checked := False;
    CurrentTop := CurrentTop + ScaleY(24);

    Log('IW: creating Manual path edit + Browse');
    ManualPathEdit := TEdit.Create(WizardForm);
    ManualPathEdit.Parent := MyPage.Surface;
    ManualPathEdit.Top := CurrentTop;
    ManualPathEdit.Left := ScaleX(0);
    ManualPathEdit.Width := ScaleX(300);
    ManualPathEdit.Height := ScaleY(25);
    ManualPathEdit.Text := 'C:\Games\Dead Space';

    ManualBrowseButton := TButton.Create(WizardForm);
    ManualBrowseButton.Parent := MyPage.Surface;
    ManualBrowseButton.Top := CurrentTop;
    ManualBrowseButton.Left := ManualPathEdit.Left + ManualPathEdit.Width + ScaleX(8);
    ManualBrowseButton.Width := ScaleX(75);
    ManualBrowseButton.Height := ScaleY(25);
    ManualBrowseButton.Caption := 'Browse...';
    ManualBrowseButton.OnClick := @BrowseManualPath;

    Log('IW: manual controls created');
  except
    Log('IW: ERROR creating manual controls; minimalizing.');
    if ManualCheckbox = nil then
    begin
      ManualCheckbox := TCheckBox.Create(WizardForm);
      ManualCheckbox.Parent := MyPage.Surface;
      ManualCheckbox.Top := CurrentTop;
      ManualCheckbox.Left := ScaleX(0);
      ManualCheckbox.Caption := 'Install to custom path:';
      ManualCheckbox.Checked := True;
    end;
  end;

  if IsSteamInstalled then
  begin
    try
      Log('IW: creating Steam checkbox (path=' + SteamInstallPath + ')');
      SteamCheckbox := TCheckBox.Create(WizardForm);
      SteamCheckbox.Parent := MyPage.Surface;
      SteamCheckbox.Top := InfoLabel4.Top + ScaleY(24);
      if Assigned(ManualBrowseButton) then
        CurrentTop := ManualBrowseButton.Top + ManualBrowseButton.Height + ScaleY(8)
      else if Assigned(ManualCheckbox) then
        CurrentTop := ManualCheckbox.Top + ScaleY(28)
      else
        CurrentTop := InfoLabel4.Top + ScaleY(24);
      SteamCheckbox.Top := CurrentTop;
      SteamCheckbox.Left := ScaleX(0);
      SteamCheckbox.Width := ScaleX(300);
      SteamCheckbox.Height := ScaleY(20);
      SteamCheckbox.Caption := 'Install for Steam';
      SteamCheckbox.Checked := True;

      if Assigned(ManualCheckbox) then
        ManualCheckbox.Checked := False;

      Log('IW: steam checkbox created');
    except
      Log('IW: WARNING Steam checkbox creation failed; continuing without Steam option.');
    end;
  end;

  if IsEAInstalled and (EAInstallPath <> '') then
  begin
    try
      if Assigned(SteamCheckbox) then
        CurrentTop := SteamCheckbox.Top + SteamCheckbox.Height + ScaleY(8)
      else if Assigned(ManualBrowseButton) then
        CurrentTop := ManualBrowseButton.Top + ManualBrowseButton.Height + ScaleY(8)
      else if Assigned(ManualCheckbox) then
        CurrentTop := ManualCheckbox.Top + ScaleY(28)
      else
        CurrentTop := InfoLabel4.Top + ScaleY(24);

      Log('IW: creating EA checkbox (path=' + EAInstallPath + ')');
      EACheckbox := TCheckBox.Create(WizardForm);
      EACheckbox.Parent := MyPage.Surface;
      EACheckbox.Top := CurrentTop;
      EACheckbox.Left := ScaleX(0);
      EACheckbox.Width := ScaleX(300);
      EACheckbox.Height := ScaleY(20);
      EACheckbox.Caption := 'Install for EA App';
      EACheckbox.Checked := not IsSteamInstalled;

      if EACheckbox.Checked and Assigned(ManualCheckbox) then
        ManualCheckbox.Checked := False;

      CurrentTop := EACheckbox.Top + EACheckbox.Height + ScaleY(8);

      Log('IW: EA checkbox created');
    except
      Log('IW: WARNING EA checkbox creation failed; continuing without EA option.');
    end;
  end;

  if not IsSteamInstalled and not IsEAInstalled then
  begin
    Log('IW: steam NOT installed/detected; leaving Manual default');
    if Assigned(ManualCheckbox) then
      ManualCheckbox.Checked := True;
  end;


  ManualCheckboxClick(nil);
  Log('IW: checkboxes wired');
end;


