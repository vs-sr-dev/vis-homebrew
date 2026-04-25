@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h
set EDPATH=%WATCOM%\eddat

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling dosh.c (DOS 16-bit, small model) ===
wcc -zq -bt=dos -ms -fo=..\build\dosh.obj dosh.c
if errorlevel 1 goto :fail

echo === Linking DOSH.EXE ===
wlink @link_dosh.lnk
if errorlevel 1 goto :fail

echo.
echo BUILD OK
dir ..\build\DOSH.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
