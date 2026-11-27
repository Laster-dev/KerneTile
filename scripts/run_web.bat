@echo off
setlocal
echo ==============================================================================
echo  KerneTile / KTUI Web (WebAssembly) 本地快速启动器
echo ==============================================================================

set "ROOT_DIR=%~dp0"
cd /d "%ROOT_DIR%"

if not exist "build-web\ktui_gallery.html" (
    echo [提示] 未检测到 Web 产物，正在自动执行首次编译构建...
    call scripts\build_web.bat
    if errorlevel 1 (
        echo [错误] 编译失败，请检查控制台输出。
        pause
        exit /b 1
    )
)

echo.
echo [1/2] 正在启动本地 Web 预览服务 (端口: 8080)...
echo [2/2] 正在打开系统默认浏览器...
echo.
echo ==============================================================================
echo  Web 界面正在运行中！
echo  地址: http://localhost:8080/ktui_gallery.html
echo  提示: 保持此窗口打开即可访问，按 Ctrl+C 或直接关闭本窗口可停止服务。
echo ==============================================================================
echo.

:: 自动调用浏览器打开网页
start http://localhost:8080/ktui_gallery.html

:: 启动本地静态 HTTP 服务 (优先系统 Python，次选 Emscripten 预置 Python)
where python >nul 2>&1
if not errorlevel 1 (
    python -m http.server 8080 --directory build-web
    exit /b 0
)

if exist "C:\Program Files\Unity\Hub\Editor\6000.6.6f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten\python\python.exe" (
    "C:\Program Files\Unity\Hub\Editor\6000.6.6f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten\python\python.exe" -m http.server 8080 --directory build-web
    exit /b 0
)

:: 后备方案：调用 Windows 内置 PowerShell 启动极简 HTTP 服务
powershell -NoProfile -Command "$listener = New-Object System.Net.HttpListener; $listener.Prefixes.Add('http://localhost:8080/'); $listener.Start(); Write-Host '服务已通过内置 PowerShell 启动 (http://localhost:8080/)...'; while($listener.IsListening){ $ctx = $listener.GetContext(); $f = Join-Path 'build-web' ($ctx.Request.Url.LocalPath.TrimStart('/')); if(-not (Test-Path $f -PathType Leaf)){ $f = 'build-web/ktui_gallery.html' }; if(Test-Path $f -PathType Leaf){ if($f.EndsWith('.wasm')){ $ctx.Response.ContentType = 'application/wasm' } elseif($f.EndsWith('.js')){ $ctx.Response.ContentType = 'application/javascript' }; $b = [System.IO.File]::ReadAllBytes($f); $ctx.Response.ContentLength64 = $b.Length; $ctx.Response.OutputStream.Write($b, 0, $b.Length) } else { $ctx.Response.StatusCode = 404 }; $ctx.Response.Close() }"
