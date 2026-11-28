#include <ktui/app/window.hpp>
#include <ktui/animation/frame_clock.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/mica_backdrop.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <cmath>

namespace ktui {

Window* Window::s_active_window = nullptr;
std::vector<Window*> Window::s_live_windows;

Window::Window(uint32_t id, std::unique_ptr<IPlatformWindow> platform_window, bool show_titlebar)
    : m_id(id),
      m_platform_window(std::move(platform_window)),
      m_show_titlebar(show_titlebar) {
    s_live_windows.push_back(this);
    RenderBox::s_destroy_hook = &Window::on_render_box_destroyed;
    init_internal_hooks();
}

Window::~Window() {
    auto it = std::find(s_live_windows.begin(), s_live_windows.end(), this);
    if (it != s_live_windows.end()) {
        s_live_windows.erase(it);
    }
    if (s_active_window == this) {
        s_active_window = s_live_windows.empty() ? nullptr : s_live_windows.back();
    }
    close();
}

void Window::on_render_box_destroyed(RenderBox* box) noexcept {
    for (Window* w : s_live_windows) {
        if (w) {
            w->m_event_dispatcher.forget_node(box);
            w->m_focus_manager.forget_node(box);
#if KTUI_INSPECTOR_ACTIVE
            UiInspector::forget_node(w->m_inspector, box);
#endif
        }
    }
}

uint32_t Window::time_until_touch_long_press_ms() const noexcept {
    if (!has_pending_touch_long_press()) return 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_touch_lp.down_time).count();
    if (elapsed >= static_cast<long long>(kTouchLongPressMs)) return 0;
    return static_cast<uint32_t>(kTouchLongPressMs - static_cast<uint32_t>(elapsed));
}

bool Window::tick_touch_long_press() {
    if (!has_pending_touch_long_press() || !m_root_render_box) return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_touch_lp.down_time).count();
    if (elapsed < static_cast<long long>(kTouchLongPressMs)) return false;
    return fire_touch_long_press();
}

bool Window::trigger_touch_long_press_for_test() {
    if (!has_pending_touch_long_press() || !m_root_render_box) return false;
    return fire_touch_long_press();
}

void Window::start_theme_ripple(const Theme& old_theme, const Theme& new_theme, Vec2 origin) {
    Vec2 sz = size();
    if (sz.x <= 1.0f || sz.y <= 1.0f) return;
    float ox = std::clamp(origin.x, 0.0f, sz.x);
    float oy = std::clamp(origin.y, 0.0f, sz.y);
    float d1 = std::hypot(ox, oy);
    float d2 = std::hypot(sz.x - ox, oy);
    float d3 = std::hypot(ox, sz.y - oy);
    float d4 = std::hypot(sz.x - ox, sz.y - oy);
    m_theme_ripple.max_radius = std::max({d1, d2, d3, d4}) + 24.0f;
    m_theme_ripple.origin = Vec2{ox, oy};
    m_theme_ripple.old_theme = old_theme;
    m_theme_ripple.new_theme = new_theme;
    m_theme_ripple.start = std::chrono::steady_clock::now();
    m_theme_ripple.active = true;
    m_theme_crossfade.active = false;
    request_redraw();
    if (m_platform_window) m_platform_window->invalidate();
}

void Window::start_theme_crossfade(const Theme& old_theme, const Theme& new_theme, float duration_sec) {
    m_theme_crossfade.old_theme = old_theme;
    m_theme_crossfade.new_theme = new_theme;
    m_theme_crossfade.duration_sec = std::max(0.05f, duration_sec);
    m_theme_crossfade.start = std::chrono::steady_clock::now();
    m_theme_crossfade.active = true;
    m_theme_ripple.active = false;
    request_redraw();
    if (m_platform_window) m_platform_window->invalidate();
}

void Window::set_content(WidgetPtr content) {
    m_content_widget = std::move(content);
    m_root_render_box = m_content_widget ? m_content_widget->create_render_node() : nullptr;
    m_event_dispatcher.clear();
    m_focus_manager.clear_focus();
    ensure_renderer();
    request_redraw();
    if (m_platform_window) {
        m_platform_window->invalidate();
    }
}

void Window::close() {
    if (!m_is_closed) {
        m_is_closed = true;
#if KTUI_INSPECTOR_ACTIVE
        UiInspector::close_devtools_window(this);
#endif
        m_renderer.reset();
        std::vector<uint8_t>().swap(m_framebuffer);
        m_overlay_manager.dismiss_all();
        m_event_dispatcher.clear();
        m_focus_manager.clear_focus();
        m_root_render_box.reset();
        m_content_widget.reset();
        m_custom_hit_test = nullptr;
        if (m_platform_window) {
            m_platform_window->close();
        }
    }
}

