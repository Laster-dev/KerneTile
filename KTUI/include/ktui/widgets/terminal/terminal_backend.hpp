#pragma once

#include <ktui/core/fn.hpp>
#include <ktui/core/light_mutex.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ktui {

// ==============================================================================
// 终端可插拔后端抽象接口 (ITerminalBackend)
// 对标 Microsoft Windows Terminal 的 ITerminalConnection，可自由对接任意后端：
//   - Windows ConPTY (PowerShell / CMD / WSL / Git Bash / 任意 CLI 进程)
//   - Linux/macOS POSIX PTY (forkpty / openpty)
//   - SSH / Telnet / WebSocket / 串口 (COM/UART) / Docker Exec / 命名管道
//   - 自定义嵌入式 REPL / 调试控制台 / 单元测试 Mock 后端
// 线程安全约定：on_output / on_exit 可从任意后台线程安全调用，RenderTerminal 自动跨线程调度至 UI 线程。
// ==============================================================================
class ITerminalBackend {
public:
    virtual ~ITerminalBackend() = default;

    // 启动后端会话 (cols x rows 为初始终端字符网格尺寸)
    virtual bool start(int cols, int rows) = 0;

    // 向后端写入用户输入字节流 (UTF-8 文本或 VT 控制序列)
    virtual void write(std::string_view utf8_bytes) = 0;

    // 通知后端终端窗口尺寸发生变化 (如触发 ConPTY ResizePseudoConsole / SIGWINCH)
    virtual void resize(int cols, int rows) = 0;

    // 优雅关闭后端连接并释放底层句柄/线程
    virtual void close() = 0;

    // 查询后端是否仍在运行
    [[nodiscard]] virtual bool is_running() const noexcept = 0;

    // 后端展示名称 (供状态栏/标签页展示)
    [[nodiscard]] virtual std::string backend_name() const { return "Custom Backend"; }

    // 由 RenderTerminal 绑定的输出/退出回调 (允许后台线程直接调用)
    Fn<void(std::string_view)> on_output;
    Fn<void(int exit_code)> on_exit;
};

// ==============================================================================
// 1. 通用回调适配器后端 (CallbackTerminalBackend)
// 无需继承子类，只需传入 lambda 即可对接 SSH、WebSocket、串口、管道或自定义协议，
// 随时调用 feed_output(bytes) 即可将远端字节流注入终端渲染！
// ==============================================================================
class CallbackTerminalBackend : public ITerminalBackend {
public:
    struct Handlers {
        std::string name{"Custom Pipe"};
        Fn<bool(int cols, int rows)> on_start;
        Fn<void(std::string_view)> on_write;
        Fn<void(int cols, int rows)> on_resize;
        Fn<void()> on_close;
    };

    CallbackTerminalBackend() = default;
    explicit CallbackTerminalBackend(Handlers h) : m_handlers(std::move(h)) {}

    bool start(int cols, int rows) override {
        m_running.store(true, std::memory_order_release);
        if (m_handlers.on_start) {
            if (!m_handlers.on_start(cols, rows)) {
                m_running.store(false, std::memory_order_release);
                return false;
            }
        }
        return true;
    }

    void write(std::string_view utf8_bytes) override {
        if (!m_running.load(std::memory_order_acquire)) return;
        if (m_handlers.on_write) {
            m_handlers.on_write(utf8_bytes);
        }
    }

    void resize(int cols, int rows) override {
        if (!m_running.load(std::memory_order_acquire)) return;
        if (m_handlers.on_resize) {
            m_handlers.on_resize(cols, rows);
        }
    }

    void close() override {
        bool was = m_running.exchange(false, std::memory_order_acq_rel);
        if (was && m_handlers.on_close) {
            m_handlers.on_close();
        }
    }

