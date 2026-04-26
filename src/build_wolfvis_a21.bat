@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a21.c ===
wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a21.obj wolfvis_a21.c
if errorlevel 1 goto :fail

echo === Linking WOLFA21.EXE ===
wlink @link_wolfvis_a21.lnk
if errorlevel 1 goto :fail

echo === Copying WOLFA21.EXE to cd_root_a21 ===
if not exist ..\cd_root_a21 mkdir ..\cd_root_a21
copy /y ..\build\WOLFA21.EXE ..\cd_root_a21\WOLFA21.EXE >nul
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA21.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
