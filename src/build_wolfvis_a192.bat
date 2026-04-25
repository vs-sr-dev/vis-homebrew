@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a192.c ===
wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a192.obj wolfvis_a192.c
if errorlevel 1 goto :fail

echo === Linking WOLFA192.EXE ===
wlink @link_wolfvis_a192.lnk
if errorlevel 1 goto :fail

echo === Copying WOLFA192.EXE to cd_root_a192 ===
copy /y ..\build\WOLFA192.EXE ..\cd_root_a192\WOLFA192.EXE >nul
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA192.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
