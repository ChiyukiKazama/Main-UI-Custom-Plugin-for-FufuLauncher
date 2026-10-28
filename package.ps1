param()
$ErrorActionPreference='Stop'
$pluginRoot=(Resolve-Path -LiteralPath $PSScriptRoot).Path
$dll=Join-Path $pluginRoot 'bin\x64\Release\MainUICustomizer.dll'
$config=Join-Path $pluginRoot 'config.ini'
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { throw 'Build Release/x64 first.' }
$version=(Get-Item -LiteralPath $dll).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'DLL product version is missing or invalid; rebuild first.' }
$configText=[IO.File]::ReadAllText($config)
$general=[regex]::Match($configText,'(?ms)^\[General\][ \t]*\r?\n(?<body>.*?)(?=^\[|\z)')
$configVersion=[regex]::Match($general.Groups['body'].Value,'(?m)^Version[ \t]*=[ \t]*(?<version>\S+)[ \t]*\r?$').Groups['version'].Value
if ($configVersion -cne $version) { throw "Configuration version '$configVersion' differs from DLL version '$version'. Update it explicitly before packaging." }
$sourceHashes=@{}
foreach ($source in @($dll,$config)) { $sourceHashes[[IO.Path]::GetFileName($source)]=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash }
$dist=Join-Path $pluginRoot 'dist'
if (-not (Test-Path -LiteralPath $dist)) { New-Item -ItemType Directory -Path $dist | Out-Null }
$zip=Join-Path $dist "MainUICustomizer-$version.zip"
$stagingZip=Join-Path $dist "MainUICustomizer-$version.building.zip"
if (Test-Path -LiteralPath $zip) { throw "Package already exists: $zip. Existing releases are preserved." }
if (Test-Path -LiteralPath $stagingZip) { throw "Staging package already exists: $stagingZip. Inspect it before retrying." }
try {
    Compress-Archive -LiteralPath @($dll,$config) -DestinationPath $stagingZip
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead($stagingZip)
    try {
        $entries=@($archive.Entries | ForEach-Object FullName)
        if ($entries.Count -ne 2 -or -not ($entries -ccontains 'MainUICustomizer.dll') -or -not ($entries -ccontains 'config.ini')) {
            throw 'Package root must contain exactly MainUICustomizer.dll and config.ini.'
        }
        foreach ($name in $entries) {
            $stream=$archive.GetEntry($name).Open()
            try {
                $hasher=[Security.Cryptography.SHA256]::Create()
                try { $entryHash=[Convert]::ToHexString($hasher.ComputeHash($stream)) } finally { $hasher.Dispose() }
            } finally { $stream.Dispose() }
            if ($entryHash -cne $sourceHashes[$name]) { throw "Packaged file hash mismatch: $name" }
        }
    } finally { $archive.Dispose() }
    foreach ($source in @($dll,$config)) {
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $sourceHashes[[IO.Path]::GetFileName($source)]) {
            throw "Source changed during packaging: $source. No release was published."
        }
    }
    Move-Item -LiteralPath $stagingZip -Destination $zip
} finally {
    if (Test-Path -LiteralPath $stagingZip) { Remove-Item -LiteralPath $stagingZip }
}
Get-Item -LiteralPath $zip | Select-Object FullName,Length
Get-FileHash -LiteralPath $zip -Algorithm SHA256 | Format-List Hash
