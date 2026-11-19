param([ValidateSet('native','stm32','esp32','all')][string]$Target='all')
# Dot-source this file. Changes affect this PowerShell process only.
$TyvorRoot=Split-Path -Parent $PSScriptRoot
$TyvorTools=Join-Path $TyvorRoot '.tools'
$TyvorPaths=@((Join-Path $TyvorTools 'cmake-3.30.9-windows-x86_64\bin'),(Join-Path $TyvorTools 'ninja'))
if($Target -in 'native','all'){ $TyvorPaths+=Join-Path $TyvorTools 'llvm-mingw-20250613-ucrt-x86_64\bin' }
if($Target -in 'stm32','all'){ $TyvorPaths+=Join-Path $TyvorTools 'bin' }
if($Target -in 'esp32','all'){
    $env:IDF_PATH=Join-Path $TyvorTools 'idf-source\esp-idf-5.5.1'
    $env:IDF_TOOLS_PATH=Join-Path $TyvorTools 'espressif'
    $env:IDF_PYTHON_ENV_PATH=Join-Path $env:IDF_TOOLS_PATH 'python_env\idf5.5_py3.11_env'
    $env:IDF_COMPONENT_MANAGER='0'
    # Source archive SDK: never derive IDF version from the parent WIP repository.
    $env:GIT_CEILING_DIRECTORIES=$TyvorRoot
    $env:IDF_SKIP_CHECK_SUBMODULES='1' # Archive dependencies checked by tools.lock.json SHA-256.
    $env:ESP_ROM_ELF_DIR=Join-Path $env:IDF_TOOLS_PATH 'tools\esp-rom-elfs\20241011'
    $TyvorPaths+=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts'
    $TyvorPaths+=Join-Path $env:IDF_TOOLS_PATH 'tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin'
    $TyvorPaths+=Join-Path $env:IDF_TOOLS_PATH 'tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin'
}
$TyvorPaths | ForEach-Object { if(!(Test-Path -LiteralPath $_)){ throw "Missing tool directory: $_" } }
$env:PATH=($TyvorPaths -join ';')+';'+$env:PATH
$env:PIP_CACHE_DIR=Join-Path $TyvorTools 'pip-cache'
$env:UV_CACHE_DIR=Join-Path $TyvorTools 'uv-cache'
$env:UV_PYTHON_INSTALL_DIR=Join-Path $TyvorTools 'python'
$env:UV_PYTHON_BIN_DIR=Join-Path $TyvorTools 'python-bin'
Write-Host "TYVOR isolated $Target tools activated for this process: $TyvorTools"
