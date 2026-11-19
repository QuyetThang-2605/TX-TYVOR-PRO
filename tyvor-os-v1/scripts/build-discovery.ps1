param([ValidateSet('native','stm32','all')][string]$Target='all')
$ErrorActionPreference='Stop'
$DiscoveryRequestedTarget=$Target # activate.ps1 is dot-sourced and has its own $Target parameter.
$TyvorRoot=Split-Path -Parent $PSScriptRoot
$TyvorEvidence=Join-Path $TyvorRoot 'build/discovery/evidence'
New-Item -ItemType Directory -Force $TyvorEvidence | Out-Null
$TyvorPython=Join-Path $TyvorRoot '.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe'
$TyvorReceipt=Join-Path $PSScriptRoot 'discovery_artifacts.py'
function Invoke-Checked {
    param([string]$Exe,[string[]]$Arguments,[string]$Log)
    $oldPreference=$ErrorActionPreference;$ErrorActionPreference='Continue'
    & $Exe @Arguments 2>&1 | ForEach-Object {$_.ToString()} | Tee-Object -FilePath $Log
    $exitCode=$LASTEXITCODE;$ErrorActionPreference=$oldPreference
    if($exitCode -ne 0){throw "$Exe failed (exit $exitCode); see $Log"}
}
if($DiscoveryRequestedTarget -in 'native','all'){
    . (Join-Path $PSScriptRoot 'activate.ps1') -Target native
    foreach($config in 'Debug','Release'){
        $label='native-'+$config.ToLower();$build=Join-Path $TyvorRoot ('build/discovery/'+$label)
        Invoke-Checked $TyvorPython @($TyvorReceipt,'begin','--target','native','--config',$config,'--compiler',(Get-Command clang).Source) (Join-Path $TyvorEvidence ($label+'-inputs.txt'))
        Invoke-Checked cmake @('-S',$TyvorRoot,'-B',$build,'-G','Ninja',"-DCMAKE_BUILD_TYPE=$config","-DCMAKE_C_COMPILER=$((Get-Command clang).Source)",'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DTYVOR_BUILD_DISCOVERY_TESTS=ON') (Join-Path $TyvorEvidence ($label+'-configure.txt'))
        Invoke-Checked cmake @('--build',$build) (Join-Path $TyvorEvidence ($label+'-build.txt'))
        Invoke-Checked ctest @('--test-dir',$build,'--output-on-failure','--output-junit',(Join-Path $TyvorEvidence ($label+'.xml'))) (Join-Path $TyvorEvidence ($label+'-ctest.txt'))
        Invoke-Checked $TyvorPython @($TyvorReceipt,'finish','--target','native','--config',$config) (Join-Path $TyvorEvidence ($label+'-receipt.txt'))
    }
}
if($DiscoveryRequestedTarget -in 'stm32','all'){
    . (Join-Path $PSScriptRoot 'activate.ps1') -Target stm32
    foreach($variant in 'discovery','legacy-ve'){
        $build=Join-Path $TyvorRoot ('build/discovery/'+$variant)
        $source=if($variant -eq 'discovery'){Join-Path $TyvorRoot 'stm32/boards/stm32f4_discovery'}else{Join-Path $TyvorRoot 'stm32'}
        $gate=if($variant -eq 'discovery'){'-DTYVOR_DISCOVERY_UART_ENABLED=OFF'}else{'-DTYVOR_UART2_PA2_PA3_VERIFIED=OFF'}
        Invoke-Checked $TyvorPython @($TyvorReceipt,'begin','--target',$variant,'--config','MinSizeRel','--compiler',(Get-Command arm-none-eabi-gcc).Source) (Join-Path $TyvorEvidence ($variant+'-inputs.txt'))
        Invoke-Checked cmake @('-S',$source,'-B',$build,'-G','Ninja',"-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $TyvorRoot 'cmake/arm-none-eabi.cmake')",'-DCMAKE_BUILD_TYPE=MinSizeRel','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',$gate) (Join-Path $TyvorEvidence ($variant+'-configure.txt'))
        Invoke-Checked cmake @('--build',$build) (Join-Path $TyvorEvidence ($variant+'-build.txt'))
        Invoke-Checked $TyvorPython @($TyvorReceipt,'finish','--target',$variant,'--config','MinSizeRel') (Join-Path $TyvorEvidence ($variant+'-receipt.txt'))
    }
}
Write-Host 'Discovery offline builds/tests only. No USB, backup, flash, erase, reset, option-byte, driver or Git mutations.'
