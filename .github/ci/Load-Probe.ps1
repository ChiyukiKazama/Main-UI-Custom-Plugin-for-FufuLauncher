param([Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][string]$Dll)
. (Join-Path $PSScriptRoot 'Common.ps1')
# Audited PluginWorker returns before initialization unless the host is YuanShen.exe.
# Process exit owns DLL teardown: never unload while its attach thread can be running.
Assert-LoadSafe $Root
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CiDllProbe {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr LoadLibraryW(string path);
}
'@
if ([CiDllProbe]::LoadLibraryW((Resolve-Path -LiteralPath $Dll).Path) -eq [IntPtr]::Zero) { throw "LoadLibrary failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
Start-Sleep -Milliseconds 300
Write-Output 'PASS: DLL loaded in audited non-game pwsh host.'