void Window::set_dithering_enabled(bool enabled) {
    if (m_dithering_enabled != enabled) {
        m_dithering_enabled = enabled;
        if (m_renderer) {
            m_renderer->set_dithering_enabled(enabled);
        }
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }
}

void Window::set_pixel_snapping_enabled(bool enabled) {
    if (m_pixel_snapping_enabled != enabled) {
        m_pixel_snapping_enabled = enabled;
        if (m_renderer) {
            m_renderer->set_pixel_snapping_enabled(enabled);
        }
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }
}

void Window::set_font_gamma_enabled(bool enabled) {
    if (m_font_gamma_enabled != enabled) {
        m_font_gamma_enabled = enabled;
        if (m_renderer) {
            m_renderer->set_font_gamma_enabled(enabled);
        }
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }
}

void Window::set_acrylic_grain_enabled(bool enabled) {
    if (m_acrylic_grain_enabled != enabled) {
        m_acrylic_grain_enabled = enabled;
        if (m_renderer) {
            m_renderer->set_acrylic_grain_enabled(enabled);
        }
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }
}

void Window::ensure_renderer() {
    if (m_renderer || !m_platform_window) return;
    Vec2 sz = m_platform_window->size();
    m_render_scale = m_platform_window->render_scale();
    const auto w = static_cast<uint32_t>(std::max<long>(1L, std::lround(sz.x * m_render_scale)));
    const auto h = static_cast<uint32_t>(std::max<long>(1L, std::lround(sz.y * m_render_scale)));
    kernetile::RendererConfig cfg;
    cfg.width = w;
    cfg.height = h;
    // 仅多窗口场景的辅助/浮动子窗采用紧凑图集；主窗口按 DPI 自适应（Android / 高 DPI 屏默认 2048x2048，避免 CJK 高倍率单帧溢出）
    const bool is_aux_window = (s_live_windows.size() > 1);
    cfg.atlas_size = is_aux_window
        ? (m_render_scale >= 2.0f ? 1024u : 512u)
        : (m_render_scale >= 2.0f ? 2048u : 1024u);
    cfg.enable_dithering = m_dithering_enabled;
    cfg.enable_pixel_snapping = m_pixel_snapping_enabled;
    cfg.enable_font_gamma = m_font_gamma_enabled;
    cfg.enable_acrylic_grain = m_acrylic_grain_enabled;
    m_renderer = kernetile::KerneTileRenderer::create(cfg);
    m_last_native_handle = m_platform_window->get_native_handle();
    m_renderer->set_compute_target(m_last_native_handle,
                                   m_platform_window->get_compute_display());
    m_renderer_w = w;
    m_renderer_h = h;
}

Rect Window::offset_ime_rect(Rect r) const {
    const float tb_h = titlebar_height();
    r.min_y += tb_h;
    r.max_y += tb_h;
    return r;
}

void Window::sync_ime_state() {
    if (!m_platform_window) return;
    RenderBox* focused = m_focus_manager.focused_node();
    if (auto* client = (focused ? focused->as_text_input_client() : nullptr)) {
        if (client->accepts_text_input()) {
            m_platform_window->set_ime_enabled(true);
            m_platform_window->set_ime_cursor_rect(offset_ime_rect(client->get_ime_cursor_rect()));
            return;
        }
    }
    m_platform_window->set_ime_enabled(false);
}

void Window::advance_mica_animation() {
    auto now = std::chrono::steady_clock::now();
    float dt = (m_mica_anim.last_time.time_since_epoch().count() == 0)
        ? 0.016f
        : std::clamp(std::chrono::duration<float>(now - m_mica_anim.last_time).count(), 0.0f, 0.05f);
    m_mica_anim.last_time = now;
    float target_focus = m_window_focused ? 1.0f : 0.0f;
    if (std::abs(m_mica_anim.focus_progress - target_focus) > 0.002f) {
        float k = 1.0f - std::exp(-dt * 9.5f);
        m_mica_anim.focus_progress += (target_focus - m_mica_anim.focus_progress) * k;
        if (std::abs(m_mica_anim.focus_progress - target_focus) <= 0.003f) {
            m_mica_anim.focus_progress = target_focus;
        } else {
            request_redraw();
        }
    } else {
        m_mica_anim.focus_progress = target_focus;
    }
}

