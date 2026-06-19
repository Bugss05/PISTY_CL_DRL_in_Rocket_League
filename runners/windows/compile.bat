@echo off
REM Rebuild cmake de raiz — usa quando adicionas ficheiros .cpp novos
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "CONFIG_H=%SCRIPT_DIR%..\..\src\config.h"

REM Le configuracoes do src\config.h
for /f "tokens=3" %%A in ('findstr /b "#define CONFIG_PROJECT_ROOT" "%CONFIG_H%"') do set "PROJECT_ROOT=%%~A"
for /f "tokens=3" %%A in ('findstr /b "#define CONFIG_LIBTORCH_PATH" "%CONFIG_H%"') do set "LIBTORCH_PATH=%%~A"
for /f "tokens=3*" %%A in ('findstr /b "#define CONFIG_VS_GENERATOR" "%CONFIG_H%"') do set "VS_GENERATOR=%%~A"

REM Torch_DIR e derivado do LIBTORCH_PATH
set "TORCH_DIR=%LIBTORCH_PATH%\share\cmake\Torch"

if exist "%PROJECT_ROOT%\build" rmdir /s /q "%PROJECT_ROOT%\build"
mkdir "%PROJECT_ROOT%\build"
cd /d "%PROJECT_ROOT%\build"
cmake .. -G "%VS_GENERATOR%" -A x64 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTorch_DIR="%TORCH_DIR%"

endlocal
