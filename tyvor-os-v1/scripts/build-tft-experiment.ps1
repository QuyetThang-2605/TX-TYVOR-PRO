param([ValidateSet('native','esp32','all')][string]$Target='all')
$ErrorActionPreference='Stop'
$TftRequestedTarget=$Target
$TftRoot=Split-Path -Parent $PSScriptRoot
$TftEvidence=Join-Path $TftRoot 'build/tft/evidence'
New-Item -ItemType Directory -Force $TftEvidence | Out-Null
$TftPython=Join-Path $TftRoot '.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe'
$TftArtifacts=Join-Path $PSScriptRoot 'tft_artifacts.py'
function Invoke-TftChecked {
    param([string]$Exe,[string[]]$Arguments,[string]$Log)
    $saved=$ErrorActionPreference;$ErrorActionPreference='Continue'
    & $Exe @Arguments 2>&1 | ForEach-Object {$_.ToString()} | Tee-Object -FilePath $Log
    $code=$LASTEXITCODE;$ErrorActionPreference=$saved
    if($code -ne 0){throw "$Exe failed ($code); see $Log"}
}
Invoke-TftChecked $TftPython @($TftArtifacts,'dependencies') (Join-Path $TftEvidence 'dependency-check.txt')
if($TftRequestedTarget -in 'native','all'){
    . (Join-Path $PSScriptRoot 'activate.ps1') -Target native
    foreach($config in 'Debug','Release'){
        $name='native-'+$config.ToLower();$build=Join-Path $TftRoot ('build/tft/'+$name)
        Invoke-TftChecked $TftPython @($TftArtifacts,'begin','--target','native','--config',$config,'--compiler',(Get-Command clang).Source) (Join-Path $TftEvidence ($name+'-inputs.txt'))
        Invoke-TftChecked cmake @('-S',$TftRoot,'-B',$build,'-G','Ninja',"-DCMAKE_BUILD_TYPE=$config","-DCMAKE_C_COMPILER=$((Get-Command clang).Source)",'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DTYVOR_BUILD_DISCOVERY_TESTS=ON','-DTYVOR_BUILD_TFT_EXPERIMENT=ON') (Join-Path $TftEvidence ($name+'-configure.txt'))
        Invoke-TftChecked cmake @('--build',$build,'--parallel','6') (Join-Path $TftEvidence ($name+'-build.txt'))
        Invoke-TftChecked ctest @('--test-dir',$build,'--output-on-failure','--output-junit',(Join-Path $TftEvidence ($name+'.xml'))) (Join-Path $TftEvidence ($name+'-ctest.txt'))
        Invoke-TftChecked $TftPython @($TftArtifacts,'finish','--target','native','--config',$config) (Join-Path $TftEvidence ($name+'-receipt.txt'))
    }
}
if($TftRequestedTarget -in 'esp32','all'){
    . (Join-Path $PSScriptRoot 'activate.ps1') -Target esp32
    $build=Join-Path $TftRoot 'build/tft/esp32';$project=Join-Path $TftRoot 'esp32/experiments/tft_spi_320x240'
    $idfPython=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
    Invoke-TftChecked $TftPython @($TftArtifacts,'begin','--target','esp32','--config','IDF','--compiler',(Get-Command xtensa-esp32s3-elf-gcc).Source) (Join-Path $TftEvidence 'esp32-inputs.txt')
    Invoke-TftChecked $idfPython @((Join-Path $env:IDF_PATH 'tools/idf.py'),'-C',$project,'-B',$build,'-D',"SDKCONFIG=$build/sdkconfig",'-D',"SDKCONFIG_DEFAULTS=$project/sdkconfig.defaults",'reconfigure','build','size') (Join-Path $TftEvidence 'esp32-build.txt')
    Invoke-TftChecked $TftPython @($TftArtifacts,'finish','--target','esp32','--config','IDF') (Join-Path $TftEvidence 'esp32-receipt.txt')
}
Write-Host 'TFT experiment offline build/test only. No flash/serial/reset or Git writes.'
