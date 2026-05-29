@echo off
REM Batch version for Windows (alternative to PowerShell)
REM Run this with: run.bat

setlocal enabledelayedexpansion

REM --- CONFIGURAÇÕES ---
set LOG_FILE=crash_report.log
set TEMP_LIMIT=90
set COOL_DOWN=600
set PROJECT_ROOT=C:\Robotica
set BIN_PATH=%PROJECT_ROOT%\build
set BUILD_DIR=%PROJECT_ROOT%\build

REM --- VARIÁVEIS DE AMBIENTE ---
rem HSA_OVERRIDE_GFX_VERSION=12.0.1
rem AMD_SERIALIZE_KERNEL=3
set PYTHONHOME=C:\Python314
set PYTHONPATH=%BIN_PATH%

REM Build
echo [%date% %time%] A compilar o projeto... >> %LOG_FILE%
cd /d %BUILD_DIR%
cmake --build . --config Release --target GigaLearnBot

if errorlevel 1 (
    echo [%date% %time%] ERRO NA COMPILAÇÃO! Código: %ERRORLEVEL% >> %LOG_FILE%
    echo ERRO NA COMPILAÇÃO!
    pause
    exit /b 1
)

echo [%date% %time%] Compilação bem-sucedida! >> %LOG_FILE%
echo Compilação bem-sucedida!

REM Loop de execução
:LOOP
echo [%date% %time%] A iniciar GigaLearnBot... >> %LOG_FILE%
echo A iniciar GigaLearnBot...

cd /d %BIN_PATH%\Release
GigaLearnBot.exe

set EXIT_CODE=%ERRORLEVEL%

if %EXIT_CODE% neq 0 (
    echo [%date% %time%] CRASH DETETADO! Código: %EXIT_CODE% >> %LOG_FILE%
    echo CRASH DETETADO! Código: %EXIT_CODE%
    
    REM Tenta verificar temperatura da GPU
    rocm-smi --showtemp >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        echo rocm-smi disponível >> %LOG_FILE%
        for /f "tokens=2" %%A in ('rocm-smi --showtemp ^| findstr /R "Temperature"') do (
            set GPU_TEMP=%%A
            goto :TEMP_FOUND
        )
    )
    
    :TEMP_FOUND
    if defined GPU_TEMP (
        echo Temperatura da GPU: %GPU_TEMP%°C >> %LOG_FILE%
        echo Temperatura da GPU: %GPU_TEMP%°C
        
        if %GPU_TEMP% gtr %TEMP_LIMIT% (
            echo ALERTA: GPU a %GPU_TEMP%°C! A arrefecer por 10 minutos... >> %LOG_FILE%
            echo ALERTA: GPU a %GPU_TEMP%°C! A arrefecer por 10 minutos...
            timeout /t %COOL_DOWN% /nobreak
        ) else (
            echo Temperatura segura. A reiniciar em 5 segundos... >> %LOG_FILE%
            echo Temperatura segura. A reiniciar em 5 segundos...
            timeout /t 5 /nobreak
        )
    ) else (
        echo rocm-smi não disponível. A reiniciar em 5 segundos... >> %LOG_FILE%
        echo rocm-smi não disponível. A reiniciar em 5 segundos...
        timeout /t 5 /nobreak
    )
    
    goto LOOP
) else (
    echo [%date% %time%] Bot fechado manualmente ou finalizado com sucesso. >> %LOG_FILE%
    echo Bot fechado manualmente ou finalizado com sucesso.
)

endlocal
