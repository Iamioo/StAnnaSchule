@echo off
rem Builds bin\StAnna.exe (32-bit, no C runtime) with Visual Studio's compiler.
setlocal
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat
if not exist "%VCVARS%" (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VCVARS=%%i\VC\Auxiliary\Build\vcvarsall.bat
)
call "%VCVARS%" x86 >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
if not exist bin mkdir bin

cl /nologo /c /O2 /GS- /Gs999999 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fobuild\ src\game.c src\plat_win32.c || exit /b 1
if exist build\stanna.ico (
  rc /nologo /fo build\stanna.res src\stanna.rc || exit /b 1
  set RES=build\stanna.res
)
link /nologo /NODEFAULTLIB /ENTRY:SaEntry /SUBSYSTEM:WINDOWS,5.01 /OPT:REF /OPT:ICF ^
  /OUT:bin\StAnna.exe build\game.obj build\plat_win32.obj %RES% ^
  kernel32.lib user32.lib gdi32.lib winmm.lib || exit /b 1
rem optional: let Windows 95/98/NT4 load it too
where python >nul 2>nul && python -I tools\pe_oldwin.py bin\StAnna.exe
echo Built bin\StAnna.exe
