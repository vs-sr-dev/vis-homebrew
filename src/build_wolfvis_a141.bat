@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a141.c ===
wcc -zq -bt=windows -ml -fo=..\build\wolfvis_a141.obj wolfvis_a141.c
if errorlevel 1 goto :fail

echo === Linking WOLFA141.EXE ===
wlink @link_wolfvis_a141.lnk
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA141.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
