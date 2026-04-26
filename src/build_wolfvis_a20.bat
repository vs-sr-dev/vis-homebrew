@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a20.c ===
wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a20.obj wolfvis_a20.c
if errorlevel 1 goto :fail

echo === Linking WOLFA20.EXE ===
wlink @link_wolfvis_a20.lnk
if errorlevel 1 goto :fail

echo === Copying WOLFA20.EXE to cd_root_a20 ===
if not exist ..\cd_root_a20 mkdir ..\cd_root_a20
copy /y ..\build\WOLFA20.EXE ..\cd_root_a20\WOLFA20.EXE >nul
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA20.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
