param([ValidateSet('native','esp32','all')][string]$Target='all')
$ErrorActionPreference='Stop'
$Ui2Target=$Target;$Ui2Root=Split-Path -Parent $PSScriptRoot;$Ui2Evidence=Join-Path $Ui2Root 'build/ui-v2/evidence'
New-Item -ItemType Directory -Force $Ui2Evidence | Out-Null
$Ui2Python=Join-Path $Ui2Root '.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe'
$Ui2Check=Join-Path $PSScriptRoot 'ui2_artifacts.py'
function Invoke-Ui2Checked {
    param([string]$Exe,[string[]]$Arguments,[string]$Log)
    $saved=$ErrorActionPreference;$ErrorActionPreference='Continue'
    & $Exe @Arguments 2>&1 | ForEach-Object {$_.ToString()} | Tee-Object -FilePath $Log
    $code=$LASTEXITCODE;$ErrorActionPreference=$saved
    if($code -ne 0){throw "$Exe failed ($code); see $Log"}
}
Invoke-Ui2Checked $Ui2Python @((Join-Path $PSScriptRoot 'fetch-tft-dependencies.py'),'--verify-only') (Join-Path $Ui2Evidence 'dependencies.txt')
if($Ui2Target -in 'native','all'){
    . (Join-Path $PSScriptRoot 'activate.ps1') -Target native
    foreach($config in 'Debug','Release'){
        $name='native-'+$config.ToLower();$build=Join-Path $Ui2Root ('build/ui-v2/'+$name)
        Invoke-Ui2Checked $Ui2Python @($Ui2Check,'begin','--target','native','--config',$config,'--compiler',(Get-Command clang).Source) (Join-Path $Ui2Evidence ($name+'-inputs.txt'))
        Invoke-Ui2Checked cmake @('-S',$Ui2Root,'-B',$build,'-G','Ninja',"-DCMAKE_BUILD_TYPE=$config",'-DCMAKE_C_COMPILER=clang','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DTYVOR_BUILD_DISCOVERY_TESTS=ON','-DTYVOR_BUILD_TFT_EXPERIMENT=ON','-DTYVOR_BUILD_UI_V2=ON') (Join-Path $Ui2Evidence ($name+'-configure.txt'))
        Invoke-Ui2Checked cmake @('--build',$build,'--parallel','8') (Join-Path $Ui2Evidence ($name+'-build.txt'))
        Invoke-Ui2Checked ctest @('--test-dir',$build,'--output-on-failure','--output-junit',(Join-Path $Ui2Evidence ($name+'.xml'))) (Join-Path $Ui2Evidence ($name+'-ctest.txt'))
        Invoke-Ui2Checked $Ui2Python @($Ui2Check,'finish','--target','native','--config',$config) (Join-Path $Ui2Evidence ($name+'-receipt.txt'))
    }
}
if($Ui2Target -in 'esp32','all'){
    . (Join-Path $PSScriptRoot 'activate.ps1') -Target esp32
    $build=Join-Path $Ui2Root 'build/ui-v2/esp32';$project=Join-Path $Ui2Root 'esp32/experiments/tyvor_ui_v2'
    $idfPython=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
    Invoke-Ui2Checked $Ui2Python @($Ui2Check,'begin','--target','esp32','--config','IDF','--compiler',(Get-Command xtensa-esp32s3-elf-gcc).Source) (Join-Path $Ui2Evidence 'esp32-inputs.txt')
    Invoke-Ui2Checked $idfPython @((Join-Path $env:IDF_PATH 'tools/idf.py'),'-C',$project,'-B',$build,'-D',"SDKCONFIG=$build/sdkconfig",'-D',"SDKCONFIG_DEFAULTS=$project/sdkconfig.defaults",'reconfigure','build','size') (Join-Path $Ui2Evidence 'esp32-build.txt')
    Invoke-Ui2Checked $Ui2Python @($Ui2Check,'finish','--target','esp32','--config','IDF') (Join-Path $Ui2Evidence 'esp32-receipt.txt')
}
Write-Host 'UI V2 build/test only. No flash, device access, GPIO enablement or Git writes.'
