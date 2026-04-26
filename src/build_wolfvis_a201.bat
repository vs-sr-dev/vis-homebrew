@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a201.c ===
wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a201.obj wolfvis_a201.c
if errorlevel 1 goto :fail

echo === Linking WOLFA201.EXE ===
wlink @link_wolfvis_a201.lnk
if errorlevel 1 goto :fail

echo === Copying WOLFA201.EXE to cd_root_a201 ===
if not exist ..\cd_root_a201 mkdir ..\cd_root_a201
copy /y ..\build\WOLFA201.EXE ..\cd_root_a201\WOLFA201.EXE >nul
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA201.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
