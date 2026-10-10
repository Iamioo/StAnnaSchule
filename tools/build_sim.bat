@echo off
rem Builds and runs the headless balance test (tools\sim.c).
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul || exit /b 1
cd /d "%~dp0.."
if not exist build\sim mkdir build\sim
cl /nologo /O2 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fobuild\sim\ /Febuild\sim\sim.exe src\game.c tools\sim.c || exit /b 1
build\sim\sim.exe %1 %2
