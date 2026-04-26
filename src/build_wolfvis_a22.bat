@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a22.c ===
wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a22.obj wolfvis_a22.c
if errorlevel 1 goto :fail

echo === Linking WOLFA22.EXE ===
wlink @link_wolfvis_a22.lnk
if errorlevel 1 goto :fail

echo === Copying WOLFA22.EXE to cd_root_a22 ===
if not exist ..\cd_root_a22 mkdir ..\cd_root_a22
copy /y ..\build\WOLFA22.EXE ..\cd_root_a22\WOLFA22.EXE >nul
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA22.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
