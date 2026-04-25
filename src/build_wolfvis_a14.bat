@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a14.c ===
wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a14.obj wolfvis_a14.c
if errorlevel 1 goto :fail

echo === Linking WOLFA14.EXE ===
wlink @link_wolfvis_a14.lnk
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA14.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
