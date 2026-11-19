param([ValidateSet('native','stm32','esp32','all')][string]$Target='all')
$ErrorActionPreference='Stop'
$TyvorRoot=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'activate.ps1') -Target $Target
$TyvorEvidence=Join-Path $TyvorRoot 'build\evidence'
New-Item -ItemType Directory -Force $TyvorEvidence | Out-Null
$TyvorPython=Join-Path $TyvorRoot '.tools\python\cpython-3.11.13-windows-x86_64-none\python.exe'
if(!(Test-Path -LiteralPath $TyvorPython)){throw 'Build receipts require managed Python; run bootstrap-tools.ps1 first'}
$TyvorProvenance=Join-Path $PSScriptRoot 'build_provenance.py'
function Invoke-TyvorChecked {
    param([string]$Exe,[string[]]$Arguments,[string]$Log)
    $phasePreference=$ErrorActionPreference
    $ErrorActionPreference='Continue' # PS5 represents native stderr as ErrorRecord.
    & $Exe @Arguments 2>&1 | ForEach-Object { $_.ToString() } | Tee-Object -FilePath $Log
    $phaseExit=$LASTEXITCODE
    $ErrorActionPreference=$phasePreference
    if($phaseExit -ne 0){ throw "$Exe failed with exit $phaseExit; see $Log" }
}
if($Target -in 'native','all'){
    foreach($phaseConfig in 'Debug','Release'){
        $phaseBuild=Join-Path $TyvorRoot ('build\native-'+$phaseConfig.ToLower())
        $phaseLabel='native-'+$phaseConfig.ToLower()
        Invoke-TyvorChecked $TyvorPython @($TyvorProvenance,'begin','--target','native','--config',$phaseConfig,'--compiler',(Get-Command clang).Source) (Join-Path $TyvorEvidence "$phaseLabel-inputs.txt")
        Invoke-TyvorChecked cmake @('-S',$TyvorRoot,'-B',$phaseBuild,'-G','Ninja',"-DCMAKE_BUILD_TYPE=$phaseConfig","-DCMAKE_C_COMPILER=$((Get-Command clang).Source)",'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON') (Join-Path $TyvorEvidence "$phaseLabel-configure.txt")
        Invoke-TyvorChecked cmake @('--build',$phaseBuild) (Join-Path $TyvorEvidence "$phaseLabel-build.txt")
        Invoke-TyvorChecked ctest @('--test-dir',$phaseBuild,'--output-on-failure','--output-junit',(Join-Path $TyvorEvidence "$phaseLabel.xml")) (Join-Path $TyvorEvidence "$phaseLabel-ctest.txt")
        Invoke-TyvorChecked $TyvorPython @($TyvorProvenance,'finish','--target','native','--config',$phaseConfig) (Join-Path $TyvorEvidence "$phaseLabel-provenance.txt")
    }
    $phaseOutput=Join-Path $TyvorRoot 'artifacts\ui-mock'
    New-Item -ItemType Directory -Force $phaseOutput | Out-Null
    Invoke-TyvorChecked (Join-Path $TyvorRoot 'build\native-release\tyvor_ui_sim.exe') @($phaseOutput) (Join-Path $TyvorEvidence 'ui-simulator.txt')
}
if($Target -in 'stm32','all'){
    $phaseBuild=Join-Path $TyvorRoot 'build\stm32'
    Invoke-TyvorChecked $TyvorPython @($TyvorProvenance,'begin','--target','stm32','--config','MinSizeRel','--compiler',(Get-Command arm-none-eabi-gcc).Source) (Join-Path $TyvorEvidence 'stm32-inputs.txt')
    Invoke-TyvorChecked cmake @('-S',(Join-Path $TyvorRoot 'stm32'),'-B',$phaseBuild,'-G','Ninja',"-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $TyvorRoot 'cmake\arm-none-eabi.cmake')",'-DCMAKE_BUILD_TYPE=MinSizeRel') (Join-Path $TyvorEvidence 'stm32-configure.txt')
    Invoke-TyvorChecked cmake @('--build',$phaseBuild) (Join-Path $TyvorEvidence 'stm32-build.txt')
    Invoke-TyvorChecked arm-none-eabi-size @('-A',(Join-Path $phaseBuild 'tyvor_stm32.elf')) (Join-Path $TyvorEvidence 'stm32-size.txt')
    Invoke-TyvorChecked $TyvorPython @($TyvorProvenance,'finish','--target','stm32','--config','MinSizeRel') (Join-Path $TyvorEvidence 'stm32-provenance.txt')
}
if($Target -in 'esp32','all'){
    $phasePython=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
    Invoke-TyvorChecked $TyvorPython @($TyvorProvenance,'begin','--target','esp32','--config','IDF','--compiler',(Get-Command xtensa-esp32s3-elf-gcc).Source) (Join-Path $TyvorEvidence 'esp32-inputs.txt')
    Invoke-TyvorChecked $phasePython @((Join-Path $env:IDF_PATH 'tools\idf.py'),'-C',(Join-Path $TyvorRoot 'esp32'),'-B',(Join-Path $TyvorRoot 'build\esp32'),'reconfigure','build','size') (Join-Path $TyvorEvidence 'esp32-build.txt')
    Invoke-TyvorChecked $TyvorPython @($TyvorProvenance,'finish','--target','esp32','--config','IDF') (Join-Path $TyvorEvidence 'esp32-provenance.txt')
}
Write-Host 'Selected build/test steps completed. No flash/commit/push actions exist in this script.'
