$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$dllDir = Join-Path $projectRoot 'release'
$libDir = Join-Path $projectRoot 'lib\x64'
$modules = @('lpvCore', 'lpvGeom', 'lpvCalib', 'lpvGauge', 'lpvLocate')

New-Item -ItemType Directory -Force -Path $libDir | Out-Null

foreach ($module in $modules) {
    $dll = Join-Path $dllDir ($module + '.dll')
    if (!(Test-Path $dll)) {
        throw "Missing $dll"
    }

    $dump = & dumpbin /exports $dll
    $exports = New-Object System.Collections.Generic.List[string]
    $inExports = $false
    foreach ($line in $dump) {
        if ($line -match '^\s*ordinal\s+hint\s+RVA\s+name\s*$') {
            $inExports = $true
            continue
        }
        if (!$inExports) {
            continue
        }
        if ($line -match '^\s*Summary\s*$') {
            break
        }
        if ($line -match '^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)\s*$') {
            $export = $Matches[1]
            if ($export -ne '[NONAME]') {
                $exports.Add($export)
            }
        }
    }

    if ($exports.Count -eq 0) {
        throw "No named exports found in $dll"
    }

    $def = Join-Path $libDir ($module + '.def')
    $content = @("LIBRARY $module.dll", 'EXPORTS') + ($exports | ForEach-Object { "    $_" })
    [System.IO.File]::WriteAllLines($def, $content, [System.Text.Encoding]::ASCII)

    $lib = Join-Path $libDir ($module + '.lib')
    & lib /nologo /machine:x64 /def:$def /out:$lib
    if ($LASTEXITCODE -ne 0) {
        throw "lib.exe failed for $module"
    }
}

Get-ChildItem $libDir -Filter *.lib | Select-Object Name, Length