void Window::paint_scene(PaintContext& ctx, Vec2 sz, Vec2 client_sz, float tb_h, bool mica_flag) {
    if (!mica_flag) {
        const Theme& t = ThemeContext::current();
        const Color base_bg = t.is_dark
            ? Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f}
            : Color{0xF3 / 255.0f, 0xF3 / 255.0f, 0xF3 / 255.0f, 1.0f};
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, sz.x, sz.y), base_bg, 0.0f, 0.0f);
    }
    if (tb_h > 0.0f) {
        ctx.push_clip(Rect::from_xywh(0.0f, tb_h, client_sz.x, client_sz.y), 0.0f);
        {
            PaintContext::OffsetScope client_offset(ctx, Vec2{0.0f, tb_h});
            m_root_render_box->paint(ctx);
        }
        ctx.pop_clip();
        m_titlebar.paint(ctx, sz, title(), m_window_focused, is_resizable(), is_maximized(), is_always_on_top(), mica_flag);
    } else {
        m_root_render_box->paint(ctx);
    }
    m_overlay_manager.layout_and_paint(ctx, sz);
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    if (!is_decorated()) {
        m_titlebar.paint_outer_border(ctx, sz, m_window_focused, is_maximized());
    }
#endif
#if KTUI_INSPECTOR_ACTIVE
    if (m_inspector.enabled) {
        if (m_renderer) m_renderer->mark_scene_end();
        UiInspector::paint_canvas_overlay(ctx, m_inspector, m_root_render_box.get(), tb_h, m_render_scale);
    }
#endif
}