    [[nodiscard]] bool is_running() const noexcept override {
        return m_running.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::string backend_name() const override {
        return m_handlers.name;
    }

    // 外部任意线程调用：向终端推入 VT/UTF-8 输出流
    void feed_output(std::string_view utf8_bytes) {
        if (on_output && !utf8_bytes.empty()) {
            on_output(utf8_bytes);
        }
    }

    // 外部调用：通知后端已退出
    void notify_exit(int exit_code = 0) {
        m_running.store(false, std::memory_order_release);
        if (on_exit) on_exit(exit_code);
    }

private:
    Handlers m_handlers;
    std::atomic<bool> m_running{false};
};

// ==============================================================================
// 2. Windows 原生 ConPTY 伪终端后端 (ConPtyTerminalBackend)
// 与微软 Windows Terminal 使用完全相同的 CreatePseudoConsole / ResizePseudoConsole API
// 动态解析 kernel32.dll 导出符号，零额外依赖，支持 PowerShell / CMD / WSL / 任意 CLI 程序
// ==============================================================================
#if defined(_WIN32)
class ConPtyTerminalBackend : public ITerminalBackend {
public:
    explicit ConPtyTerminalBackend(std::string command_line = "powershell.exe -NoLogo",
                                   std::string working_dir = "")
        : m_command_line(std::move(command_line)),
          m_working_dir(std::move(working_dir)) {}

    ~ConPtyTerminalBackend() override {
        close();
    }

    bool start(int cols, int rows) override {
        close();
        cols = (cols < 10) ? 80 : cols;
        rows = (rows < 4) ? 24 : rows;

        HMODULE k32 = ::GetModuleHandleW(L"kernel32.dll");
        if (!k32) return false;

        using CreatePseudoConsoleFn = HRESULT(WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
        using ResizePseudoConsoleFn = HRESULT(WINAPI*)(HPCON, COORD);
        using ClosePseudoConsoleFn  = void(WINAPI*)(HPCON);

        auto pCreate = reinterpret_cast<CreatePseudoConsoleFn>(::GetProcAddress(k32, "CreatePseudoConsole"));
        m_pResize    = reinterpret_cast<ResizePseudoConsoleFn>(::GetProcAddress(k32, "ResizePseudoConsole"));
        m_pClose     = reinterpret_cast<ClosePseudoConsoleFn>(::GetProcAddress(k32, "ClosePseudoConsole"));
        if (!pCreate || !m_pResize || !m_pClose) {
            emit_error("\x1b[31m[ConPTY] 当前 Windows 版本不支持 CreatePseudoConsole (需 Win10 1809+)\x1b[0m\r\n");
            return false;
        }

        HANDLE pty_in_read = nullptr, pty_in_write = nullptr;
        HANDLE pty_out_read = nullptr, pty_out_write = nullptr;
        if (!::CreatePipe(&pty_in_read, &pty_in_write, nullptr, 0) ||
            !::CreatePipe(&pty_out_read, &pty_out_write, nullptr, 0)) {
            if (pty_in_read) ::CloseHandle(pty_in_read);
            if (pty_in_write) ::CloseHandle(pty_in_write);
            if (pty_out_read) ::CloseHandle(pty_out_read);
            if (pty_out_write) ::CloseHandle(pty_out_write);
            return false;
        }

        COORD size{};
        size.X = static_cast<SHORT>(cols);
        size.Y = static_cast<SHORT>(rows);

        HPCON hpc = nullptr;
        HRESULT hr = pCreate(size, pty_in_read, pty_out_write, 0, &hpc);
        // ConPTY 内部已复制句柄，父进程侧可立即关闭子端句柄
        ::CloseHandle(pty_in_read);
        ::CloseHandle(pty_out_write);
        if (FAILED(hr) || !hpc) {
            ::CloseHandle(pty_in_write);
            ::CloseHandle(pty_out_read);
            emit_error("\x1b[31m[ConPTY] CreatePseudoConsole 失败\x1b[0m\r\n");
            return false;
        }

        SIZE_T attr_size = 0;
        ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
        std::vector<uint8_t> attr_buf(attr_size);
        auto* attr_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
        if (!::InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_size) ||
            !::UpdateProcThreadAttribute(attr_list, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                         hpc, sizeof(HPCON), nullptr, nullptr)) {
            ::DeleteProcThreadAttributeList(attr_list);
            m_pClose(hpc);
            ::CloseHandle(pty_in_write);
            ::CloseHandle(pty_out_read);
            return false;
        }

        STARTUPINFOEXW si{};
        si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        si.lpAttributeList = attr_list;

        std::wstring cmd_w = utf8_to_wide(m_command_line);
        std::wstring cwd_w = utf8_to_wide(m_working_dir);
        PROCESS_INFORMATION pi{};
        BOOL ok = ::CreateProcessW(
            nullptr,
            cmd_w.data(),
            nullptr,
            nullptr,
            FALSE,
            EXTENDED_STARTUPINFO_PRESENT,
            nullptr,
            cwd_w.empty() ? nullptr : cwd_w.c_str(),
            &si.StartupInfo,
            &pi);

        ::DeleteProcThreadAttributeList(attr_list);

        if (!ok) {
            m_pClose(hpc);
            ::CloseHandle(pty_in_write);
            ::CloseHandle(pty_out_read);
            emit_error("\x1b[31m[ConPTY] 启动子进程失败: " + m_command_line + "\x1b[0m\r\n");
            return false;
        }

        ::CloseHandle(pi.hThread);
        {
            LightLock lk(m_io_mutex);
            m_hpc = hpc;
            m_pipe_in_write = pty_in_write;
            m_pipe_out_read = pty_out_read;
            m_process_handle = pi.hProcess;
        }
        m_running.store(true, std::memory_order_release);

        // 启动独立后台读取线程，实时流式推送 ConPTY 输出
        m_reader_thread = std::thread([this]() {
            char buf[4096];
            while (m_running.load(std::memory_order_acquire)) {
                HANDLE hread = nullptr;
                {
                    LightLock lk(m_io_mutex);
                    hread = m_pipe_out_read;
                }
                if (!hread) break;

                DWORD nread = 0;
                BOOL rok = ::ReadFile(hread, buf, sizeof(buf), &nread, nullptr);
                if (!rok || nread == 0) {
                    break;
                }
                if (on_output) {
                    on_output(std::string_view(buf, static_cast<size_t>(nread)));
                }
            }

            DWORD exit_code = 0;
            HANDLE hproc = nullptr;
            {
                LightLock lk(m_io_mutex);
                hproc = m_process_handle;
            }
            if (hproc) {
                ::WaitForSingleObject(hproc, 200);
                ::GetExitCodeProcess(hproc, &exit_code);
            }
            bool was_running = m_running.exchange(false, std::memory_order_acq_rel);
            if (was_running && on_exit) {
                on_exit(static_cast<int>(exit_code));
            }
        });

        return true;
    }

