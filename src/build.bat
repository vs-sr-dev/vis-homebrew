@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win
set EDPATH=%WATCOM%\eddat
set WIPFC=%WATCOM%\wipfc

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling hello.c (Win16, large model) ===
wcc -zq -bt=windows -ml -fo=..\build\hello.obj hello.c
if errorlevel 1 goto :fail

echo === Linking HELLO.EXE ===
wlink @link.lnk
if errorlevel 1 goto :fail

echo.
echo BUILD OK
dir ..\build\HELLO.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
