param([Parameter(Mandatory)][string]$Helper,[Parameter(Mandatory)][string]$Track)
$ErrorActionPreference='Stop'
$prefix='Local\sattrak.SetupMusic.Test.'+[guid]::NewGuid().ToString('N')
$events=@{}
$audioProcess=$null
try {
    foreach($eventName in @('stop','mute','ready','muted')) {
        $events[$eventName]=[System.Threading.EventWaitHandle]::new($false,[System.Threading.EventResetMode]::ManualReset,$prefix+'.'+$eventName)
    }
    $audioProcess=Start-Process -FilePath $Helper -ArgumentList ('"'+$Track+'"'),('"'+$prefix+'"'),$PID -WindowStyle Hidden -PassThru
    if(!$events.ready.WaitOne(5000)){throw 'Audio helper did not become ready'}
    $events.mute.Set() | Out-Null
    if(!$events.muted.WaitOne(2000)){throw 'Mute was not applied to the playing sound'}
    $events.mute.Reset() | Out-Null
    $audioDeadline=[DateTime]::UtcNow.AddSeconds(2)
    while($events.muted.WaitOne(0) -and [DateTime]::UtcNow -lt $audioDeadline){Start-Sleep -Milliseconds 20}
    if($events.muted.WaitOne(0)){throw 'Unmute was not applied'}
    $events.stop.Set() | Out-Null
    if(!$audioProcess.WaitForExit(5000)){throw 'Audio helper did not shut down'}
    if($audioProcess.ExitCode -ne 0){throw "Audio helper failed: $($audioProcess.ExitCode)"}
    Write-Output 'Actual MP3 playback initialization, mute/unmute and clean shutdown passed.'
    $events.stop.Reset() | Out-Null
    $events.ready.Reset() | Out-Null
    $badTrack=Join-Path ([IO.Path]::GetDirectoryName($Track)) 'not-an-installed-recording.mp3'
    $audioProcess=Start-Process -FilePath $Helper -ArgumentList ('"'+$badTrack+'"'),('"'+$prefix+'"'),$PID -WindowStyle Hidden -PassThru
    if(!$audioProcess.WaitForExit(5000)){throw 'Missing-track failure did not terminate'}
    if($audioProcess.ExitCode -ne 3 -or $events.ready.WaitOne(0)){throw 'Missing track was not rejected cleanly'}
    Write-Output 'Missing-track failure passed without falsely reporting playback ready.'
} finally {
    if($events.ContainsKey('stop')){$events.stop.Set() | Out-Null}
    if($audioProcess -and !$audioProcess.HasExited){$audioProcess.WaitForExit(5000) | Out-Null}
    foreach($eventHandle in $events.Values){$eventHandle.Dispose()}
}
