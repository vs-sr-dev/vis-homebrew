@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a23.c ===
wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a23.obj wolfvis_a23.c
if errorlevel 1 goto :fail

echo === Linking WOLFA23.EXE ===
wlink @link_wolfvis_a23.lnk
if errorlevel 1 goto :fail

echo === Copying WOLFA23.EXE to cd_root_a23 ===
if not exist ..\cd_root_a23 mkdir ..\cd_root_a23
copy /y ..\build\WOLFA23.EXE ..\cd_root_a23\WOLFA23.EXE >nul
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA23.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
