@echo off
rem Builds internal.dll (ESP + executor merged) and injector.exe
rem Run from any normal cmd or double-click - VS env is set up automatically.

rem --- Auto-setup x86 MSVC environment if cl.exe isn't already on PATH ---
where cl >nul 2>&1
if not errorlevel 1 goto :have_cl

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_cl

for /f "usebackq delims=" %%i in (
    `"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set "VS_INSTALL=%%i"

if not defined VS_INSTALL goto :no_cl

set "VCVARS=%VS_INSTALL%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" goto :no_cl

call "%VCVARS%" x86 >nul
if errorlevel 1 goto :no_cl

:have_cl

set "DXSDK=C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)"
if not exist "%DXSDK%\Include\d3d9.h" goto :no_dx

echo.
echo ==== Building internal.dll ====
cl /nologo /EHa /O2 /MT /LD /Fe:internal.dll ^
   /D_CRT_SECURE_NO_WARNINGS ^
   /I. /Ivendor ^
   /I"%DXSDK%\Include" ^
   src\dllmain.cpp src\hooks.cpp src\esp.cpp src\menu.cpp src\executor.cpp ^
   vendor\imgui\imgui.cpp ^
   vendor\imgui\imgui_draw.cpp ^
   vendor\imgui\imgui_tables.cpp ^
   vendor\imgui\imgui_widgets.cpp ^
   vendor\imgui\imgui_impl_dx9.cpp ^
   vendor\imgui\imgui_impl_win32.cpp ^
   /link /DLL /MACHINE:X86 ^
   /LIBPATH:"%DXSDK%\Lib\x86" ^
   user32.lib gdi32.lib kernel32.lib d3d9.lib
if errorlevel 1 goto :fail

echo.
echo ==== Building injector.exe ====
cl /nologo /EHsc /O2 /MT /Fe:injector.exe ^
   /D_CRT_SECURE_NO_WARNINGS ^
   injector\injector.cpp ^
   /link /MACHINE:X86 kernel32.lib user32.lib
if errorlevel 1 goto :fail

del src\*.obj src\*.exp injector\*.obj *.obj *.exp *.lib 2>nul
echo.
echo [OK] Built internal.dll and injector.exe
goto :end

:no_cl
echo [FAIL] Could not find MSVC. Install Visual Studio with the C++ workload.
goto :end

:no_dx
echo [FAIL] DirectX SDK not found at %DXSDK%
goto :end

:fail
del *.obj *.exp *.lib 2>nul
echo.
echo [FAIL] Build failed.

:end
pause
