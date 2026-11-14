param([switch]$Install, [string]$Phone = $env:LUMIA_IP, [string]$Platform = 'ARM', [string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$app = $PSScriptRoot
$pfx = Join-Path $app 'Lumibooks.pfx'
if (-not (Test-Path $pfx)) {
    $c = New-SelfSignedCertificate -Type Custom -Subject 'CN=lumiaw10m' -KeyUsage DigitalSignature `
        -CertStoreLocation Cert:\CurrentUser\My -KeyExportPolicy Exportable -HashAlgorithm SHA256 `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
    [IO.File]::WriteAllBytes($pfx, $c.Export('Pfx', ''))
    [IO.File]::WriteAllBytes((Join-Path $app 'Lumibooks.cer'), $c.Export('Cert'))
    "created signing certificate"
}
$mf = Join-Path $app 'Package.appxmanifest'
$m = [IO.File]::ReadAllText($mf)
$m = [regex]::Replace($m, '(<Identity [^>]*Version=")(\d+)\.(\d+)\.(\d+)\.0"', { param($x) $x.Groups[1].Value + $x.Groups[2].Value + '.' + $x.Groups[3].Value + '.' + ([int]$x.Groups[4].Value + 1) + '.0"' })
[IO.File]::WriteAllText($mf, $m)
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path $msbuild)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = & $vswhere -version '[16,17)' -latest -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
}
$root = Split-Path $app
$vcPlatform = if ($Platform -eq 'x86') { 'Win32' } else { $Platform }
$bat = @"
@echo off
subst B: /d >nul 2>&1
subst B: "$root" || exit /b 1
cd /d B:\Lumibooks
"$msbuild" Lumibooks.vcxproj /nologo /v:m /p:Configuration=$Configuration /p:Platform=$vcPlatform /p:AppxPackageDir=B:\Lumibooks\out\ /p:UapAppxPackageBuildMode=SideloadOnly /p:AppxBundle=Never
set RC=%ERRORLEVEL%
cd /d "$env:TEMP"
subst B: /d
exit /b %RC%
"@
$f = Join-Path $env:TEMP 'lumibooks-build.bat'
Set-Content -Path $f -Value $bat -Encoding ascii
cmd /c $f
$rc = $LASTEXITCODE
Remove-Item $f
if ($rc) { "BUILD FAILED $rc"; exit $rc }
$dir = Get-ChildItem "$app\out" -Directory | Sort-Object LastWriteTime | Select-Object -Last 1
$appx = Get-ChildItem $dir.FullName -Filter *.appx | Select-Object -First 1
"built: $($appx.FullName) ($([math]::Round($appx.Length / 1KB)) KB)"
if (-not $Install) { return }
if (-not $Phone) { "set LUMIA_IP or pass -Phone"; exit 1 }
Add-Type -AssemblyName System.Net.Http
$h = New-Object Net.Http.HttpClient
$h.Timeout = [TimeSpan]::FromMinutes(5)
$mp = New-Object Net.Http.MultipartFormDataContent
$files = @($appx) + @(Get-ChildItem "$($dir.FullName)\Dependencies\$Platform" -Filter *.appx -ErrorAction SilentlyContinue) + @(Get-ChildItem $app -Filter *.cer)
foreach ($x in $files) {
    $bc = New-Object Net.Http.ByteArrayContent(, [IO.File]::ReadAllBytes($x.FullName))
    $bc.Headers.TryAddWithoutValidation('Content-Disposition', "form-data; name=`"$($x.Name)`"; filename=`"$($x.Name)`"") | Out-Null
    $mp.Add($bc)
}
$r = $h.PostAsync("http://$Phone/api/app/packagemanager/package?package=$($appx.Name)", $mp).Result
"upload: $([int]$r.StatusCode)"
for ($i = 0; $i -lt 90; $i++) {
    try { $s = $h.GetAsync("http://$Phone/api/app/packagemanager/state").Result } catch { Start-Sleep -Seconds 2; continue }
    if ($s -and [int]$s.StatusCode -ne 204) { "install state $([int]$s.StatusCode): $($s.Content.ReadAsStringAsync().Result)"; break }
    Start-Sleep -Seconds 2
}
