@echo off
setlocal

echo ==============================================================================
echo  KerneTile / KTUI Web (WebAssembly) Build and Package Script
echo ==============================================================================

where cmake >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
    )
)

where emcc >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files\Unity\Hub\Editor\6000.6.6f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten\emscripten\emcc.bat" (
        set "EMSDK_DIR=C:\Program Files\Unity\Hub\Editor\6000.6.6f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten"
        set "EM_CONFIG=C:\Program Files\Unity\Hub\Editor\6000.6.6f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten\.emscripten"
        set "PATH=C:\Program Files\Unity\Hub\Editor\6000.6.6f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten\emscripten;C:\Program Files\Unity\Hub\Editor\6000.6.6f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten\node;C:\Program Files\Unity\Hub\Editor\6000.6.6f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten\python;%PATH%"
    ) else if defined EMSDK (
        call "%EMSDK%\emsdk_env.bat" >nul 2>&1
    )
)

where emcc >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Emscripten compiler emcc not found.
    echo Please install emsdk or set EMSDK environment variable.
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
cmake --build build-web --target ktui_studio_demo
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [3/3] Verification: running smoke test via Node.js...
cd build-web
node ktui_studio_demo.js --capture-only
cd /d "%ROOT_DIR%"

echo ==============================================================================
echo  KerneTile KTUI Web Build Complete.
echo  Artifacts generated:
echo    - build-web/ktui_studio_demo.html
echo    - build-web/ktui_studio_demo.js
echo    - build-web/ktui_studio_demo.wasm
echo.
echo  To preview in browser, run:
echo    python -m http.server 8080 --directory build-web
echo  Then navigate to: http://localhost:8080/ktui_studio_demo.html
echo ==============================================================================
