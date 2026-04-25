@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win
set EDPATH=%WATCOM%\eddat

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_dd.c (Win16 DISPDIB experiment) ===
wcc -zq -bt=windows -ml -fo=..\build\wolfvis_dd.obj wolfvis_dd.c
if errorlevel 1 goto :fail

echo === Linking WOLFVDD.EXE ===
wlink @link_wolfvis_dd.lnk
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFVDD.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
