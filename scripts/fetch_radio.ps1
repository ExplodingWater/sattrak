$ErrorActionPreference = 'Stop'
$destination = Join-Path $PSScriptRoot '../assets/radio'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$clips = @(
    @{Author='Trebblofang'; Id=241466; Name='01 - 7058 kHz LSB - English conversation'},
    @{Author='kb7clx'; Id=396869; Name='02 - 50 MHz USB - European contacts'},
    @{Author='kb7clx'; Id=397389; Name='03 - 14260 kHz USB - American Field Day'},
    @{Author='kb7clx'; Id=394314; Name='04 - 7128 kHz SSB - Transatlantic contacts'},
    @{Author='kb7clx'; Id=243529; Name='06 - 7005 kHz - AM received Morse pileup'}
)
foreach ($clip in $clips) {
    if (Test-Path -LiteralPath (Join-Path $destination ($clip.Name+'.mp3'))) { continue }
    $page = Invoke-WebRequest -Uri "https://freesound.org/people/$($clip.Author)/sounds/$($clip.Id)/" -UseBasicParsing
    $audio = [regex]::Match($page.Content, 'https://cdn\.freesound\.org/previews/[^"<>\s]+-hq\.mp3').Value
    if (!$audio) { throw "No public MP3 rendition for $($clip.Id)" }
    Invoke-WebRequest -Uri $audio -OutFile (Join-Path $destination ($clip.Name+'.mp3'))
    Write-Output "$($clip.Name): saved public HQ MP3 rendition"
}
$commons = 'https://commons.wikimedia.org/wiki/File:Recording_2021-07-09_1032-UTC_10489701-kHz.wav'
$page = Invoke-WebRequest -Uri $commons -UseBasicParsing
$audio = [regex]::Match($page.Content, 'https://upload\.wikimedia\.org/wikipedia/commons/[a-f0-9]/[a-f0-9]{2}/Recording_2021-07-09_1032-UTC_10489701-kHz\.wav').Value
if (!$audio) { throw 'No original QO-100 WAV link found' }
Invoke-WebRequest -Uri $audio -OutFile (Join-Path $destination '05 - QO-100 - Amateur satellite conversation.wav')
Write-Output 'QO-100: saved original WAV'
