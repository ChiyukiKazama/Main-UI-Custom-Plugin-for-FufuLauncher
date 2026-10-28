Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Assert-Version([string]$Version) {
    if ($Version -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\z') { throw 'Version must be exactly major.minor.patch, without leading zeroes or suffixes.' }
    foreach ($part in $Version.Split('.')) {
        if ($part.Length -gt 5 -or [int]$part -gt 65535) { throw 'Each VERSIONINFO component must be in 0..65535.' }
    }
}

function Get-SourceVersion([string]$Root) {
    $header=[IO.File]::ReadAllText((Join-Path $Root 'src/Version.h'))
    $match=[regex]::Match($header,'(?m)^#define MAIN_UI_CUSTOMIZER_VERSION "([^"]+)"\s*$')
    if (-not $match.Success) { throw 'Cannot read source version.' }
    Assert-Version $match.Groups[1].Value
    return $match.Groups[1].Value
}

function Get-General([string]$Text) {
    $sections=[regex]::Matches($Text,'(?ms)^\uFEFF?\[General\][ \t]*\r?\n(?<body>.*?)(?=^\[|\z)')
    if ($sections.Count -ne 1) { throw 'Exactly one General INI section is required.' }
    return $sections[0].Groups['body']
}

function Get-GeneralValue([string]$Text,[string]$Key) {
    $body=Get-General $Text
    $matches=[regex]::Matches($body.Value,'(?m)^'+[regex]::Escape($Key)+'[ \t]*=[ \t]*(?<value>[^\r\n]*?)[ \t]*\r?$')
    if ($matches.Count -ne 1) { throw "Exactly one General.$Key is required." }
    return $matches[0].Groups['value'].Value
}

function Get-PackageConfig([string]$Root,[string]$Version) {
    $utf8=[Text.UTF8Encoding]::new($false,$true)
    $bytes=[IO.File]::ReadAllBytes((Join-Path $Root 'config.ini'))
    $text=$utf8.GetString($bytes)
    if ((Get-GeneralValue $text 'File') -cne 'MainUICustomizer.dll') { throw 'Default INI DLL filename mismatch.' }
    if ((Get-GeneralValue $text 'Version') -cne (Get-SourceVersion $Root)) { throw 'Source header and default INI base versions differ.' }
    $body=Get-General $text
    $value=[regex]::Match($body.Value,'(?m)^Version[ \t]*=[ \t]*(?<value>[^\r\n]*?)[ \t]*\r?$').Groups['value']
    $index=$body.Index+$value.Index
    return ,$utf8.GetBytes($text.Substring(0,$index)+$Version+$text.Substring($index+$value.Length))
}

function Get-BytesHash([byte[]]$Bytes) {
    return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}

function Assert-PeDll([string]$Path,[string]$Version) {
    $data=[IO.File]::ReadAllBytes($Path)
    if ($data.Length -lt 256 -or [BitConverter]::ToUInt16($data,0) -ne 0x5A4D) { throw 'Invalid DOS header.' }
    $offset=[BitConverter]::ToInt32($data,0x3C)
    if ($offset -lt 64 -or $offset+264 -gt $data.Length -or [BitConverter]::ToUInt32($data,$offset) -ne 0x4550) { throw 'Invalid PE header.' }
    if ([BitConverter]::ToUInt16($data,$offset+4) -ne 0x8664 -or [BitConverter]::ToUInt16($data,$offset+24) -ne 0x20B) { throw 'Expected x64 PE32+.' }
    $flags=[BitConverter]::ToUInt16($data,$offset+22)
    if (($flags -band 0x2002) -ne 0x2002) { throw 'Expected executable DLL characteristics.' }
    $info=[Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    if ($info.ProductVersion -cne $Version -or $info.FileVersion -cne "$Version.0" -or
        $info.OriginalFilename -cne 'MainUICustomizer.dll' -or $info.InternalName -cne 'MainUICustomizer') { throw 'DLL version or filename metadata mismatch.' }
}

function Get-LoadAuditHash([string]$Root) {
    # Pin the reviewed non-game entry guard, global initializers and build inputs.
    # Version resources are data only and are generated separately for dispatch.
    $files=@(Get-ChildItem -LiteralPath (Join-Path $Root 'src') -File | Where-Object { $_.Extension -in '.cpp','.h' -and $_.Name -ne 'Version.h' })
    $files+=@(Get-ChildItem -LiteralPath (Join-Path $Root 'vendor/minhook') -Recurse -File | Where-Object { $_.Extension -in '.c','.h' })
    $files+=Get-Item -LiteralPath (Join-Path $Root 'MainUICustomizer.vcxproj')
    $text=[Text.StringBuilder]::new()
    foreach ($file in $files | Sort-Object FullName) {
        $relative=[IO.Path]::GetRelativePath($Root,$file.FullName).Replace('\','/')
        [void]$text.Append($relative+"`n"+[IO.File]::ReadAllText($file.FullName).Replace("`r`n","`n")+"`n")
    }
    return Get-BytesHash ([Text.Encoding]::UTF8.GetBytes($text.ToString()))
}

function Assert-LoadSafe([string]$Root) {
    $policy=Get-Content -LiteralPath (Join-Path $Root '.github/ci/policy.json') -Raw | ConvertFrom-Json
    if ((Get-LoadAuditHash $Root) -cne $policy.safeLoadSourcesSHA256) { throw 'Non-game host safety audit is stale. Review the entry guard and global initializers before updating policy.json.' }
    $hostName=[IO.Path]::GetFileNameWithoutExtension([Environment]::ProcessPath)
    if ($hostName -ine 'pwsh' -or -not [Environment]::Is64BitProcess) { throw 'Load probes require a non-game x64 pwsh host.' }
}

function Assert-Package([string]$Root,[string]$Directory,[string]$Version,[string]$Commit,[string]$ExpectedHash='') {
    Assert-Version $Version
    $name="MainUICustomizer-$Version.zip"
    $path=Join-Path $Directory $name
    $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $checksum=[IO.File]::ReadAllText((Join-Path $Directory "$name.sha256")).Trim()
    if ($checksum -cne "$hash  $name" -or ($ExpectedHash -and $hash -cne $ExpectedHash)) { throw 'ZIP SHA256 mismatch.' }
    $report=Get-Content -LiteralPath (Join-Path $Directory 'compression-report.json') -Raw | ConvertFrom-Json
    if ($report.version -cne $Version -or $report.commit -cne $Commit -or $report.zipSHA256 -cne $hash -or
        $report.packedBytes -ge $report.originalBytes -or -not $report.integrityPassed -or -not $report.decompressionPassed -or -not $report.loadPassed) { throw 'Validation report mismatch.' }
    $zip=[IO.Compression.ZipFile]::OpenRead($path)
    try {
        $names=@($zip.Entries | ForEach-Object FullName)
        if ($names.Count -ne 2 -or -not ($names -ccontains 'MainUICustomizer.dll') -or -not ($names -ccontains 'config.ini')) { throw 'ZIP root must contain exactly the DLL and default INI.' }
        foreach ($entry in $zip.Entries) {
            $stream=$entry.Open(); $memory=[IO.MemoryStream]::new()
            try { $stream.CopyTo($memory); $bytes=$memory.ToArray() } finally { $stream.Dispose(); $memory.Dispose() }
            if ($entry.FullName -ceq 'config.ini') {
                if ((Get-BytesHash $bytes) -cne (Get-BytesHash (Get-PackageConfig $Root $Version))) { throw 'ZIP does not contain the versioned repository default INI.' }
            } elseif ((Get-BytesHash $bytes) -cne $report.packedSHA256) { throw 'ZIP DLL hash mismatch.' }
        }
    } finally { $zip.Dispose() }
    return $hash
}
