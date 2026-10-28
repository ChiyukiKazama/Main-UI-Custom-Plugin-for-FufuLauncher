param([ValidateSet('Release','Debug')][string]$Configuration='Release',[string]$MSBuildPath='',[switch]$SyncDefaultConfig)
$ErrorActionPreference='Stop'
if (-not $MSBuildPath) {
    $command=Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) { $MSBuildPath=$command.Source }
    elseif (Test-Path -LiteralPath 'C:\VSTool\VSBuildTools2026\MSBuild\Current\Bin\MSBuild.exe') {
        $MSBuildPath='C:\VSTool\VSBuildTools2026\MSBuild\Current\Bin\MSBuild.exe'
    } else { throw 'Install Visual Studio C++ tools and Windows SDK, or pass -MSBuildPath.' }
}
& $MSBuildPath (Join-Path $PSScriptRoot 'MainUICustomizer.vcxproj') /m "/p:Configuration=$Configuration" "/p:SyncDefaultConfig=$SyncDefaultConfig" /p:Platform=x64 /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw "MainUICustomizer build failed ($LASTEXITCODE)." }