    void write(std::string_view utf8_bytes) override {
        if (!m_running.load(std::memory_order_acquire) || utf8_bytes.empty()) return;
        LightLock lk(m_io_mutex);
        if (!m_pipe_in_write) return;
        const char* p = utf8_bytes.data();
        size_t rem = utf8_bytes.size();
        while (rem > 0) {
            DWORD written = 0;
            if (!::WriteFile(m_pipe_in_write, p, static_cast<DWORD>(rem), &written, nullptr) || written == 0) {
                break;
            }
            p += written;
            rem -= written;
        }
    }

    void resize(int cols, int rows) override {
        if (!m_running.load(std::memory_order_acquire)) return;
        if (cols < 2 || rows < 2) return;
        LightLock lk(m_io_mutex);
        if (m_hpc && m_pResize) {
            COORD sz{};
            sz.X = static_cast<SHORT>(cols);
            sz.Y = static_cast<SHORT>(rows);
            m_pResize(m_hpc, sz);
        }
    }

    void close() override {
        m_running.store(false, std::memory_order_release);

        HPCON hpc = nullptr;
        HANDLE h_in = nullptr, h_out = nullptr, h_proc = nullptr;
        {
            LightLock lk(m_io_mutex);
            hpc = m_hpc; m_hpc = nullptr;
            h_in = m_pipe_in_write; m_pipe_in_write = nullptr;
            h_out = m_pipe_out_read; m_pipe_out_read = nullptr;
            h_proc = m_process_handle; m_process_handle = nullptr;
        }

        // 关闭 ConPTY 会向子进程发送退出信号并立即唤醒阻塞的 ReadFile
        if (hpc && m_pClose) m_pClose(hpc);
        if (h_in) ::CloseHandle(h_in);
        if (h_out) ::CloseHandle(h_out);
        if (m_reader_thread.joinable()) {
            m_reader_thread.join();
        }
        if (h_proc) {
            if (::WaitForSingleObject(h_proc, 120) == WAIT_TIMEOUT) {
                ::TerminateProcess(h_proc, 0);
            }
            ::CloseHandle(h_proc);
        }
    }

