; Use sattrak's own miniaudio decoder through a short-lived, static-runtime helper.
[Code]
var
  MusicMuteButton: TNewButton;
  MusicStopEvent, MusicMuteEvent, MusicReadyEvent, MusicMutedEvent, MusicProcessId: LongWord;
  MusicMuted: Boolean;

function CreateMusicEvent(Attributes, ManualReset, InitialState: LongWord; Name: String): LongWord;
  external 'CreateEventW@kernel32.dll stdcall';
function SetMusicEvent(Event: LongWord): Boolean;
  external 'SetEvent@kernel32.dll stdcall';
function ResetMusicEvent(Event: LongWord): Boolean;
  external 'ResetEvent@kernel32.dll stdcall';
function WaitMusicObject(Handle, Milliseconds: LongWord): LongWord;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function CloseMusicHandle(Handle: LongWord): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function OpenMusicProcess(Access, InheritHandle, ProcessId: LongWord): LongWord;
  external 'OpenProcess@kernel32.dll stdcall';
function SetupProcessId: LongWord;
  external 'GetCurrentProcessId@kernel32.dll stdcall';

procedure ReleaseMusicHandle(var Handle: LongWord);
begin
  if Handle <> 0 then CloseMusicHandle(Handle);
  Handle := 0;
end;

procedure StopSetupMusic;
var
  ProcessHandle: LongWord;
begin
  if MusicStopEvent <> 0 then SetMusicEvent(MusicStopEvent);
  if MusicProcessId <> 0 then
  begin
    ProcessHandle := OpenMusicProcess($00100000, 0, MusicProcessId);
    if ProcessHandle <> 0 then
    begin
      WaitMusicObject(ProcessHandle, 2000);
      CloseMusicHandle(ProcessHandle);
    end;
    MusicProcessId := 0;
    Log('Installer music stopped.');
  end;
  ReleaseMusicHandle(MusicStopEvent);
  ReleaseMusicHandle(MusicMuteEvent);
  ReleaseMusicHandle(MusicReadyEvent);
  ReleaseMusicHandle(MusicMutedEvent);
end;

procedure DisableSetupMusic;
begin
  StopSetupMusic;
  MusicMuteButton.Enabled := False;
  MusicMuteButton.Caption := 'Music unavailable';
  MusicMuteButton.Hint := 'Installation can continue without audio';
end;

procedure MusicMuteClick(Sender: TObject);
begin
  if MusicProcessId = 0 then Exit;
  if MusicMuted then
  begin
    if not ResetMusicEvent(MusicMuteEvent) then begin DisableSetupMusic; Exit; end;
    MusicMuteButton.Caption := 'Mute';
    Log('Installer music unmuted (25%).');
  end else begin
    if not SetMusicEvent(MusicMuteEvent) then begin DisableSetupMusic; Exit; end;
    MusicMuteButton.Caption := 'Unmute';
    Log('Installer music muted.');
  end;
  MusicMuted := not MusicMuted;
end;

procedure InitializeWizard;
var
  EventPrefix, Parameters: String;
  ProcessId: Integer;
begin
  if WizardSilent then begin Log('Installer music skipped for silent setup.'); Exit; end;
  MusicMuteButton := TNewButton.Create(WizardForm);
  MusicMuteButton.Parent := WizardForm;
  MusicMuteButton.Left := ScaleX(8);
  MusicMuteButton.Top := WizardForm.NextButton.Top;
  MusicMuteButton.Width := ScaleX(115);
  MusicMuteButton.Height := WizardForm.NextButton.Height;
  MusicMuteButton.Anchors := [akLeft, akBottom];
  MusicMuteButton.Caption := 'Mute';
  MusicMuteButton.Hint := '2003 Toyota Corolla - Toyota Corolla';
  MusicMuteButton.ShowHint := True;
  MusicMuteButton.OnClick := @MusicMuteClick;
  try
    EventPrefix := 'Local\sattrak.SetupMusic.' + ExtractFileName(ExpandConstant('{tmp}'));
    MusicStopEvent := CreateMusicEvent(0, 1, 0, EventPrefix + '.stop');
    MusicMuteEvent := CreateMusicEvent(0, 1, 0, EventPrefix + '.mute');
    MusicReadyEvent := CreateMusicEvent(0, 1, 0, EventPrefix + '.ready');
    MusicMutedEvent := CreateMusicEvent(0, 1, 0, EventPrefix + '.muted');
    if (MusicStopEvent = 0) or (MusicMuteEvent = 0) or (MusicReadyEvent = 0) or (MusicMutedEvent = 0) then
    begin DisableSetupMusic; Exit; end;
    ExtractTemporaryFile('sattrak-installer-audio.exe');
    ExtractTemporaryFile('installer-music.mp3');
    Parameters := '"' + ExpandConstant('{tmp}\installer-music.mp3') + '" "' + EventPrefix + '" ' + IntToStr(SetupProcessId);
    if not Exec(ExpandConstant('{tmp}\sattrak-installer-audio.exe'), Parameters,
      ExpandConstant('{tmp}'), SW_HIDE, ewNoWait, ProcessId) then
    begin DisableSetupMusic; Exit; end;
    MusicProcessId := ProcessId;
    if WaitMusicObject(MusicReadyEvent, 5000) <> 0 then
    begin Log('Installer audio could not start; continuing without music.'); DisableSetupMusic; Exit; end;
    Log('Installer music started: 2003 Toyota Corolla - Toyota Corolla, looping at 25% (miniaudio).');
  except
    Log('Installer music unavailable: ' + GetExceptionMessage);
    DisableSetupMusic;
  end;
end;

procedure DeinitializeSetup;
begin
  StopSetupMusic;
end;
