@echo off
rem Builds and runs tools\progress_test.c (plays every task with several seeds).
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>nul
cd /d "%~dp0.."
if not exist build\test mkdir build\test
cl /nologo /O2 /W3 /utf-8 /Isrc /Fobuild\test\ /Fe:build\test\progress_test.exe tools\progress_test.c src\game.c || exit /b 1
build\test\progress_test.exe 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
