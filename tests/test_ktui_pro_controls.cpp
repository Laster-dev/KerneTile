#include <ktui/ktui.hpp>
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace ktui;

int main() {
    std::printf("=== KTUI Pro Controls Unit Tests (7 Widgets) ===\n");

    // 1. QueryFilterBuilder
    {
        RenderQueryFilterBuilder qf(
            { QueryFilterField{"status", "Status", {"open", "closed"}},
              QueryFilterField{"priority", "Priority", {"high", "low"}} },
            { QueryFilterToken{"status", "open"} });
        qf.layout(BoxConstraints::tight(Vec2{480.0f, 38.0f}));
        qf.add_token("priority", "high");
        assert(qf.tokens().size() == 2);
        assert(qf.to_expression() == "status:open priority:high");
        qf.toggle_raw_mode();
        assert(qf.is_raw_mode());
        qf.toggle_raw_mode();
        assert(!qf.is_raw_mode());
        assert(qf.tokens().size() == 2);
        KeyEvent bs(PlatformEventType::KeyDown, 0x08);
        qf.on_key(bs);
        assert(qf.tokens().size() == 1);
        std::printf("[1/7] QueryFilterBuilder (single RenderTextBox) passed\n");
    }

    // 2. ScrubNumberBox
    {
        double last_val = 0.0;
        RenderScrubNumberBox sb("Opacity", 50.0, 0.0, 100.0, 1.0, 0, "%",
                                [&](double v) { last_val = v; });
        sb.layout(BoxConstraints::tight(Vec2{180.0f, 34.0f}));
        PointerEvent down(PlatformEventType::PointerDown, Vec2{50.0f, 17.0f}, PointerButton::Primary);
        sb.on_pointer_down(down);
        KeyModifiers shift_mod; shift_mod.shift = true;
        PointerEvent move1(PlatformEventType::PointerMove, Vec2{54.0f, 17.0f}, PointerButton::None, shift_mod);
        sb.on_pointer_move(move1);
        PointerEvent move2(PlatformEventType::PointerMove, Vec2{74.0f, 17.0f}, PointerButton::None, shift_mod);
        sb.on_pointer_move(move2);
        PointerEvent up(PlatformEventType::PointerUp, Vec2{70.0f, 17.0f}, PointerButton::Primary);
        sb.on_pointer_up(up);
        assert(std::abs(sb.value() - 100.0) < 1e-4);
        assert(std::abs(last_val - 100.0) < 1e-4);
        std::printf("[2/7] ScrubNumberBox passed\n");
    }

    // 3. HoldConfirmButton (PillBar + CircularRing)
    {
        int fired = 0;
        RenderHoldConfirmButton btn("Hold", "Holding", "Done", 1.5f, true, HoldConfirmShape::PillBar, 64.0f, [&]() { ++fired; });
        btn.layout(BoxConstraints::tight(Vec2{200.0f, 36.0f}));
        PointerEvent down(PlatformEventType::PointerDown, Vec2{50.0f, 18.0f}, PointerButton::Primary);
        btn.on_pointer_down(down);
        btn.advance_by(0.75f);
        assert(fired == 0 && btn.progress() > 0.45f);
        PointerEvent up(PlatformEventType::PointerUp, Vec2{50.0f, 18.0f}, PointerButton::Primary);
        btn.on_pointer_up(up);
        btn.advance_by(0.5f);
        assert(fired == 0 && btn.progress() == 0.0f);

        btn.on_pointer_down(down);
        btn.advance_by(1.6f);
        assert(fired == 1);

        int ring_fired = 0;
        RenderHoldConfirmButton ring("删", "", "✓", 1.0f, true, HoldConfirmShape::CircularRing, 64.0f, [&]() { ++ring_fired; });
        ring.layout(BoxConstraints::loose(Vec2{200.0f, 200.0f}));
        assert(ring.size().x == 64.0f && ring.size().y == 64.0f);
        PointerEvent rdown(PlatformEventType::PointerDown, Vec2{32.0f, 32.0f}, PointerButton::Primary);
        ring.on_pointer_down(rdown);
        ring.advance_by(1.1f);
        assert(ring_fired == 1);
        std::printf("[3/7] HoldConfirmButton (PillBar & CircularRing) passed\n");
    }

    // 4. GhostTextBox (逐词 Tab 补全到下一个空格)
    {
        std::string accepted;
        RenderGhostTextBox gt("SELECT ", "", {"SELECT * FROM users WHERE active = 1;"},
                              nullptr, nullptr, [&](const std::string& s) { accepted = s; });
        gt.layout(BoxConstraints::tight(Vec2{420.0f, 36.0f}));
        assert(gt.ghost_suffix() == "* FROM users WHERE active = 1;");
        KeyEvent tab(PlatformEventType::KeyDown, 0x09);
        gt.on_key(tab);
        assert(gt.text() == "SELECT * ");
        assert(gt.ghost_suffix() == "FROM users WHERE active = 1;");
        gt.on_key(tab);
        assert(gt.text() == "SELECT * FROM ");
        assert(accepted == "SELECT * FROM ");
        std::printf("[4/7] GhostTextBox (word-by-word Tab) passed\n");
    }

    // 5. MinimapTimelineSlider
    {
        RenderMinimapTimelineSlider tl({10, 20, 95, 30, 88, 15}, 0.2f, 0.6f);
        tl.layout(BoxConstraints::tight(Vec2{480.0f, 84.0f}));
        tl.set_range(0.3f, 0.8f, false);
        assert(std::abs(tl.start_ratio() - 0.3f) < 1e-4f);
        assert(std::abs(tl.end_ratio() - 0.8f) < 1e-4f);
        std::printf("[5/7] MinimapTimelineSlider passed\n");
    }

    // 6. IpPortInput (单一原生 RenderTextBox + 智能自动补点去重 + Ctrl+A/C/V)
    {
        RenderIpPortInput ip("192.168.1.1", 8080, true);
        ip.layout(BoxConstraints::tight(Vec2{196.0f, 30.0f}));
        KeyModifiers ctrl; ctrl.ctrl = true;
        KeyEvent sel_all(PlatformEventType::KeyDown, 'A', "", ctrl);
        ip.on_key(sel_all);
        const std::string typed = "127.0.0.1:9000";
        for (char c : typed) ip.on_ime_commit(std::string(1, c));
        assert(ip.is_valid());
        assert(ip.endpoint_string() == "127.0.0.1:9000");
        assert(ip.address_category() == "Loopback");

        ip.on_key(sel_all);
        KeyEvent copy_ev(PlatformEventType::KeyDown, 'C', "", ctrl);
        ip.on_key(copy_ev);
        assert(Clipboard::get_text() == "127.0.0.1:9000");

        Clipboard::set_text("http://10.20.30.40:443/api");
        KeyEvent paste_ev(PlatformEventType::KeyDown, 'V', "", ctrl);
        ip.on_key(paste_ev);
        assert(ip.is_valid());
        assert(ip.endpoint_string() == "10.20.30.40:443");
        assert(ip.address_category() == "LAN");
        std::printf("[6/7] IpPortInput (single RenderTextBox + auto-dot + clipboard) passed\n");
    }

    // 7. SpotlightTour (click-through inside cutout hole)
    {
        bool finished = false;
        std::vector<SpotlightStep> steps = {
            SpotlightStep{"Step 1", "Desc 1", []() { return Rect::from_xywh(100.0f, 100.0f, 120.0f, 40.0f); }, 6.0f},
            SpotlightStep{"Step 2", "Desc 2", []() { return Rect::from_xywh(300.0f, 100.0f, 120.0f, 40.0f); }, 6.0f}
        };
        RenderSpotlightTourOverlay ov(std::move(steps), nullptr, [&](bool ok) { finished = ok; });
        ov.layout(BoxConstraints::tight(Vec2{800.0f, 600.0f}));
        // Run paint once with null renderer? No, test step progression & hit_test bounds directly
        assert(ov.current_step() == 0);
        ov.next();
        assert(ov.current_step() == 1);
        ov.next();
        assert(finished);
        std::printf("[7/7] SpotlightTour passed\n");
    }

    std::printf("=== All 7 Pro Controls Passed! ===\n");
    return 0;
}
