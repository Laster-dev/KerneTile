@echo off
chcp 65001 >nul
title KerneTile - WSL1 安装与 Linux 环境配置助手

:: 检查是否具有管理员权限
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [信息] 正在请求管理员权限以配置 Windows 子系统 (WSL1)...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo ==============================================================================
echo              KerneTile / KTUI WSL1 (Linux 子系统) 一键安装助手
echo ==============================================================================
echo.
echo [1/3] 正在启用 Windows 功能: 适用于 Linux 的 Windows 子系统 (WSL)...
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart
if %errorlevel% neq 0 (
    echo [错误] 启用 WSL 功能失败，请确保以系统管理员身份运行。
    pause
    exit /b %errorlevel%
)

echo.
echo [2/3] 设置 WSL 默认架构版本为 WSL 1...
wsl.exe --set-default-version 1 >nul 2>&1

echo.
echo [3/3] 检查并下载安装 Ubuntu Linux 分发版...
echo (正在启动 Ubuntu 安装，首次安装如果系统要求重启，请重启后重新打开该脚本完成初始化)
wsl.exe --install -d Ubuntu --web-download

echo.
echo ==============================================================================
echo 恭喜！WSL1 Linux 环境配置指令已下发完成。
echo 如果系统提示需要重启电脑，请重启后再次打开终端，输入:
echo     wsl
echo 即可进入 Linux 终端，然后运行:
echo     cd /mnt/d/KerneTile
echo     bash scripts/setup_linux_env.sh
echo 即可在 Linux 平台无缝一键编译 KerneTile 引擎并运行 35+ 项跨平台自动化测试！
echo ==============================================================================
echo.
pause