void Window::render_frame(bool force_full_present) {
    if (!m_root_render_box || !m_platform_window) return;
    FrameClock::global().begin_frame();
    Vec2 sz = m_platform_window->size();
    if (sz.x <= 0 || sz.y <= 0) return;
    const float scale = m_platform_window->render_scale();
    const auto w = static_cast<uint32_t>(std::max<long>(1L, std::lround(sz.x * scale)));
    const auto h = static_cast<uint32_t>(std::max<long>(1L, std::lround(sz.y * scale)));

    ensure_renderer();
    if (!m_renderer) return;
    void* cur_native_handle = m_platform_window->get_native_handle();
    if (cur_native_handle != m_last_native_handle) {
        m_last_native_handle = cur_native_handle;
        m_renderer->set_compute_target(cur_native_handle, m_platform_window->get_compute_display());
        force_full_present = true;
    }
    if (m_compute_was_ready != m_renderer->compute_ready()) force_full_present = true;
    m_compute_was_ready = m_renderer->compute_ready();

    // 极速曝光恢复路径 (CPU 直接复用已有帧缓冲；GPU Flip-Model SwapChain 由 DWM 持久持有，无脏标时直接 0 开销早出)
    if (force_full_present &&
        m_renderer_w == w && m_renderer_h == h && m_render_scale == scale &&
        !needs_redraw() &&
        !m_root_render_box->needs_paint() &&
        !m_root_render_box->needs_layout())
    {
        if (m_compute_was_ready) {
            return;
        }
        if (m_framebuffer.size() == static_cast<size_t>(w) * h * 4) {
            m_platform_window->present_rgba8(m_framebuffer.data(), w, h);
            return;
        }
    }

    if (m_renderer_w != w || m_renderer_h != h || m_render_scale != scale) {
        if (m_renderer_w != 0 && (m_renderer_w != w || m_renderer_h != h)) {
            m_overlay_manager.dismiss_non_modal();
        }
        m_render_scale = scale;
        m_renderer->resize(w, h);
        m_renderer_w = w;
        m_renderer_h = h;
        force_full_present = true;
    }

    // 1. 布局
    const float tb_h = titlebar_height();
    const Vec2 client_sz{sz.x, std::max(0.0f, sz.y - tb_h)};
    m_root_render_box->layout(BoxConstraints::tight(client_sz));

    // 2. 清理上一帧脏标记 (在 paint() 前清空以保留动效步进产生的新脏标)
    m_root_render_box->clear_dirty_recursive();
    clear_redraw_flag();

    // 3. 推进自算云母焦点平滑过渡
    advance_mica_animation();

    // 4. 录制图元 (支持圆形涟漪双 Pass 主题切换或常规单 Pass)
#if KTUI_INSPECTOR_ACTIVE
    if (m_inspector.enabled) {
        UiInspector::capture_frame_snapshot(m_inspector, m_renderer.get());
        UiInspector::sync_devtools_window(this, false);
        if (UiInspector::has_active_animations(m_inspector)) {
            request_redraw();
        }
    }
#endif
    std::array<uint32_t, 16> mica_grid{};
    if (m_theme_ripple.active) {
        auto now = std::chrono::steady_clock::now();
        float t = std::clamp(std::chrono::duration<float>(now - m_theme_ripple.start).count() / 0.52f, 0.0f, 1.0f);
        float ease = 1.0f - std::pow(1.0f - t, 3.0f);
        float radius = std::max(1.0f, m_theme_ripple.max_radius * ease);

        const bool old_mica = MicaBackdropEngine::build_mica_grid(
            m_backdrop, m_theme_ripple.old_theme.is_dark, m_mica_anim.focus_progress,
            m_platform_window->monitor_uv_rect(), mica_grid);
        m_renderer->set_mica_backdrop(old_mica, mica_grid);

        m_renderer->begin_frame();
        PaintContext ctx(m_renderer.get(), scale, m_pixel_snapping_enabled);
        {
            ThemeScopeGuard old_scope(m_theme_ripple.old_theme);
            paint_scene(ctx, sz, client_sz, tb_h, old_mica);
        }
        {
            ThemeScopeGuard new_scope(m_theme_ripple.new_theme);
            Rect circle_bounds = Rect::from_xywh(
                m_theme_ripple.origin.x - radius,
                m_theme_ripple.origin.y - radius,
                radius * 2.0f, radius * 2.0f);
            ctx.push_clip(circle_bounds, radius);

            std::array<uint32_t, 16> new_mica_grid{};
            const bool new_mica = MicaBackdropEngine::build_mica_grid(
                m_backdrop, m_theme_ripple.new_theme.is_dark, m_mica_anim.focus_progress,
                m_platform_window->monitor_uv_rect(), new_mica_grid);
            Color ripple_bg = m_theme_ripple.new_theme.is_dark
                ? Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f}
                : Color{0xEA / 255.0f, 0xED / 255.0f, 0xF1 / 255.0f, 1.0f};
            if (new_mica) {
                float ar = 0.0f, ag = 0.0f, ab = 0.0f;
                for (uint32_t packed : new_mica_grid) {
                    ar += static_cast<float>(packed & 255u) / 255.0f;
                    ag += static_cast<float>((packed >> 8u) & 255u) / 255.0f;
                    ab += static_cast<float>((packed >> 16u) & 255u) / 255.0f;
                }
                ripple_bg = Color{ar / 16.0f, ag / 16.0f, ab / 16.0f, 1.0f};
            }
            ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, sz.x, sz.y), ripple_bg, 0.0f, 0.0f);
            paint_scene(ctx, sz, client_sz, tb_h, new_mica);
            ctx.pop_clip();
        }
        m_renderer->end_frame();

        if (t < 1.0f) {
            request_redraw();
        } else {
            m_theme_ripple.active = false;
            request_redraw();
        }
    } else if (m_theme_crossfade.active) {
        auto now = std::chrono::steady_clock::now();
        float t = std::clamp(std::chrono::duration<float>(now - m_theme_crossfade.start).count() / m_theme_crossfade.duration_sec, 0.0f, 1.0f);
        float ease = 1.0f - std::pow(1.0f - t, 3.0f);

        std::array<uint32_t, 16> old_grid{}, new_grid{};
        const bool old_mica = MicaBackdropEngine::build_mica_grid(
            m_backdrop, m_theme_crossfade.old_theme.is_dark, m_mica_anim.focus_progress,
            m_platform_window->monitor_uv_rect(), old_grid);
        const bool new_mica = MicaBackdropEngine::build_mica_grid(
            m_backdrop, m_theme_crossfade.new_theme.is_dark, m_mica_anim.focus_progress,
            m_platform_window->monitor_uv_rect(), new_grid);
        const bool mica_active = old_mica || new_mica;
        if (mica_active) {
            for (size_t i = 0; i < 16; ++i) {
                auto unpack_c = [](uint32_t pk) {
                    return Color{
                        static_cast<float>(pk & 255u) / 255.0f,
                        static_cast<float>((pk >> 8u) & 255u) / 255.0f,
                        static_cast<float>((pk >> 16u) & 255u) / 255.0f,
                        static_cast<float>((pk >> 24u) & 255u) / 255.0f
                    };
                };
                Color c0 = old_mica ? unpack_c(old_grid[i]) : m_theme_crossfade.old_theme.colors.background;
                Color c1 = new_mica ? unpack_c(new_grid[i]) : m_theme_crossfade.new_theme.colors.background;
                Color cm = kernetile::math::lerp_oklab(c0, c1, ease);
                uint32_t r = static_cast<uint32_t>(std::clamp(cm.r * 255.0f + 0.5f, 0.0f, 255.0f));
                uint32_t g = static_cast<uint32_t>(std::clamp(cm.g * 255.0f + 0.5f, 0.0f, 255.0f));
                uint32_t b = static_cast<uint32_t>(std::clamp(cm.b * 255.0f + 0.5f, 0.0f, 255.0f));
                uint32_t a = static_cast<uint32_t>(std::clamp(cm.a * 255.0f + 0.5f, 0.0f, 255.0f));
                mica_grid[i] = r | (g << 8u) | (b << 16u) | (a << 24u);
            }
        }
        m_renderer->set_mica_backdrop(mica_active, mica_grid);

        Theme blended = Theme::lerp(m_theme_crossfade.old_theme, m_theme_crossfade.new_theme, ease);
        m_renderer->begin_frame();
        PaintContext ctx(m_renderer.get(), scale, m_pixel_snapping_enabled);
        {
            ThemeScopeGuard scope(blended);
            paint_scene(ctx, sz, client_sz, tb_h, mica_active);
        }
        m_renderer->end_frame();

        if (t < 1.0f) {
            request_redraw();
        } else {
            m_theme_crossfade.active = false;
            request_redraw();
        }
    } else {
        const bool mica_active = MicaBackdropEngine::build_mica_grid(
            m_backdrop,
            ThemeContext::current().is_dark,
            m_mica_anim.focus_progress,
            m_platform_window->monitor_uv_rect(),
            mica_grid);
        m_renderer->set_mica_backdrop(mica_active, mica_grid);

        m_renderer->begin_frame();
        PaintContext ctx(m_renderer.get(), scale, m_pixel_snapping_enabled);
        paint_scene(ctx, sz, client_sz, tb_h, mica_active);
        m_renderer->end_frame();
    }

    // 若本帧发生字形图集溢出，立即预约下一帧触发自动扩容与完整补绘，杜绝静止画面残留丢字
    if (m_renderer->has_atlas_overflowed()) {
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }

    // 5. GPU 计算呈现或 CPU 软件回退呈现
    if (m_renderer->present_computed_frame(force_full_present)) {
        m_framebuffer.clear();
        return;
    }
    if (m_framebuffer.empty()) force_full_present = true;
    m_framebuffer.resize(static_cast<size_t>(w) * h * 4);
    m_renderer->render_to_rgba8(m_framebuffer.data());

    const auto& stats = m_renderer->get_last_frame_stats();
    if (force_full_present) {
        m_platform_window->present_rgba8(m_framebuffer.data(), w, h);
    } else if (stats.dirty_tiles > 0) {
        uint32_t dirty_x = 0, dirty_y = 0, dirty_w = w, dirty_h = h;
        if (m_renderer->get_last_dirty_bounds(dirty_x, dirty_y, dirty_w, dirty_h)) {
            m_platform_window->present_rgba8_region(m_framebuffer.data(), w, h, dirty_x, dirty_y, dirty_w, dirty_h);
        } else {
            m_platform_window->present_rgba8(m_framebuffer.data(), w, h);
        }
    }
}

