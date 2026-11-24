@echo off
setlocal
echo ==============================================================================
echo  KerneTile / KTUI Web (WebAssembly) GitHub Pages Deployment Script
echo ==============================================================================

set "ROOT_DIR=%~dp0.."
cd /d "%ROOT_DIR%"

if not exist "build-web\ktui_gallery.html" (
    echo [1/3] Building WebAssembly artifacts...
    call scripts\build_web.bat
    if errorlevel 1 (
        echo [Error] Web build failed.
        exit /b 1
    )
)

echo.
python scripts\deploy_gh_pages.py
if errorlevel 1 (
    echo [Error] Packaging for GitHub Pages failed.
    exit /b 1
)

echo.
echo ==============================================================================
echo  正在推�?gh-pages 分支�?GitHub 远程仓库 (origin)...
echo ==============================================================================
git push origin gh-pages

if errorlevel 1 (
    echo.
    echo [提示] 推送遇到权限验证或网络中断时，可稍后在终端手动执行:
    echo        git push origin gh-pages
) else (
    echo.
    echo [成功] gh-pages 已推送至 GitHub�?
    echo 请访问仓�?Settings -> Pages 进行确认:
    echo   https://github.com/Laster-dev/KerneTile/settings/pages
)
