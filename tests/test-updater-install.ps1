param(
    [Parameter(Mandatory=$true)][string]$Helper,
    [Parameter(Mandatory=$true)][string]$Iscc,
    [Parameter(Mandatory=$true)][string]$WorkFolder
)
$ErrorActionPreference='Stop'
$key='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\sattrak-update-fixture_is1'
if(Test-Path $key){throw 'An updater fixture is already registered'}
$root=Join-Path ([IO.Path]::GetFullPath($WorkFolder)) ([guid]::NewGuid().ToString('N'))
$target=Join-Path $root 'installation with spaces'
$source=Join-Path $root 'source'
$cache=Join-Path $env:LOCALAPPDATA 'sattrak-update-fixture/updates'
New-Item -ItemType Directory -Force $source,$cache|Out-Null
$script=Join-Path $root 'fixture.iss'
@"
[Setup]
AppId=sattrak-update-fixture
AppName=sattrak updater test fixture
AppVersion=1.0.0
DefaultDirName=$target
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=$root
OutputBaseFilename=fixture
Uninstallable=yes
CloseApplications=no
RestartApplications=no
[Files]
Source: "$source\release-version.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "$source\payload.txt"; DestDir: "{app}"; Flags: ignoreversion
"@ | Set-Content -LiteralPath $script
function Quote([string]$value){return '"'+$value+'"'}
function Run([string]$program,[string]$arguments){
    $process=Start-Process -FilePath $program -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $process.WaitForExit()
    return $process.ExitCode
}
function Build([string]$version){
    Set-Content (Join-Path $source 'release-version.txt') $version
    Set-Content (Join-Path $source 'payload.txt') $version
    & $Iscc /Qp $script | Out-Null
    if($LASTEXITCODE -ne 0){throw 'Fixture compile failed'}
}
try {
    Build '1.0.0'
    if((Run (Join-Path $root 'fixture.exe') ('/VERYSILENT /SUPPRESSMSGBOXES /SP- /NORESTART /DIR='+ (Quote $target))) -ne 0){throw 'Fixture install failed'}
    $personal=Join-Path $target 'local/recordings'
    New-Item -ItemType Directory -Force $personal|Out-Null
    Set-Content (Join-Path $personal 'keep.csv') 'personal recording'
    Build '1.1.0'
    $package=Join-Path $cache ((Split-Path $root -Leaf)+'.exe')
    Copy-Item (Join-Path $root 'fixture.exe') $package
    $hash=(Get-FileHash $package -Algorithm SHA256).Hash.ToLowerInvariant()
    $bytes=(Get-Item $package).Length
    $log=Join-Path $root 'update-install.log'
    $arguments='0 '+(Quote $package)+' '+(Quote $target)+' '+('0'*64)+' '+$bytes+' '+(Quote $log)+' v1.1.0'
    if((Run $Helper $arguments) -ne 6){throw 'Bad checksum was not rejected'}
    if((Get-Content (Join-Path $target 'payload.txt') -Raw).Trim() -ne '1.0.0'){throw 'Bad package changed the installed app'}
    $parent=Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList '-NoProfile -Command Start-Sleep -Seconds 3' -WindowStyle Hidden -PassThru
    $arguments=$parent.Id.ToString()+' '+(Quote $package)+' '+(Quote $target)+' '+$hash+' '+$bytes+' '+(Quote $log)+' v1.1.0'
    $updater=Start-Process -FilePath $Helper -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Start-Sleep -Milliseconds 500
    if($updater.HasExited -or (Get-Content (Join-Path $target 'payload.txt') -Raw).Trim() -ne '1.0.0'){throw 'Updater did not wait for its parent'}
    $parent.WaitForExit();$updater.WaitForExit()
    if($updater.ExitCode -ne 0){throw "Update failed with exit code $($updater.ExitCode)"}
    if((Get-Content (Join-Path $target 'payload.txt') -Raw).Trim() -ne '1.1.0'){throw 'Updated payload is missing'}
    if((Get-Content (Join-Path $personal 'keep.csv') -Raw).Trim() -ne 'personal recording'){throw 'Personal recording was modified'}
    Set-Content (Join-Path $target 'payload.txt') 'keep after same-version retry'
    Copy-Item (Join-Path $root 'fixture.exe') $package -Force
    $arguments='0 '+(Quote $package)+' '+(Quote $target)+' '+$hash+' '+$bytes+' '+(Quote $log)+' v1.1.0'
    if((Run $Helper $arguments) -ne 7){throw 'Same-version update was not skipped'}
    if((Get-Content (Join-Path $target 'payload.txt') -Raw).Trim() -ne 'keep after same-version retry'){throw 'Same-version retry replaced files'}
    Write-Output 'Checksum rejection, parent wait, silent installation, personal data preservation and repeat-update protection passed'
} finally {
    $uninstaller=Join-Path $target 'unins000.exe'
    if(Test-Path $uninstaller){$null=Run $uninstaller '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART'}
}
