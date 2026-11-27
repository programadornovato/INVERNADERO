@echo off
setlocal EnableDelayedExpansion
rem =====================================================================
rem  CREAR UN SOLO .BIN DEL INVERNADERO
rem
rem  Une bootloader + particiones + boot_app0 + programa en un archivo
rem  que se graba en la direccion 0x0:
rem      build\esp32.esp32.esp32\INVERNADERO_completo.bin
rem
rem  Antes: en Arduino IDE usa "Sketch > Exportar binario compilado".
rem =====================================================================

set "PROYECTO=INVERNADERO"
set "CARPETA=%~dp0build\esp32.esp32.esp32"
set "SALIDA=%CARPETA%\%PROYECTO%_completo.bin"
set "PAQUETES=%LOCALAPPDATA%\Arduino15\packages\esp32"

rem ---- Archivos de la compilacion ----
set "APP=%CARPETA%\%PROYECTO%.ino.bin"
set "PARTICIONES=%CARPETA%\%PROYECTO%.ino.partitions.bin"

if not exist "%APP%" (
  echo ERROR: no existe %APP%
  echo Primero exporta el binario desde Arduino IDE.
  goto :fin
)
if not exist "%PARTICIONES%" (
  echo ERROR: no existe %PARTICIONES%
  goto :fin
)

rem ---- esptool: se usa la version mas nueva instalada ----
set "ESPTOOL="
for /d %%v in ("%PAQUETES%\tools\esptool_py\*") do (
  if exist "%%v\esptool.exe" set "ESPTOOL=%%v\esptool.exe"
)
if not defined ESPTOOL (
  echo ERROR: no se encontro esptool.exe en %PAQUETES%\tools\esptool_py
  goto :fin
)

rem ---- Nucleo ESP32 instalado (la version mas nueva) ----
set "NUCLEO="
for /d %%v in ("%PAQUETES%\hardware\esp32\*") do set "NUCLEO=%%v"
if not defined NUCLEO (
  echo ERROR: no se encontro el nucleo ESP32 en %PAQUETES%\hardware\esp32
  goto :fin
)

rem ---- Bootloader: el de la compilacion si existe; si no, el del nucleo ----
set "BOOTLOADER=%CARPETA%\%PROYECTO%.ino.bootloader.bin"
if not exist "%BOOTLOADER%" set "BOOTLOADER=%NUCLEO%\tools\sdk\bin\bootloader_dio_80m.bin"
if not exist "%BOOTLOADER%" (
  echo ERROR: no se encontro el bootloader
  goto :fin
)

set "BOOTAPP0=%NUCLEO%\tools\partitions\boot_app0.bin"
if not exist "%BOOTAPP0%" (
  echo ERROR: no se encontro %BOOTAPP0%
  goto :fin
)

echo.
echo Bootloader : %BOOTLOADER%
echo Particiones: %PARTICIONES%
echo boot_app0  : %BOOTAPP0%
echo Programa   : %APP%
echo esptool    : %ESPTOOL%
echo.

"%ESPTOOL%" --chip esp32 merge_bin -o "%SALIDA%" ^
  --flash_mode dio --flash_freq 80m --flash_size 4MB ^
  0x1000  "%BOOTLOADER%" ^
  0x8000  "%PARTICIONES%" ^
  0xe000  "%BOOTAPP0%" ^
  0x10000 "%APP%"

if errorlevel 1 (
  echo.
  echo ERROR: no se pudo crear el archivo.
  goto :fin
)

echo.
echo LISTO: %SALIDA%
echo Grabalo en la direccion 0x0, por ejemplo:
echo   esptool.exe --chip esp32 --port COM3 --baud 921600 write_flash 0x0 "%PROYECTO%_completo.bin"

:fin
echo.
pause
