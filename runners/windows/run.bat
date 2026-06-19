@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "CONFIG_H=%SCRIPT_DIR%..\..\src\config.h"

REM Le configuracoes do src\config.h
for /f "tokens=3" %%A in ('findstr /b "#define CONFIG_PROJECT_ROOT" "%CONFIG_H%"') do set "PROJECT_ROOT=%%~A"
for /f "tokens=3" %%A in ('findstr /b "#define CONFIG_PYTHON_HOME" "%CONFIG_H%"') do set "PYTHON_HOME=%%~A"
for /f "tokens=3" %%A in ('findstr /b "#define CONFIG_PYTHON_PACKAGES" "%CONFIG_H%"') do set "PYTHON_PACKAGES=%%~A"

set "BIN_PATH=%PROJECT_ROOT%\build"

REM --- Build incremental ---
cd /d "%PROJECT_ROOT%\build"
cmake --build . --config Release --target GigaLearnBot
if errorlevel 1 (
    echo Erro ao compilar! A cancelar arranque.
    pause
    exit /b 1
)

REM --- CONFIGURAÇÕES ---
set LOG_FILE=crash_report.log
set TEMP_LIMIT=90
set COOL_DOWN=600

REM --- VARIÁVEIS DE AMBIENTE ---
set HSA_OVERRIDE_GFX_VERSION=12.0.1
set ROCR_VISIBLE_DEVICES=0
set HIP_FORCE_DEV_KERNARG=1
set MALLOC_ARENA_MAX=4
set "PYTHONHOME=%PYTHON_HOME%"
set "PYTHONPATH=%PYTHON_PACKAGES%;%BIN_PATH%\python_scripts"

:LOOP
echo [%date% %time%] A iniciar GigaLearnBot...
echo [%date% %time%] A iniciar GigaLearnBot... >> "%LOG_FILE%"

cd /d "%BIN_PATH%"
GigaLearnBot.exe

set EXIT_CODE=%ERRORLEVEL%

if %EXIT_CODE% neq 0 (
    echo CRASH DETETADO! Codigo: %EXIT_CODE%
    echo [%date% %time%] CRASH DETETADO! Codigo: %EXIT_CODE% >> "%LOG_FILE%"

    set GPU_TEMP=
    rocm-smi --showtemp >nul 2>&1
    if not errorlevel 1 (
        for /f "tokens=2" %%A in ('rocm-smi --showtemp ^| findstr /i "Temperature"') do (
            set GPU_TEMP=%%A
            goto :TEMP_FOUND
        )
    )
    :TEMP_FOUND
    if defined GPU_TEMP (
        echo Temperatura da GPU: !GPU_TEMP!C
        echo [%date% %time%] Temperatura da GPU: !GPU_TEMP!C >> "%LOG_FILE%"
        if !GPU_TEMP! gtr %TEMP_LIMIT% (
            echo ALERTA: GPU a !GPU_TEMP!C! A arrefecer por 10 minutos...
            echo [%date% %time%] ALERTA: GPU a !GPU_TEMP!C! A arrefecer por 10 minutos... >> "%LOG_FILE%"
            timeout /t %COOL_DOWN% /nobreak
        ) else (
            echo Temperatura segura. A reiniciar em 5 segundos...
            echo [%date% %time%] Temperatura segura. A reiniciar em 5 segundos... >> "%LOG_FILE%"
            timeout /t 5 /nobreak
        )
    ) else (
        echo rocm-smi nao disponivel. A reiniciar em 5 segundos...
        echo [%date% %time%] rocm-smi nao disponivel. A reiniciar em 5 segundos... >> "%LOG_FILE%"
        timeout /t 5 /nobreak
    )

    goto LOOP
) else (
    echo Bot fechado manualmente ou finalizado com sucesso.
    echo [%date% %time%] Bot fechado manualmente ou finalizado com sucesso. >> "%LOG_FILE%"
)

endlocal
