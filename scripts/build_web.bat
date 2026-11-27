@echo off
setlocal

echo ==============================================================================
echo  KerneTile / KTUI Web (WebAssembly) Build and Package Script
echo ==============================================================================

where cmake >nul 2>&1
if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VSWHERE%" (
        for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
            if exist "%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
                set "PATH=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
            )
        )
    )
)

where emcc >nul 2>&1
if errorlevel 1 (
    if defined EMSDK (
        if exist "%EMSDK%\emsdk_env.bat" (
            call "%EMSDK%\emsdk_env.bat" >nul 2>&1
        )
    )
)

where emcc >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Emscripten compiler emcc not found.
    echo Please install emsdk and set the EMSDK environment variable.
    exit /b 1
)

echo [1/3] Configuring KerneTile for Web...
set "ROOT_DIR=%~dp0.."
cd /d "%ROOT_DIR%"

call emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

echo [2/3] Compiling KerneTile and KTUI WebAssembly artifacts...
cmake --build build-web --target ktui_gallery
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [3/3] Verification: running smoke test via Node.js...
cd build-web
node ktui_gallery.js --capture-only
cd /d "%ROOT_DIR%"

echo ==============================================================================
echo  KerneTile KTUI Web Build Complete.
echo  Artifacts generated:
echo    - build-web/ktui_gallery.html
echo    - build-web/ktui_gallery.js
echo    - build-web/ktui_gallery.wasm
echo.
echo  To preview in browser, run:
echo    scripts\run_web.bat
echo ==============================================================================
