@echo off
setlocal
if not defined WATCOM set WATCOM=%~dp0..\tools\OW
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\win

cd /d "%~dp0"
if not exist ..\build mkdir ..\build

echo === Compiling wolfvis_a13_1.c ===
rem -ox enables aggressive optimizations (-obmiler: branch, math, inline,
rem loop, expand, order). -s drops stack-overflow checks (saves cycles
rem in inner loops). Project ran -od (no optimization) until A.13.1 — adding
rem -ox alone is the biggest perf lever before any algorithmic change.
wcc -zq -bt=windows -ml -ox -s -fo=..\build\wolfvis_a13_1.obj wolfvis_a13_1.c
if errorlevel 1 goto :fail

echo === Linking WOLFA131.EXE ===
wlink @link_wolfvis_a13_1.lnk
if errorlevel 1 goto :fail

echo BUILD OK
dir ..\build\WOLFA131.EXE
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