    [[nodiscard]] bool is_running() const noexcept override {
        return m_running.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::string backend_name() const override {
        return "ConPTY (" + m_command_line + ")";
    }

private:
    static std::wstring utf8_to_wide(const std::string& s) {
        if (s.empty()) return {};
        int len = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (len <= 0) return {};
        std::wstring out(static_cast<size_t>(len), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
        return out;
    }

    void emit_error(const std::string& msg) {
        if (on_output) on_output(msg);
    }

    std::string m_command_line;
    std::string m_working_dir;
    LightMutex m_io_mutex;
    HPCON m_hpc{nullptr};
    HANDLE m_pipe_in_write{nullptr};
    HANDLE m_pipe_out_read{nullptr};
    HANDLE m_process_handle{nullptr};
    HRESULT(WINAPI* m_pResize)(HPCON, COORD){nullptr};
    void(WINAPI* m_pClose)(HPCON){nullptr};
    std::atomic<bool> m_running{false};
    std::thread m_reader_thread;
};
#endif // _WIN32

// ==============================================================================
// 3. 零依赖内置交互演示后端 (DemoShellBackend)
// 跨平台可用（含 Headless / Android / WASM），内置全彩 ANSI / TrueColor / OSC 9;4 演示命令
// ==============================================================================
class DemoShellBackend : public ITerminalBackend {
public:
    bool start(int cols, int rows) override {
        m_cols = cols;
        m_rows = rows;
        m_running = true;
        m_line.clear();
        m_esc_state = 0;

        emit("\x1b]0;KerneTile Terminal — DemoShell\x07");
        emit("\x1b[1;36mKerneTile Terminal\x1b[0m \x1b[90m(Windows Terminal Compatible • VT100/xterm-256/TrueColor)\x1b[0m\r\n");
        emit("输入 \x1b[1;33mhelp\x1b[0m 查看内置命令，或点击上方工具栏切换真实 \x1b[1;32mConPTY (PowerShell / CMD)\x1b[0m 后端。\r\n\r\n");
        prompt();
        return true;
    }

    void write(std::string_view utf8_bytes) override {
        if (!m_running) return;
        for (size_t i = 0; i < utf8_bytes.size(); ++i) {
            unsigned char ch = static_cast<unsigned char>(utf8_bytes[i]);
            if (m_esc_state == 1) {
                if (ch == '[') { m_esc_state = 2; continue; }
                m_esc_state = 0;
                continue;
            }
            if (m_esc_state == 2) {
                if (ch >= 0x40 && ch <= 0x7E) {
                    m_esc_state = 0;
                    if (ch == 'A') history_nav(-1);      // Up
                    else if (ch == 'B') history_nav(+1); // Down
                }
                continue;
            }
            if (ch == 0x1B) { m_esc_state = 1; continue; }
            if (ch == 0x03) { // Ctrl+C
                m_line.clear();
                emit("^C\r\n");
                prompt();
                continue;
            }
            if (ch == 0x0C) { // Ctrl+L
                emit("\x1b[2J\x1b[H");
                prompt();
                emit(m_line);
                continue;
            }
            if (ch == '\r' || ch == '\n') {
                emit("\r\n");
                std::string cmd = m_line;
                m_line.clear();
                if (!cmd.empty()) {
                    m_history.push_back(cmd);
                    m_hist_idx = static_cast<int>(m_history.size());
                }
                exec_command(cmd);
                if (m_running) prompt();
                continue;
            }
            if (ch == 0x7F || ch == 0x08) { // Backspace
                if (!m_line.empty()) {
                    // 完整退格 UTF-8 字符
                    size_t pop_n = 1;
                    while (pop_n < m_line.size() &&
                           (static_cast<unsigned char>(m_line[m_line.size() - pop_n]) & 0xC0u) == 0x80u) {
                        ++pop_n;
                    }
                    bool wide = (pop_n >= 3);
                    m_line.resize(m_line.size() - pop_n);
                    emit(wide ? "\b\b  \b\b" : "\b \b");
                }
                continue;
            }
            if (ch == '\t') {
                // 简单命令补全
                static constexpr const char* kCmds[] = {"help", "colors", "neofetch", "ls", "progress", "clear", "echo", "date", "exit"};
                for (const char* c : kCmds) {
                    std::string_view sv(c);
                    if (!m_line.empty() && sv.size() > m_line.size() && sv.substr(0, m_line.size()) == m_line) {
                        std::string_view rest = sv.substr(m_line.size());
                        m_line.append(rest);
                        emit(rest);
                        break;
                    }
                }
                continue;
            }
            if (ch >= 0x20) {
                m_line.push_back(static_cast<char>(ch));
                emit(std::string_view(reinterpret_cast<const char*>(&ch), 1));
            }
        }
    }

    void resize(int cols, int rows) override {
        m_cols = cols;
        m_rows = rows;
    }

    void close() override {
        m_running = false;
    }

    [[nodiscard]] bool is_running() const noexcept override { return m_running; }
    [[nodiscard]] std::string backend_name() const override { return "DemoShell (Built-in)"; }

private:
    void emit(std::string_view s) {
        if (on_output) on_output(s);
    }

    void prompt() {
        emit("\x1b[1;32muser@kernetile\x1b[0m:\x1b[1;34m~/KerneTile\x1b[0m$ ");
    }

    void history_nav(int dir) {
        if (m_history.empty()) return;
        int next = m_hist_idx + dir;
        if (next < 0) next = 0;
        if (next > static_cast<int>(m_history.size())) next = static_cast<int>(m_history.size());
        m_hist_idx = next;
        emit("\r\x1b[K");
        prompt();
        m_line = (m_hist_idx < static_cast<int>(m_history.size())) ? m_history[static_cast<size_t>(m_hist_idx)] : "";
        emit(m_line);
    }

    void exec_command(const std::string& raw) {
        std::string cmd = raw;
        while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());
        while (!cmd.empty() && cmd.back() == ' ') cmd.pop_back();
        if (cmd.empty()) return;

        if (cmd == "help") {
            emit("\x1b[1m内置演示命令:\x1b[0m\r\n");
            emit("  \x1b[36mcolors\x1b[0m     测试 ANSI 16色 / 256色 / 24-bit TrueColor 渐变与 SGR 样式\r\n");
            emit("  \x1b[36mneofetch\x1b[0m   展示系统与 KerneTile 引擎概览\r\n");
            emit("  \x1b[36mls\x1b[0m         列出当前项目目录结构\r\n");
            emit("  \x1b[36mprogress\x1b[0m   演示 Windows Terminal OSC 9;4 顶部进度条\r\n");
            emit("  \x1b[36mecho ...\x1b[0m   回显任意文本（支持中文与 UTF-8）\r\n");
            emit("  \x1b[36mdate\x1b[0m       显示当前本地时间\r\n");
            emit("  \x1b[36mclear\x1b[0m      清屏 (或按 Ctrl+L)\r\n");
            emit("  \x1b[36mexit\x1b[0m       结束当前 DemoShell 会话\r\n");
        } else if (cmd == "clear") {
            emit("\x1b[2J\x1b[H");
        } else if (cmd == "ls") {
            emit("\x1b[1;34mKTUI/\x1b[0m  \x1b[1;34minclude/\x1b[0m  \x1b[1;34msrc/\x1b[0m  \x1b[1;34mexamples/\x1b[0m  \x1b[1;34mtests/\x1b[0m  \x1b[32mCMakeLists.txt\x1b[0m  README.md\r\n");
        } else if (cmd == "colors") {
            emit("\x1b[1mSGR 样式:\x1b[0m \x1b[1mBold\x1b[0m  \x1b[2mDim\x1b[0m  \x1b[3mItalic\x1b[0m  \x1b[4mUnderline\x1b[0m  \x1b[7mInverse\x1b[0m  \x1b[9mStrike\x1b[0m\r\n");
            emit("\x1b[1mANSI 16色:\x1b[0m ");
            for (int i = 0; i < 8; ++i) emit("\x1b[" + std::to_string(40 + i) + "m   \x1b[0m");
            emit(" ");
            for (int i = 0; i < 8; ++i) emit("\x1b[" + std::to_string(100 + i) + "m   \x1b[0m");
            emit("\r\n\x1b[1m24-bit TrueColor 连续色带:\x1b[0m\r\n");
            for (int i = 0; i < 48; ++i) {
                int r = (i < 16) ? (255 - i * 16) : (i >= 32 ? (i - 32) * 16 : 0);
                int g = (i < 16) ? (i * 16) : (i < 32 ? (255 - (i - 16) * 16) : 0);
                int b = (i >= 16 && i < 32) ? ((i - 16) * 16) : (i >= 32 ? (255 - (i - 32) * 16) : 0);
                char seq[48];
                std::snprintf(seq, sizeof(seq), "\x1b[48;2;%d;%d;%dm ", r, g, b);
                emit(seq);
            }
            emit("\x1b[0m\r\n");
        } else if (cmd == "neofetch") {
            emit("\x1b[1;36m   _  __                     _____ _ _      \x1b[0m  \x1b[1;33mOS:\x1b[0m Windows 11 / KerneTile\r\n");
            emit("\x1b[1;36m  | |/ /___ _ __ _ __   ___ |_   _(_) | ___ \x1b[0m  \x1b[1;33mEngine:\x1b[0m D3D12 Compute Tile Renderer\r\n");
            emit("\x1b[1;36m  | ' // _ \\ '__| '_ \\ / _ \\  | | | | |/ _ \\\x1b[0m  \x1b[1;33mWidget:\x1b[0m KTUI Terminal (VT500/ConPTY)\r\n");
            emit("\x1b[1;36m  | . \\  __/ |  | | | |  __/  | | | | |  __/\x1b[0m  \x1b[1;33mGrid:\x1b[0m " + std::to_string(m_cols) + "x" + std::to_string(m_rows) + " cells\r\n");
            emit("\x1b[1;36m  |_|\\_\\___|_|  |_| |_|\\___|  |_| |_|_|\\___|\x1b[0m  \x1b[1;33mBackend:\x1b[0m Pluggable ITerminalBackend\r\n");
        } else if (cmd == "progress") {
            emit("\x1b]9;4;1;68\x07已通过 \x1b[1;36mOSC 9;4;1;68\x1b[0m 设置 Windows Terminal 顶部进度条为 68%！\r\n");
            emit("再次输入 \x1b[33mprogress off\x1b[0m 可清除进度条。\r\n");
        } else if (cmd == "progress off") {
            emit("\x1b]9;4;0;0\x07已清除 OSC 9;4 进度条。\r\n");
        } else if (cmd == "date") {
            std::time_t t = std::time(nullptr);
            char buf[64]{};
            if (const std::tm* lt = std::localtime(&t)) {
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
            }
            emit(std::string(buf) + "\r\n");
        } else if (cmd.rfind("echo ", 0) == 0) {
            emit(cmd.substr(5) + "\r\n");
        } else if (cmd == "exit") {
            emit("\x1b[90m[DemoShell 已退出，退出码 0]\x1b[0m\r\n");
            m_running = false;
            if (on_exit) on_exit(0);
        } else {
            emit("\x1b[31m未知命令:\x1b[0m " + cmd + "  (输入 \x1b[33mhelp\x1b[0m 查看帮助)\r\n");
        }
    }

    int m_cols{80};
    int m_rows{24};
    bool m_running{false};
    int m_esc_state{0};
    std::string m_line;
    std::vector<std::string> m_history;
    int m_hist_idx{0};
};

} // namespace ktui