bool Window::route_overlay_event(const PlatformEvent& pe) {
    if (!m_overlay_manager.has_active_overlay()) return false;

    if (pe.type == PlatformEventType::PointerDown) {
        bool handled = m_overlay_manager.handle_pointer_down(pe.position, pe.button, pe.device_kind);
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
        if (handled) return true;
    } else if (pe.type == PlatformEventType::PointerMove) {
        bool handled = m_overlay_manager.handle_pointer_move(pe.position, pe.device_kind);
        if (handled) {
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
            return true;
        }
    } else if (pe.type == PlatformEventType::PointerScroll) {
        bool handled = m_overlay_manager.handle_pointer_scroll(pe.position, pe.delta);
        if (handled) {
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
            return true;
        }
    } else if (pe.type == PlatformEventType::PointerUp) {
        bool handled = m_overlay_manager.handle_pointer_up(pe.position, pe.button, pe.device_kind);
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
        if (handled) return true;
    } else if (pe.type == PlatformEventType::KeyDown && pe.key_code == 27) {
        m_overlay_manager.dismiss_top();
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
        return true;
    }
    return m_overlay_manager.has_modal_overlay();
}

bool Window::route_titlebar_event(const PlatformEvent& pe, float tb_h) {
    if (tb_h <= 0.0f) return false;
    const Vec2 win_sz = size();
    const bool resizable = is_resizable();
    const bool maximized = is_maximized();

    if (pe.type == PlatformEventType::PointerMove) {
        if (pe.position.x < 0.0f || pe.position.y < 0.0f) {
            if (m_titlebar.handle_pointer_leave()) {
                request_redraw();
            }
            return false;
        }
        if (m_titlebar.handle_pointer_move(pe.position, win_sz, resizable)) {
            request_redraw();
        }
        TitlebarHitTest ht = m_titlebar.hit_test(pe.position, win_sz, resizable, maximized);
        const bool on_tb_interactive = m_titlebar.is_interactive_button_hit(pe.position, win_sz, resizable);
        if (ht != TitlebarHitTest::Client || on_tb_interactive) {
            PointerEvent leave_ev(PlatformEventType::PointerMove, PointF{-1.0f, -1.0f}, PointerButton::None);
            m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), leave_ev);
            if (m_platform_window) {
                CursorShape cur = m_titlebar.is_hand_cursor_region() ? CursorShape::PointingHand : CursorShape::Arrow;
                switch (ht) {
                    case TitlebarHitTest::BorderLeft:
                    case TitlebarHitTest::BorderRight:       cur = CursorShape::ResizeEW; break;
                    case TitlebarHitTest::BorderTop:
                    case TitlebarHitTest::BorderBottom:      cur = CursorShape::ResizeNS; break;
                    case TitlebarHitTest::BorderTopLeft:
                    case TitlebarHitTest::BorderBottomRight: cur = CursorShape::ResizeNWSE; break;
                    case TitlebarHitTest::BorderTopRight:
                    case TitlebarHitTest::BorderBottomLeft:  cur = CursorShape::ResizeNESW; break;
                    default: break;
                }
                m_platform_window->set_cursor(cur);
            }
            return true;
        }
    } else if (pe.type == PlatformEventType::PointerDown) {
        bool tb_dirty = false;
        if (m_titlebar.handle_pointer_down(pe.position, pe.button, win_sz, resizable, maximized, tb_dirty)) {
            if (tb_dirty) {
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
            }
            return true;
        }
        if (pe.position.y < tb_h) {
            return true;
        }
    } else if (pe.type == PlatformEventType::PointerUp) {
        bool tb_dirty = false;
        if (m_titlebar.handle_pointer_up(pe.position, pe.button, win_sz, resizable, tb_dirty)) {
            if (tb_dirty) {
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
            }
            return true;
        }
    }
    return false;
}

