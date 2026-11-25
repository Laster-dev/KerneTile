#include <ktui/ktui.hpp>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

using namespace ktui;

#define KT_CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "CHECK FAILED at line %d: %s\n", __LINE__, #cond); std::exit(1); } } while (0)

int main() {
    // 1. VT500/ANSI 状态机：基础打印、光标定位 (CUP)、SGR 16色 / 256色 / 24-bit TrueColor、宽字符 CJK
    {
        TerminalBuffer buf(40, 10, 100);
        buf.feed("Hello \x1b[1;38;2;12;34;56mWorld\x1b[0m!");
        KT_CHECK(buf.cursor_row() == 0);
        KT_CHECK(buf.cursor_col() == 12);
        const auto& r0 = buf.row_at(0);
        KT_CHECK(r0.cells[0].codepoint == 'H');
        KT_CHECK(r0.cells[6].codepoint == 'W');
        KT_CHECK((r0.cells[6].attrs & TerminalAttr::Bold) != 0);
        KT_CHECK(r0.cells[6].fg.kind == TerminalColor::Kind::Rgb);
        KT_CHECK(r0.cells[6].fg.r == 12 && r0.cells[6].fg.g == 34 && r0.cells[6].fg.b == 56);
        KT_CHECK(r0.cells[11].codepoint == '!');
        KT_CHECK(r0.cells[11].attrs == TerminalAttr::None);

        // 中文宽字符应占 2 格（左半 width=2，右半 width=0）
        buf.feed("\r\n中文A");
        KT_CHECK(buf.cursor_row() == 1);
        KT_CHECK(buf.cursor_col() == 5);
        const auto& r1 = buf.row_at(1);
        KT_CHECK(r1.cells[0].width == 2);
        KT_CHECK(r1.cells[1].width == 0);
        KT_CHECK(r1.cells[2].width == 2);
        KT_CHECK(r1.cells[3].width == 0);
        KT_CHECK(r1.cells[4].codepoint == 'A');
    }

    // 2. 备用屏缓冲 (?1049h / ?1049l) + OSC 0 标题 + OSC 9;4 进度条 + DSR 6n 回报
    {
        TerminalBuffer buf(40, 8, 50);
        std::string last_title;
        std::string last_reply;
        TerminalProgress last_prog{};
        buf.on_title_changed = [&](const std::string& t) { last_title = t; };
        buf.on_reply = [&](std::string_view r) { last_reply = std::string(r); };
        buf.on_progress_changed = [&](TerminalProgress p) { last_prog = p; };

        buf.feed("MainScreen\x1b]0;KerneTile WT\x07\x1b]9;4;1;75\x07");
        KT_CHECK(last_title == "KerneTile WT");
        KT_CHECK(last_prog.state == TerminalProgressState::Normal && last_prog.value == 75);

        buf.feed("\x1b[?1049hAltContent");
        KT_CHECK(buf.is_alt_screen());
        KT_CHECK(buf.row_at(0).cells[0].codepoint == 'A');

        buf.feed("\x1b[6n");
        KT_CHECK(last_reply == "\x1b[1;11R");

        buf.feed("\x1b[?1049l");
        KT_CHECK(!buf.is_alt_screen());
        KT_CHECK(buf.row_at(0).cells[0].codepoint == 'M');
    }

    // 3. 回滚历史缓冲 + 选区文本提取 + 全文检索
    {
        TerminalBuffer buf(30, 4, 20);
        for (int i = 0; i < 8; ++i) {
            buf.feed("Line " + std::to_string(i) + " alpha\r\n");
        }
        KT_CHECK(buf.scrollback_count() > 0);
        auto matches = buf.find_all("ALPHA", false);
        KT_CHECK(matches.size() == 8);
        auto cs_matches = buf.find_all("ALPHA", true);
        KT_CHECK(cs_matches.empty());

        std::string line0 = buf.extract_text(TerminalCoord{0, 0}, TerminalCoord{11, 0}, false);
        KT_CHECK(line0 == "Line 0 alpha");
    }

    // 4. 可插拔 CallbackTerminalBackend + RenderTerminal 布局、渲染、输入、查找闭环
    {
        std::string captured_input;
        auto backend = std::make_shared<CallbackTerminalBackend>(CallbackTerminalBackend::Handlers{
            .name = "MockSSH",
            .on_write = [&](std::string_view s) { captured_input.append(s); }
        });

        auto term = make_render_node<RenderTerminal>(
            backend,
            TerminalColorScheme::one_half_dark(),
            TerminalCursorStyle::FilledBox,
            14.0f);
        term->layout(BoxConstraints::tight(Vec2{640.0f, 360.0f}));
        KT_CHECK(term->buffer().cols() >= 40);
        KT_CHECK(term->buffer().rows() >= 10);
        KT_CHECK(term->as_text_input_client() != nullptr);

        backend->feed_output("Welcome to \x1b[32mMockSSH\x1b[0m\r\n");
        PaintContext ctx(nullptr, 1.0f);
        term->paint(ctx);
        KT_CHECK(ctx.primitive_count() > 0);

        // 模拟按键与 IME 提交
        term->on_ime_commit("ls -la");
        KeyEvent enter_ev(PlatformEventType::KeyDown, 0x0D);
        term->on_key(enter_ev);
        KT_CHECK(captured_input == "ls -la\r");

        // 模拟 Ctrl+Shift+F 查找
        KeyEvent find_ev(PlatformEventType::KeyDown, 'F', "", KeyModifiers{true, true, false, false});
        term->on_key(find_ev);
        KT_CHECK(term->is_find_open());
        term->on_ime_commit("MockSSH");
        KT_CHECK(term->find_matches().size() == 1);
        term->toggle_find(); // 关闭查找栏

        // 模拟 TUI 开启 SGR 鼠标追踪 (?1003h + ?1006h，例如 Windows edit / vim / btop)
        captured_input.clear();
        backend->feed_output("\x1b[?1003h\x1b[?1006h");
        term->paint(ctx);
        KT_CHECK(term->buffer().mouse_tracking_mode() == 1003);
        KT_CHECK(term->buffer().mouse_sgr_mode());

        PointerEvent md(PlatformEventType::PointerDown, PointF{20.0f, 16.0f}, PointerButton::Primary);
        md.local_position = PointF{20.0f, 16.0f};
        term->on_pointer_down(md);
        KT_CHECK(captured_input.find("\x1b[<0;") != std::string::npos);
        KT_CHECK(captured_input.back() == 'M');

        captured_input.clear();
        PointerEvent mu(PlatformEventType::PointerUp, PointF{20.0f, 16.0f}, PointerButton::Primary);
        mu.local_position = PointF{20.0f, 16.0f};
        term->on_pointer_up(mu);
        KT_CHECK(captured_input.find("\x1b[<0;") != std::string::npos);
        KT_CHECK(captured_input.back() == 'm');

        captured_input.clear();
        PointerEvent mw(PlatformEventType::PointerScroll, PointF{20.0f, 16.0f}, PointerButton::None);
        mw.local_position = PointF{20.0f, 16.0f};
        mw.delta = Vec2{0.0f, -36.0f}; // Win32 约定: delta.y < 0 为向上滚
        term->on_pointer_move(mw);
        KT_CHECK(captured_input.find("\x1b[<64;") != std::string::npos);

        // Ctrl+滚轮缩放验证
        float before_fs = term->font_size();
        PointerEvent zoom_ev(PlatformEventType::PointerScroll, PointF{20.0f, 16.0f}, PointerButton::None, KeyModifiers{false, true, false, false}, Vec2{0.0f, -36.0f});
        zoom_ev.local_position = PointF{20.0f, 16.0f};
        term->on_pointer_move(zoom_ev);
        KT_CHECK(term->font_size() > before_fs);
    }

    // 5. 内置 DemoShellBackend 交互命令验证
    {
        auto shell = std::make_shared<DemoShellBackend>();
        std::string out;
        shell->on_output = [&](std::string_view s) { out.append(s); };
        KT_CHECK(shell->start(80, 24));
        shell->write("echo hello_ktui\r");
        KT_CHECK(out.find("hello_ktui") != std::string::npos);
        shell->write("colors\r");
        KT_CHECK(out.find("\x1b[48;2;") != std::string::npos);
    }

    std::printf("[PASS] test_ktui_terminal: all VT500/TrueColor/CJK/AltScreen/OSC9;4/Backend/RenderTerminal checks passed!\n");
    return 0;
}
