@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem compilar tudo ter em modo Release
if not exist build mkdir build
if exist build\CMakeCache.txt del /f build\CMakeCache.txt
if exist build\CMakeFiles rmdir /s /q build\CMakeFiles
cd build
cmake .. -DCMAKE_PREFIX_PATH="C:\Robotica\GigaLearnCPP\libtorch" -DCMAKE_BUILD_TYPE=Release -DCUDA_TOOLKIT_ROOT_DIR="%CUDA_PATH%"
cmake --build . --config Release --parallel %NUMBER_OF_PROCESSORS%
cd ..

rem --- CONFIGURAÇÕES ---
set LOG_FILE=crash_report.log
set TEMP_LIMIT=90
set COOL_DOWN=600
set PROJECT_ROOT=C:\Robotica
set BIN_PATH=%PROJECT_ROOT%\build

rem --- VARIÁVEIS DE AMBIENTE (Essenciais para a tua GPU) ---
rem set HSA_OVERRIDE_GFX_VERSION=12.0.1
rem set AMD_SERIALIZE_KERNEL=3
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2
set PYTHONHOME=C:\Python314
set PYTHONPATH=C:\Python314\Lib\site-packages;%BIN_PATH%\python_scripts
set PATH=%CUDA_PATH%\bin;C:\Robotica\GigaLearnCPP\libtorch\lib;%PATH%

:loop
call :log "A iniciar GigaLearnBot..."
cd /d "%BIN_PATH%"
GigaLearnBot.exe --render
set EXIT_CODE=%ERRORLEVEL%

if %EXIT_CODE% neq 0 goto crash
call :log "Bot fechado manualmente ou finalizado com sucesso."
goto :eof

:crash
call :log "CRASH DETETADO! Código: %EXIT_CODE%"

rem Obtém a temperatura atual da GPU (Sensor Edge)
rem rocm-smi não disponível no Windows; substituir por ferramenta AMD equivalente
rem for /f "tokens=..." %%T in ('rocm-smi --showtemp ^| findstr Temperature') do set GPU_TEMP=%%T

rem call :log "Temperatura da GPU: !GPU_TEMP! graus C"

rem if !GPU_TEMP! gtr %TEMP_LIMIT% (
rem     call :log "ALERTA: GPU sobreaquecida! A arrefecer por 10 minutos..."
rem     timeout /t %COOL_DOWN% /nobreak > nul
rem ) else (
call :log "Temperatura GPU indisponível no Windows. A reiniciar em 5 segundos..."
timeout /t 5 /nobreak > nul
rem )
goto loop

:log
echo [%date% %time%] %~1
echo [%date% %time%] %~1 >> "%LOG_FILE%"
goto :eof