void Window::handle_platform_event(const PlatformEvent& pe) {
    s_active_window = this;
    if (!m_root_render_box) return;
    if (pe.type == PlatformEventType::PointerDown ||
        pe.type == PlatformEventType::PointerMove ||
        pe.type == PlatformEventType::PointerUp) {
        touch::InputModeTracker::notify(pe.device_kind);
    }
    if (pe.type == PlatformEventType::PointerDown || pe.type == PlatformEventType::PointerUp) {
        m_last_pointer_pos = pe.position;
        m_has_last_pointer = true;
    }
#if KTUI_INSPECTOR_ACTIVE
    if (pe.type == PlatformEventType::KeyDown && UiInspector::is_f12_key(pe.key_code)) {
        if (pe.modifiers.shift) {
            if (!m_inspector.enabled) {
                UiInspector::toggle_devtools_window(this);
            }
            m_inspector.show_dirty_tiles = !m_inspector.show_dirty_tiles;
            if (!m_inspector.show_dirty_tiles) m_inspector.tile_heats.clear();
            UiInspector::sync_devtools_window(this, true);
        } else {
            UiInspector::toggle_devtools_window(this);
        }
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
        return;
    }
    if (m_inspector.enabled && m_inspector.pick_mode) {
        if (UiInspector::handle_canvas_event(m_inspector, pe, m_root_render_box.get(), titlebar_height())) {
            UiInspector::sync_devtools_window(this, true);
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
            return;
        }
    }
#endif

    if (route_overlay_event(pe)) return;

    const float tb_h = titlebar_height();
    if (route_titlebar_event(pe, tb_h)) return;

    const Vec2 client_pos{pe.position.x, pe.position.y - tb_h};

    switch (pe.type) {
        case PlatformEventType::PointerMove: {
            if (pe.device_kind == PointerDeviceKind::Touch) {
                if (m_touch_lp.active) {
                    float dist = std::hypot(client_pos.x - m_touch_lp.pos.x, client_pos.y - m_touch_lp.pos.y);
                    if (dist > kTouchLongPressMoveSlop) {
                        m_touch_lp.active = false;
                    }
                }
                if (m_touch_lp.fired) break;
            }
            PointerEvent ev(PlatformEventType::PointerMove, client_pos, pe.button, pe.modifiers);
            ev.device_kind = pe.device_kind;
            ev.pointer_id = pe.pointer_id;
            ev.timestamp_ms = pe.timestamp_ms;
            m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), ev);
            if (pe.device_kind == PointerDeviceKind::Touch) {
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
            }
            if (m_platform_window && pe.device_kind != PointerDeviceKind::Touch) {
                CursorShape cursor = CursorShape::Arrow;
                HitTestResult hit_res;
                if (m_root_render_box->hit_test(hit_res, client_pos)) {
                    for (const auto& entry : hit_res.path()) {
                        if (entry.target && entry.target->cursor_shape != CursorShape::Arrow) {
                            cursor = entry.target->cursor_shape;
                            break;
                        }
                    }
                }
                m_platform_window->set_cursor(cursor);
            }
            break;
        }
        case PlatformEventType::PointerScroll: {
            PointerEvent ev(PlatformEventType::PointerScroll, client_pos, PointerButton::None, pe.modifiers, pe.delta);
            ev.device_kind = pe.device_kind;
            ev.pointer_id = pe.pointer_id;
            ev.timestamp_ms = pe.timestamp_ms;
            m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), ev);
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
            break;
        }
        case PlatformEventType::PointerDown: {
            if (pe.device_kind == PointerDeviceKind::Touch && pe.button == PointerButton::Primary) {
                m_touch_lp.active = true;
                m_touch_lp.fired = false;
                m_touch_lp.pos = client_pos;
                m_touch_lp.pointer_id = pe.pointer_id;
                m_touch_lp.down_time = std::chrono::steady_clock::now();
            } else {
                m_touch_lp.active = false;
                m_touch_lp.fired = false;
            }
            HitTestResult hit_res;
            RenderBox* new_focus = nullptr;
            if (m_root_render_box->hit_test(hit_res, client_pos)) {
                for (const auto& entry : hit_res.path()) {
                    if (entry.target && entry.target->is_focusable) {
                        new_focus = entry.target;
                        break;
                    }
                }
            }
            if (new_focus) {
                m_focus_manager.request_focus(new_focus);
            } else {
                m_focus_manager.clear_focus();
            }

            PointerEvent ev(PlatformEventType::PointerDown, client_pos, pe.button, pe.modifiers);
            ev.device_kind = pe.device_kind;
            ev.pointer_id = pe.pointer_id;
            ev.timestamp_ms = pe.timestamp_ms;
            m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), ev);
            sync_ime_state();
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
            break;
        }
        case PlatformEventType::PointerUp: {
            if (pe.device_kind == PointerDeviceKind::Touch) {
                m_touch_lp.active = false;
                if (m_touch_lp.fired) {
                    m_touch_lp.fired = false;
                    break;
                }
            }
            PointerEvent ev(PlatformEventType::PointerUp, client_pos, pe.button, pe.modifiers);
            ev.device_kind = pe.device_kind;
            ev.pointer_id = pe.pointer_id;
            ev.timestamp_ms = pe.timestamp_ms;
            m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), ev);
            sync_ime_state();
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
            break;
        }
        case PlatformEventType::KeyDown: {
            constexpr uint32_t kKeyEscape = 27;
            KeyEvent ev(PlatformEventType::KeyDown, pe.key_code, pe.text, pe.modifiers);
            RenderBox* key_root = m_overlay_manager.top_modal_content();
            if (!key_root) key_root = m_root_render_box.get();
            m_event_dispatcher.dispatch_key_event(m_focus_manager.focused_node(), key_root, ev);
            if (pe.key_code == kKeyEscape && !ev.is_propagation_stopped()) {
                m_focus_manager.clear_focus();
            }
            sync_ime_state();
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
            break;
        }
        case PlatformEventType::ImeComposition: {
            if (auto* client = (m_focus_manager.focused_node() ? m_focus_manager.focused_node()->as_text_input_client() : nullptr)) {
                if (client->accepts_text_input()) {
                    client->on_ime_preedit(pe.text, std::min(pe.ime_cursor_byte, pe.text.size()));
                    if (m_platform_window) m_platform_window->set_ime_cursor_rect(offset_ime_rect(client->get_ime_cursor_rect()));
                    request_redraw();
                    if (m_platform_window) m_platform_window->invalidate();
                }
            }
            break;
        }
        case PlatformEventType::ImeCommit: {
            RenderBox* focused = m_focus_manager.focused_node();
            if (auto* client = (focused ? focused->as_text_input_client() : nullptr)) {
                if (client->accepts_text_input()) {
                    client->on_ime_commit(pe.text);
                    if (m_platform_window) m_platform_window->set_ime_cursor_rect(offset_ime_rect(client->get_ime_cursor_rect()));
                    request_redraw();
                    if (m_platform_window) m_platform_window->invalidate();
                    break;
                }
            }
            if (focused) {
                KeyEvent ev(PlatformEventType::ImeCommit, pe.key_code, pe.text, pe.modifiers);
                m_event_dispatcher.dispatch_key_event(focused, m_root_render_box.get(), ev);
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
            }
            break;
        }
        case PlatformEventType::FileDrop: {
            if (!pe.dropped_files.empty()) {
                m_event_dispatcher.dispatch_file_drop(m_root_render_box.get(), client_pos,
                                                      pe.dropped_files, pe.modifiers);
                for (auto& cb : m_file_drop_callbacks) {
                    if (cb) cb(pe.dropped_files, client_pos);
                }
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
            }
            break;
        }
        case PlatformEventType::WindowCloseRequested: {
            bool allow_close = true;
            for (auto& cb : m_close_callbacks) {
                if (cb && !cb()) {
                    allow_close = false;
                }
            }
            if (allow_close) {
                close();
            }
            break;
        }
        default:
            break;
    }
}

