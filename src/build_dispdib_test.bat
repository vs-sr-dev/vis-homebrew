@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling dispdib_test.c ===
wcc -zq -bt=windows -ml -ox -s -fo=..\build\dispdib_test.obj dispdib_test.c
if errorlevel 1 goto :fail

echo === Linking DDTEST.EXE ===
wlink @link_dispdib_test.lnk
if errorlevel 1 goto :fail

echo === Copying DDTEST.EXE to cd_root_ddtest ===
if not exist ..\cd_root_ddtest mkdir ..\cd_root_ddtest
copy /y ..\build\DDTEST.EXE ..\cd_root_ddtest\DDTEST.EXE >nul
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\DDTEST.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
