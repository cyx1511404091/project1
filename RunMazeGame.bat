@echo off
setlocal

set "ROOT=%~dp0"
set "GAME_EXE="

for %%F in (
    "%ROOT%x64\Release\*.exe"
    "%ROOT%Release\*.exe"
    "%ROOT%x64\Debug\*.exe"
    "%ROOT%Debug\*.exe"
) do (
    if not defined GAME_EXE (
        set "GAME_EXE=%%~fF"
    )
)

if not defined GAME_EXE (
    echo No compiled game executable was found.
    echo Please build the project in Visual Studio first, then run this file again.
    pause
    exit /b 1
)

start "" "%GAME_EXE%"