void Window::init_internal_hooks() {
    if (!m_platform_window) return;

    m_titlebar.on_minimize = [this]() { minimize(); };
    m_titlebar.on_maximize_toggle = [this]() {
        if (is_maximized()) restore();
        else maximize();
        request_redraw();
    };
    m_titlebar.on_close = [this]() { close(); };
    m_titlebar.on_pin_toggle = [this]() {
        set_always_on_top(!is_always_on_top());
        request_redraw();
    };

    m_platform_window->set_titlebar_hit_test_callback([this](PointF local_pos) -> TitlebarHitTest {
        if (has_custom_titlebar()) {
            return m_titlebar.hit_test(local_pos, size(), is_resizable(), is_maximized());
        }
        if (m_custom_hit_test) {
            TitlebarHitTest custom_ht = m_custom_hit_test(local_pos);
            if (custom_ht != TitlebarHitTest::Client) return custom_ht;
        }
        // 无系统标题栏且关闭框架默认大标题栏的浮动工具窗，仍保留 5px 边缘 8 向缩放命中测试
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
        if (!is_decorated() && is_resizable() && !is_maximized()) {
            const Vec2 sz = size();
            constexpr float kEdge = 5.0f;
            const bool l = local_pos.x < kEdge;
            const bool r = local_pos.x >= sz.x - kEdge;
            const bool t = local_pos.y < kEdge;
            const bool b = local_pos.y >= sz.y - kEdge;
            if (t && l) return TitlebarHitTest::BorderTopLeft;
            if (t && r) return TitlebarHitTest::BorderTopRight;
            if (b && l) return TitlebarHitTest::BorderBottomLeft;
            if (b && r) return TitlebarHitTest::BorderBottomRight;
            if (l) return TitlebarHitTest::BorderLeft;
            if (r) return TitlebarHitTest::BorderRight;
            if (t) return TitlebarHitTest::BorderTop;
            if (b) return TitlebarHitTest::BorderBottom;
        }
#endif
        return TitlebarHitTest::Client;
    });

    m_platform_window->on_paint_requested([this]() {
        render_frame(true);
    });

    m_platform_window->on_resized([this](Vec2 sz) {
        request_redraw();
        for (auto& cb : m_resize_callbacks) {
            if (cb) cb(sz);
        }
    });

    m_platform_window->on_moved([this](Vec2) {
        if (m_backdrop != WindowBackdrop::None) {
            request_redraw();
        }
    });

    m_platform_window->on_close_requested([this]() -> bool {
        bool allow_close = true;
        for (auto& cb : m_close_callbacks) {
            if (cb && !cb()) {
                allow_close = false;
            }
        }
        if (allow_close) {
            m_is_closed = true;
            m_renderer.reset();
            std::vector<uint8_t>().swap(m_framebuffer);
            m_overlay_manager.dismiss_all();
            m_event_dispatcher.clear();
            m_focus_manager.clear_focus();
            m_root_render_box.reset();
            m_content_widget.reset();
            m_custom_hit_test = nullptr;
        }
        return allow_close;
    });

    m_platform_window->on_dpi_changed([this](float dpi) {
        request_redraw();
        for (auto& cb : m_dpi_callbacks) {
            if (cb) cb(dpi);
        }
    });

    m_platform_window->on_focus_changed([this](bool focused) {
        m_window_focused = focused;
        m_mica_anim.last_time = std::chrono::steady_clock::now();
        if (!focused) {
            m_titlebar.handle_pointer_leave();
        }
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
        for (auto& cb : m_focus_callbacks) {
            if (cb) cb(focused);
        }
    });
}

bool Window::fire_touch_long_press() {
    m_touch_lp.active = false;
    m_touch_lp.fired = true;
    Window* prev_active = s_active_window;
    s_active_window = this;
    m_event_dispatcher.cancel_active_interaction();
    PointerEvent sec_ev(PlatformEventType::PointerDown, m_touch_lp.pos, PointerButton::Secondary);
    sec_ev.device_kind = PointerDeviceKind::Touch;
    sec_ev.pointer_id = m_touch_lp.pointer_id;
    m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), sec_ev);
    s_active_window = prev_active ? prev_active : this;
    request_redraw();
    if (m_platform_window) m_platform_window->invalidate();
    return true;
}

} // namespace ktui
