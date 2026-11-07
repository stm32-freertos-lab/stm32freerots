param([string]$LogPath = 'build/build-output.log')
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    node scripts/build_eide.cjs --configure-only
    if ($LASTEXITCODE -ne 0) { throw 'Failed to prepare EIDE build parameters' }
    $eideExtension = Get-ChildItem "$env:USERPROFILE/.vscode/extensions" -Directory -Filter 'cl.eide-*' |
        Sort-Object Name | Select-Object -Last 1
    $builder = Join-Path $eideExtension.FullName 'res/tools/win32/unify_builder/unify_builder.exe'
    & $builder -p build/NWatch-DShanMCU-F103/builder.params --rebuild --no-color |
        Tee-Object -FilePath $LogPath
    if ($LASTEXITCODE -ne 0) { throw "EIDE build failed: $LASTEXITCODE" }
}
finally {
    Pop-Location
}
