@echo off
echo ===================================================================
echo   NAP FIRMWARE TYVOR OXBOT UI V3.2.1 + TFT SPI2 + XPT2046 TOUCH
echo   Target: STM32F407G-DISC1 (0x08000000)
echo ===================================================================
"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD mode=UR reset=HWrst -w "%~dp0..\build\discovery-tft\tyvor_discovery_tft.bin" 0x08000000 -v -rst
pause
