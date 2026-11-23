@echo off
REM ---------------------------------------------------------------
REM  RailNet PMS - Windows build script (MinGW gcc)
REM  usage:  build.bat        build only
REM          build.bat run    build and start
REM ---------------------------------------------------------------
setlocal enabledelayedexpansion

if not exist build mkdir build

set SRC=
for /r src %%f in (*.c) do set SRC=!SRC! "%%f"

echo Compiling RailNet PMS...
gcc -std=gnu99 -Wall -Wextra -O2 -Iinclude %SRC% -o build\railnet.exe
if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)
echo Build OK  -^>  build\railnet.exe

if /i "%1"=="run" build\railnet.exe
endlocal
