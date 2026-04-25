@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win
set EDPATH=%WATCOM%\eddat

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wxbridge.c (Win16, large model) ===
wcc -zq -bt=windows -ml -fo=..\build\wxbridge.obj wxbridge.c
if errorlevel 1 goto :fail

echo === Linking WXBRIDGE.EXE ===
wlink @link_wxbridge.lnk
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WXBRIDGE.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
