param([switch]$IncludeEsp32,[switch]$VerifyOnly)
# Portable, workspace-only install. No registry/PATH/Git configuration changes.
$ErrorActionPreference='Stop'
$TyvorRoot=Split-Path -Parent $PSScriptRoot
$TyvorTools=Join-Path $TyvorRoot '.tools'
$TyvorDownloads=Join-Path $TyvorTools 'downloads'
New-Item -ItemType Directory -Force $TyvorDownloads | Out-Null
$phaseLock=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'tools.lock.json') -Raw | ConvertFrom-Json
foreach($phaseItem in $phaseLock.archives){
    if($phaseItem.group -eq 'esp32' -and !$IncludeEsp32){continue}
    $phaseArchive=Join-Path $TyvorDownloads $phaseItem.file
    if(!(Test-Path -LiteralPath $phaseArchive)){
        if($VerifyOnly){throw "Missing cached archive: $phaseArchive"}
        $phasePart=$phaseArchive+'.part'
        & curl.exe -fL --connect-timeout 30 --max-time 1800 -C - -o $phasePart $phaseItem.url
        if($LASTEXITCODE -ne 0){throw "Download failed; rerun to resume $phasePart"}
        if((Get-FileHash -Algorithm SHA256 -LiteralPath $phasePart).Hash -ne $phaseItem.sha256){throw "SHA-256 mismatch: $phasePart"}
        Move-Item -LiteralPath $phasePart -Destination $phaseArchive
    }
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $phaseArchive).Hash -ne $phaseItem.sha256){throw "SHA-256 mismatch: $phaseArchive"}
    $phaseDestination=Join-Path $TyvorTools $phaseItem.destination
    $phaseMarker=Join-Path $TyvorTools $phaseItem.marker
    if(!(Test-Path -LiteralPath $phaseMarker)){
        if($VerifyOnly){throw "Missing installed file: $phaseMarker"}
        New-Item -ItemType Directory -Force $phaseDestination | Out-Null
        if($phaseItem.strip){ & tar.exe -xf $phaseArchive --strip-components 1 -C $phaseDestination }
        else { & tar.exe -xf $phaseArchive -C $phaseDestination }
        if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $phaseMarker)){throw "Extraction failed: $phaseArchive"}
    }
    Write-Host "Verified $($phaseItem.file)"
}
if(!$VerifyOnly){
    $env:UV_CACHE_DIR=Join-Path $TyvorTools 'uv-cache'
    $env:UV_PYTHON_INSTALL_DIR=Join-Path $TyvorTools 'python'
    $env:UV_PYTHON_BIN_DIR=Join-Path $TyvorTools 'python-bin'
    & (Join-Path $TyvorTools 'uv\uv.exe') python install 3.11.13 --no-bin --no-registry
    if($LASTEXITCODE -ne 0){throw 'Managed Python install failed'}
}
if($IncludeEsp32 -and !$VerifyOnly){
    $env:IDF_PATH=Join-Path $TyvorTools 'idf-source\esp-idf-5.5.1'
    $env:IDF_TOOLS_PATH=Join-Path $TyvorTools 'espressif'
    $env:PIP_CACHE_DIR=Join-Path $TyvorTools 'pip-cache'
    # The source archive lacks Git metadata; record upstream release explicitly.
    Set-Content -LiteralPath (Join-Path $env:IDF_PATH 'version.txt') -Value 'v5.5.1' -Encoding ASCII
    $phasePython=Join-Path $TyvorTools 'python\cpython-3.11.13-windows-x86_64-none\python.exe'
    $phaseSavedConstraint=$env:PIP_CONSTRAINT
    try {
        $env:PIP_CONSTRAINT=Join-Path $PSScriptRoot 'esp-idf-python-lock.txt'
        & $phasePython (Join-Path $env:IDF_PATH 'tools\idf_tools.py') --non-interactive install-python-env
        if($LASTEXITCODE -ne 0){throw 'ESP-IDF Python environment install failed'}
    } finally { $env:PIP_CONSTRAINT=$phaseSavedConstraint }
}
Write-Host 'Portable tools prepared under .tools. Activate using scripts/activate.ps1.'
