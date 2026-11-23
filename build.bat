@echo off
REM ---------------------------------------------------------------
REM  RailNet PMS - Windows build script (MinGW gcc + windres)
REM  usage:  build.bat            build desktop app + console app
REM          build.bat run        build and start the desktop app
REM ---------------------------------------------------------------
setlocal enabledelayedexpansion
cd /d "%~dp0"
if not exist build mkdir build

set CFLAGS=-std=gnu99 -Wall -Wextra -O2 -Iinclude

set CORE=
for %%d in (core io util) do for %%f in (src\%%d\*.c) do set CORE=!CORE! "%%f"
set GUI=
for %%f in (src\gui\*.c) do set GUI=!GUI! "%%f"
set CONSOLE="src\main.c"
for %%f in (src\ui\*.c) do set CONSOLE=!CONSOLE! "%%f"

echo [1/3] Compiling resources...
windres -Iinclude -Iresources resources\app.rc -O coff -o build\app_res.o || goto :fail

echo [2/3] Building desktop app...
gcc %CFLAGS% %CORE% %GUI% build\app_res.o -o build\RailNetPMS.exe -mwindows -lcomctl32 -lgdi32 -lshell32 || goto :fail

echo [3/3] Building console app...
gcc %CFLAGS% %CORE% %CONSOLE% -o build\railnet_console.exe || goto :fail

echo.
echo Build OK
echo   Desktop app : build\RailNetPMS.exe
echo   Console app : build\railnet_console.exe
if /i "%1"=="run" start "" build\RailNetPMS.exe
exit /b 0

:fail
echo.
echo BUILD FAILED
exit /b 1
