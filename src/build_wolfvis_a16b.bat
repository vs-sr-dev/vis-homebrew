@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a16b.c ===
wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a16b.obj wolfvis_a16b.c
if errorlevel 1 goto :fail

echo === Linking WOLFA16B.EXE ===
wlink @link_wolfvis_a16b.lnk
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA16B.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
