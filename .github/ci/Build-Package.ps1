param([string]$Version='', [string]$Commit=$env:GITHUB_SHA,
    [string]$OutputDirectory='', [string]$WorkDirectory='')
. (Join-Path $PSScriptRoot 'Common.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$baseVersion=Get-SourceVersion $root
if (-not $Version) { $Version=$baseVersion }
Assert-Version $Version
if ($Commit -cnotmatch '^[0-9a-f]{40}\z') { throw 'An exact build commit SHA is required.' }
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $root 'dist/ci' }
if (-not $WorkDirectory) { $WorkDirectory=Join-Path $root 'obj/ci' }
$work=[IO.Path]::GetFullPath($WorkDirectory)
$output=[IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $work) { throw 'CI work directory already exists; refusing to reuse stale outputs.' }
if (Test-Path -LiteralPath $output) { throw 'Artifact directory already exists; refusing to overwrite it.' }
$before=@{}
foreach ($file in @('config.ini','src/Version.h','src/MainUICustomizer.rc')) { $before[$file]=(Get-FileHash -LiteralPath (Join-Path $root $file)).Hash }
$config=Get-PackageConfig $root $Version
New-Item -ItemType Directory -Path $work | Out-Null
$resource=Join-Path $work 'version'
New-Item -ItemType Directory -Path $resource | Out-Null
[IO.File]::WriteAllText((Join-Path $resource 'Version.h'),@"
#pragma once
#define MAIN_UI_CUSTOMIZER_VERSION "$Version"
#define MAIN_UI_CUSTOMIZER_FILE_VERSION $($Version.Replace('.',',')),0
#define MAIN_UI_CUSTOMIZER_FILE_VERSION_STRING "$Version.0"
"@+"`r`n",[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $root 'src/MainUICustomizer.rc') -Destination (Join-Path $resource 'MainUICustomizer.rc')
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild=@(& $vswhere -latest -products '*' -version '[18.0,19.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe') | Select-Object -First 1
if ($LASTEXITCODE -ne 0 -or -not $msbuild) { throw 'Visual Studio 2026 MSBuild with C++ tools is required.' }
$build=Join-Path $work 'build'
& $msbuild (Join-Path $root 'MainUICustomizer.vcxproj') /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 "/p:OutDir=$build\" "/p:IntDir=$work\intermediate\" "/p:VersionResourceFile=$resource\MainUICustomizer.rc"
if ($LASTEXITCODE -ne 0) { throw 'Release x64 v145 build failed.' }
$dll=Join-Path $build 'MainUICustomizer.dll'
Assert-PeDll $dll $Version
Assert-LoadSafe $root
$originalBytes=(Get-Item -LiteralPath $dll).Length
$originalHash=(Get-FileHash -LiteralPath $dll).Hash.ToLowerInvariant()
$policy=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'policy.json') -Raw | ConvertFrom-Json
$archive=Join-Path $work 'upx.zip'
& curl.exe --silent --show-error --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 20 --max-time 120 --output $archive --url $policy.upxUrl
if ($LASTEXITCODE -ne 0) { throw 'Official UPX download failed.' }
if ((Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant() -cne $policy.upxSHA256) { throw 'Official UPX archive SHA256 mismatch.' }
Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $work 'upx')
$upx=Join-Path $work "upx/upx-$($policy.upxVersion)-win64/upx.exe"
& $upx --best --ultra-brute $dll
if ($LASTEXITCODE -ne 0) { throw 'UPX compression failed.' }
& $upx --test $dll
if ($LASTEXITCODE -ne 0) { throw 'UPX integrity check failed.' }
$packedBytes=(Get-Item -LiteralPath $dll).Length
if ($packedBytes -ge $originalBytes) { throw 'Packed DLL must be smaller than the original.' }
Assert-PeDll $dll $Version
$packedHash=(Get-FileHash -LiteralPath $dll).Hash.ToLowerInvariant()
$unpacked=Join-Path $work 'unpacked.dll'
& $upx --decompress -o $unpacked $dll
if ($LASTEXITCODE -ne 0) { throw 'UPX decompression failed.' }
Assert-PeDll $unpacked $Version
# UPX can legitimately rebuild PE headers: no original/unpacked hash equality requirement.
foreach ($probeDll in @($dll,$unpacked)) {
    & ([Environment]::ProcessPath) -NoProfile -File (Join-Path $PSScriptRoot 'Load-Probe.ps1') -Root $root -Dll $probeDll
    if ($LASTEXITCODE -ne 0) { throw 'Non-game DLL load probe failed.' }
}
$stage=Join-Path $work 'package'
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $stage 'MainUICustomizer.dll')
[IO.File]::WriteAllBytes((Join-Path $stage 'config.ini'),$config)
New-Item -ItemType Directory -Path $output | Out-Null
$zipName="MainUICustomizer-$Version.zip"
$zip=Join-Path $output $zipName
Compress-Archive -LiteralPath @((Join-Path $stage 'MainUICustomizer.dll'),(Join-Path $stage 'config.ini')) -DestinationPath $zip
$zipHash=(Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $output "$zipName.sha256"),"$zipHash  $zipName`n")
[ordered]@{plugin='MainUICustomizer';version=$Version;baseVersion=$baseVersion;commit=$Commit;upxVersion=$policy.upxVersion;upxArchiveSHA256=$policy.upxSHA256;
    originalBytes=$originalBytes;packedBytes=$packedBytes;reductionPercent=[math]::Round(100*(1-$packedBytes/$originalBytes),2);
    originalSHA256=$originalHash;packedSHA256=$packedHash;unpackedSHA256=(Get-FileHash -LiteralPath $unpacked).Hash.ToLowerInvariant();
    zipSHA256=$zipHash;iniSHA256=(Get-BytesHash $config);integrityPassed=$true;decompressionPassed=$true;loadPassed=$true} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'compression-report.json') -Encoding utf8NoBOM
[void](Assert-Package $root $output $Version $Commit $zipHash)
foreach ($file in $before.Keys) {
    if ((Get-FileHash -LiteralPath (Join-Path $root $file)).Hash -cne $before[$file]) { throw "Source changed unexpectedly: $file" }
}
if ($env:GITHUB_OUTPUT) { "version=$Version`nzip_sha256=$zipHash" | Add-Content -LiteralPath $env:GITHUB_OUTPUT }
Write-Output "Validated $zipName ($originalBytes -> $packedBytes bytes)."
