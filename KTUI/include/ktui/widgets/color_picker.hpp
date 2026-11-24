#pragma once

#include "common/fluent_chrome.hpp"
#include "common/fluent_icons.hpp"
#include "widget.hpp"
#include <ktui/app/window.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/popup_handle.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

namespace ktui {

// 颜色转换与棋盘格绘制公共辅助
struct ColorPickerUtil {
    static void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) noexcept {
        h = std::fmod(std::fmod(h, 360.0f) + 360.0f, 360.0f);
        s = std::clamp(s, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        float c = v * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        float rp = 0.0f, gp = 0.0f, bp = 0.0f;
        if (h < 60.0f)       { rp = c; gp = x; bp = 0.0f; }
        else if (h < 120.0f) { rp = x; gp = c; bp = 0.0f; }
        else if (h < 180.0f) { rp = 0.0f; gp = c; bp = x; }
        else if (h < 240.0f) { rp = 0.0f; gp = x; bp = c; }
        else if (h < 300.0f) { rp = x; gp = 0.0f; bp = c; }
        else                 { rp = c; gp = 0.0f; bp = x; }
        r = rp + m;
        g = gp + m;
        b = bp + m;
    }

    static void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) noexcept {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        float cmax = std::max({r, g, b});
        float cmin = std::min({r, g, b});
        float delta = cmax - cmin;
        v = cmax;
        s = (cmax > 1e-6f) ? (delta / cmax) : 0.0f;
        if (delta <= 1e-6f) {
            h = 0.0f;
        } else if (cmax == r) {
            h = 60.0f * std::fmod((g - b) / delta, 6.0f);
            if (h < 0.0f) h += 360.0f;
        } else if (cmax == g) {
            h = 60.0f * (((b - r) / delta) + 2.0f);
        } else {
            h = 60.0f * (((r - g) / delta) + 4.0f);
        }
    }

    static std::string format_hex(Color c, bool include_alpha) {
        auto to_u8 = [](float x) -> unsigned {
            return static_cast<unsigned>(std::clamp(std::lround(x * 255.0f), 0L, 255L));
        };
        char buf[16];
        if (include_alpha) {
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", to_u8(c.a), to_u8(c.r), to_u8(c.g), to_u8(c.b));
        } else {
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", to_u8(c.r), to_u8(c.g), to_u8(c.b));
        }
        return std::string(buf);
    }

    static void paint_checkerboard(PaintContext& ctx, Rect r, float cell) {
        Color c1{0.82f, 0.82f, 0.82f, 1.0f};
        Color c2{0.62f, 0.62f, 0.62f, 1.0f};
        ctx.draw_rect(r, c1, 0.0f, 0.0f);
        int cols = static_cast<int>(std::ceil(r.width() / cell));
        int rows = static_cast<int>(std::ceil(r.height() / cell));
        for (int row = 0; row < rows; ++row) {
            for (int col = (row & 1); col < cols; col += 2) {
                ctx.draw_rect(Rect::from_xywh(r.min_x + static_cast<float>(col) * cell,
                                              r.min_y + static_cast<float>(row) * cell,
                                              cell, cell),
                              c2, 0.0f, 0.0f);
            }
        }
    }
};

// WinUI 3 ColorPicker 固定尺寸弹出浮层面板 (324 x 268 / 242px)
class RenderColorPickerPopup : public RenderBox {
public:
    RenderColorPickerPopup(Color initial_color,
                           bool alpha_enabled,
                           Fn<void(const Color&)> on_color_changed)
        : m_alpha_enabled(alpha_enabled),
          m_on_color_changed(std::move(on_color_changed)) {
        cursor_shape = CursorShape::Arrow;
        set_color_internal(initial_color);
        setup_behavior();
    }

    ~RenderColorPickerPopup() override = default;

    [[nodiscard]] Vec2 compute_ideal_size() const noexcept {
        float inner_h = kSpectrumH + 12.0f + kSliderH + (m_alpha_enabled ? (10.0f + kSliderH) : 0.0f) + 12.0f + kFooterH;
        return Vec2{kPanelW, inner_h + kPad * 2.0f};
    }

    void perform_layout() override {
        set_size(compute_ideal_size());
    }

    void paint(PaintContext& ctx) override {
        auto st = m_surface.tick();
        if (st.animating) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        const float alpha = st.alpha;
        FluentPopupSurface::paint_panel(ctx, theme, Rect::from_xywh(0.0f, 0.0f, size().x, size().y), alpha, 8.0f);

        Rect sv_r = spectrum_rect();
        Rect sw_r = swatch_rect();
        Rect hu_r = hue_rect();

        // 1. 2D 饱和度/明度 (SV) 色盘
        ctx.push_clip(sv_r, 6.0f);
        constexpr int kCols = 24;
        constexpr int kRows = 16;
        float cw = sv_r.width() / static_cast<float>(kCols);
        float ch = sv_r.height() / static_cast<float>(kRows);
        for (int row = 0; row < kRows; ++row) {
            float v = 1.0f - (static_cast<float>(row) + 0.5f) / static_cast<float>(kRows);
            for (int col = 0; col < kCols; ++col) {
                float s = (static_cast<float>(col) + 0.5f) / static_cast<float>(kCols);
                float r, g, b;
                ColorPickerUtil::hsv_to_rgb(m_h, s, v, r, g, b);
                ctx.draw_rect(Rect::from_xywh(sv_r.min_x + static_cast<float>(col) * cw,
                                              sv_r.min_y + static_cast<float>(row) * ch,
                                              cw + 0.75f, ch + 0.75f),
                              Color{r, g, b, alpha}, 0.0f, 0.0f);
            }
        }
        ctx.pop_clip();
        ctx.draw_rect(sv_r, theme.colors.control_stroke_default, 6.0f, 1.0f);

        float sx = sv_r.min_x + m_s * sv_r.width();
        float sy = sv_r.min_y + (1.0f - m_v) * sv_r.height();
        paint_thumb(ctx, PointF{sx, sy}, Color{m_color.r, m_color.g, m_color.b, alpha}, 7.0f, alpha);

        // 2. 右侧实时颜色预览条
        ctx.push_clip(sw_r, 6.0f);
        ColorPickerUtil::paint_checkerboard(ctx, sw_r, 6.0f);
        Color prev_c = m_color;
        prev_c.a *= alpha;
        ctx.draw_rect(sw_r, prev_c, 0.0f, 0.0f);
        ctx.pop_clip();
        ctx.draw_rect(sw_r, theme.colors.control_stroke_default, 6.0f, 1.0f);

        // 3. 色相 (Hue) 彩虹滑条
        ctx.push_clip(hu_r, kSliderH * 0.5f);
        constexpr int kHueSegs = 36;
        float hw = hu_r.width() / static_cast<float>(kHueSegs);
        for (int i = 0; i < kHueSegs; ++i) {
            float hh = (static_cast<float>(i) + 0.5f) / static_cast<float>(kHueSegs) * 360.0f;
            float r, g, b;
            ColorPickerUtil::hsv_to_rgb(hh, 1.0f, 1.0f, r, g, b);
            ctx.draw_rect(Rect::from_xywh(hu_r.min_x + static_cast<float>(i) * hw, hu_r.min_y,
                                          hw + 0.75f, hu_r.height()),
                          Color{r, g, b, alpha}, 0.0f, 0.0f);
        }
        ctx.pop_clip();
        ctx.draw_rect(hu_r, theme.colors.control_stroke_default, kSliderH * 0.5f, 1.0f);

        float hr, hg, hb;
        ColorPickerUtil::hsv_to_rgb(m_h, 1.0f, 1.0f, hr, hg, hb);
        float hx = hu_r.min_x + (m_h / 360.0f) * hu_r.width();
        paint_thumb(ctx, PointF{hx, hu_r.min_y + hu_r.height() * 0.5f}, Color{hr, hg, hb, alpha}, 8.0f, alpha);

        // 4. 透明度 (Alpha) 滑条
        if (m_alpha_enabled) {
            Rect al_r = alpha_rect();
            ctx.push_clip(al_r, kSliderH * 0.5f);
            ColorPickerUtil::paint_checkerboard(ctx, al_r, 5.0f);
            constexpr int kAlphaSegs = 24;
            float aw = al_r.width() / static_cast<float>(kAlphaSegs);
            for (int i = 0; i < kAlphaSegs; ++i) {
                float a = (static_cast<float>(i) + 0.5f) / static_cast<float>(kAlphaSegs);
                ctx.draw_rect(Rect::from_xywh(al_r.min_x + static_cast<float>(i) * aw, al_r.min_y,
                                              aw + 0.75f, al_r.height()),
                              Color{m_color.r, m_color.g, m_color.b, a * alpha}, 0.0f, 0.0f);
            }
            ctx.pop_clip();
            ctx.draw_rect(al_r, theme.colors.control_stroke_default, kSliderH * 0.5f, 1.0f);

            float ax = al_r.min_x + m_a * al_r.width();
            paint_thumb(ctx, PointF{ax, al_r.min_y + al_r.height() * 0.5f}, prev_c, 8.0f, alpha);
        }

        // 5. 底部 Hex 与 RGBA 数值条
        Rect ft = footer_rect();
        ctx.draw_rect(ft, theme.colors.control_fill_default, 4.0f, 0.0f);
        ctx.draw_rect(ft, theme.colors.control_stroke_default, 4.0f, 1.0f);

        auto to_u8 = [](float x) -> int {
            return static_cast<int>(std::clamp(std::lround(x * 255.0f), 0L, 255L));
        };
        std::string hex = ColorPickerUtil::format_hex(m_color, m_alpha_enabled);
        char rgba_buf[64];
        if (m_alpha_enabled) {
            std::snprintf(rgba_buf, sizeof(rgba_buf), "R:%d G:%d B:%d A:%d%%",
                          to_u8(m_color.r), to_u8(m_color.g), to_u8(m_color.b),
                          static_cast<int>(std::lround(m_a * 100.0f)));
        } else {
            std::snprintf(rgba_buf, sizeof(rgba_buf), "R:%d G:%d B:%d",
                          to_u8(m_color.r), to_u8(m_color.g), to_u8(m_color.b));
        }

        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.94f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.90f * alpha};
        Color mute_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.62f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.60f * alpha};
        Vec2 hs = TextMeasure::measure(hex, 12.5f);
        Vec2 rs = TextMeasure::measure(rgba_buf, 11.5f);
        ctx.draw_text(hex, PointF{ft.min_x + 10.0f, std::round(ft.min_y + (ft.height() - hs.y) * 0.5f)}, 12.5f, text_c);
        ctx.draw_text(rgba_buf,
                      PointF{std::round(ft.max_x - rs.x - 10.0f), std::round(ft.min_y + (ft.height() - rs.y) * 0.5f)},
                      11.5f, mute_c);
    }

private:
    static constexpr float kPanelW = 324.0f;
    static constexpr float kPad = 14.0f;
    static constexpr float kSpectrumH = 156.0f;
    static constexpr float kSwatchW = 32.0f;
    static constexpr float kSwatchGap = 10.0f;
    static constexpr float kSliderH = 16.0f;
    static constexpr float kFooterH = 30.0f;

    enum class DragTarget : uint8_t { None, Spectrum, Hue, Alpha };

    Rect spectrum_rect() const noexcept {
        return Rect::from_xywh(kPad, kPad, kPanelW - kPad * 2.0f - kSwatchW - kSwatchGap, kSpectrumH);
    }

    Rect swatch_rect() const noexcept {
        return Rect::from_xywh(kPanelW - kPad - kSwatchW, kPad, kSwatchW, kSpectrumH);
    }

    Rect hue_rect() const noexcept {
        return Rect::from_xywh(kPad, kPad + kSpectrumH + 12.0f, kPanelW - kPad * 2.0f, kSliderH);
    }

    Rect alpha_rect() const noexcept {
        return Rect::from_xywh(kPad, kPad + kSpectrumH + 12.0f + kSliderH + 10.0f, kPanelW - kPad * 2.0f, kSliderH);
    }

    Rect footer_rect() const noexcept {
        float y = kPad + kSpectrumH + 12.0f + kSliderH + (m_alpha_enabled ? (10.0f + kSliderH) : 0.0f) + 12.0f;
        return Rect::from_xywh(kPad, y, kPanelW - kPad * 2.0f, kFooterH);
    }

    static void paint_thumb(PaintContext& ctx, PointF center, Color fill, float radius, float alpha) {
        Rect outer = Rect::from_xywh(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        ctx.draw_rect(outer, Color{1.0f, 1.0f, 1.0f, alpha}, radius, 0.0f);
        ctx.draw_rect(outer, Color{0.0f, 0.0f, 0.0f, 0.45f * alpha}, radius, 1.0f);
        float ir = std::max(2.0f, radius - 2.5f);
        Rect inner = Rect::from_xywh(center.x - ir, center.y - ir, ir * 2.0f, ir * 2.0f);
        ctx.draw_rect(inner, fill, ir, 0.0f);
    }

    void set_color_internal(Color c) {
        m_color = c;
        m_a = std::clamp(c.a, 0.0f, 1.0f);
        ColorPickerUtil::rgb_to_hsv(c.r, c.g, c.b, m_h, m_s, m_v);
    }

    void commit_from_hsv() {
        float r, g, b;
        ColorPickerUtil::hsv_to_rgb(m_h, m_s, m_v, r, g, b);
        m_color = Color{r, g, b, m_alpha_enabled ? m_a : 1.0f};
        mark_needs_paint();
        if (m_on_color_changed) m_on_color_changed(m_color);
    }

    void apply_drag(Vec2 local) {
        if (m_drag == DragTarget::Spectrum) {
            Rect r = spectrum_rect();
            m_s = std::clamp((local.x - r.min_x) / std::max(1.0f, r.width()), 0.0f, 1.0f);
            m_v = std::clamp(1.0f - (local.y - r.min_y) / std::max(1.0f, r.height()), 0.0f, 1.0f);
            commit_from_hsv();
        } else if (m_drag == DragTarget::Hue) {
            Rect r = hue_rect();
            m_h = std::clamp((local.x - r.min_x) / std::max(1.0f, r.width()), 0.0f, 1.0f) * 360.0f;
            commit_from_hsv();
        } else if (m_drag == DragTarget::Alpha && m_alpha_enabled) {
            Rect r = alpha_rect();
            m_a = std::clamp((local.x - r.min_x) / std::max(1.0f, r.width()), 0.0f, 1.0f);
            commit_from_hsv();
        }
    }

    void setup_behavior() {
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            if (spectrum_rect().contains(e.local_position)) m_drag = DragTarget::Spectrum;
            else if (hue_rect().contains(e.local_position)) m_drag = DragTarget::Hue;
            else if (m_alpha_enabled && alpha_rect().contains(e.local_position)) m_drag = DragTarget::Alpha;
            else return;
            apply_drag(e.local_position);
            e.stop_propagation();
        };
        on_pointer_move = [this](PointerEvent& e) {
            if (m_drag != DragTarget::None) {
                apply_drag(e.local_position);
                e.stop_propagation();
                return;
            }
            bool hit = spectrum_rect().contains(e.local_position) ||
                       hue_rect().contains(e.local_position) ||
                       (m_alpha_enabled && alpha_rect().contains(e.local_position));
            cursor_shape = hit ? CursorShape::PointingHand : CursorShape::Arrow;
        };
        on_pointer_up = [this](PointerEvent&) {
            m_drag = DragTarget::None;
        };
    }

    Color m_color{0.0f, 0.47f, 0.84f, 1.0f};
    float m_h{206.0f};
    float m_s{1.0f};
    float m_v{0.84f};
    float m_a{1.0f};
    bool m_alpha_enabled{true};
    DragTarget m_drag{DragTarget::None};
    FluentPopupSurface m_surface;
    Fn<void(const Color&)> m_on_color_changed;
};

// WinUI 3 ColorPicker 紧凑触发器渲染盒 (点击弹出固定尺寸 324px 真高斯阴影选色浮层)
class RenderColorPicker : public RenderBox {
public:
    RenderColorPicker() noexcept {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderColorPicker() override { m_popup_handle.close(); }

    static void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) noexcept {
        ColorPickerUtil::hsv_to_rgb(h, s, v, r, g, b);
    }

    static void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) noexcept {
        ColorPickerUtil::rgb_to_hsv(r, g, b, h, s, v);
    }

    static std::string format_hex(Color c, bool include_alpha) {
        return ColorPickerUtil::format_hex(c, include_alpha);
    }

    void set_properties(Color color,
                        const std::string& header,
                        bool is_alpha_enabled,
                        float width,
                        bool disabled,
                        Fn<void(const Color&)> on_color_changed) {
        if (m_color != color) { m_color = color; mark_needs_paint(); }
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        m_alpha_enabled = is_alpha_enabled;
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            cursor_shape = disabled ? CursorShape::Arrow : CursorShape::PointingHand;
            if (disabled) m_popup_handle.close();
            mark_needs_paint();
        }
        m_on_color_changed = std::move(on_color_changed);
    }

    void set_color_value(const Color& c) {
        if (m_color != c) {
            m_color = c;
            mark_needs_paint();
        }
    }

    [[nodiscard]] Color color() const noexcept { return m_color; }
    [[nodiscard]] std::string hex_string() const { return format_hex(m_color, m_alpha_enabled); }

    void perform_layout() override {
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        float w = (m_width > 0.0f) ? m_width : 190.0f;
        set_size(m_constraints.constrain(Vec2{w, header_h + 32.0f}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.94f}
                                     : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
        if (!m_header.empty()) {
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, text_c);
        }

        Rect box = box_rect();
        Color bg = m_disabled ? theme.colors.control_fill_disabled
                 : ((m_pressed || m_popup_handle.is_open()) ? theme.colors.control_fill_tertiary
                    : (m_hovered ? theme.colors.control_fill_secondary : theme.colors.control_fill_default));
        ctx.draw_rect(box, bg, 4.0f, 0.0f);
        ctx.draw_rect(box, theme.colors.control_stroke_default, 4.0f, 1.0f);

        Color bottom_line = m_popup_handle.is_open() ? theme.colors.primary : theme.colors.control_stroke_bottom;
        ctx.draw_rect(Rect::from_xywh(box.min_x + 2.0f, box.max_y - (m_popup_handle.is_open() ? 2.0f : 1.0f),
                                      std::max(0.0f, box.width() - 4.0f), m_popup_handle.is_open() ? 2.0f : 1.0f),
                      bottom_line, 0.0f, 0.0f);

        // 左侧色块预览 (带棋盘格底)
        Rect sw = Rect::from_xywh(box.min_x + 6.0f, box.min_y + 6.0f, 20.0f, 20.0f);
        ctx.push_clip(sw, 4.0f);
        ColorPickerUtil::paint_checkerboard(ctx, sw, 5.0f);
        ctx.draw_rect(sw, m_color, 0.0f, 0.0f);
        ctx.pop_clip();
        ctx.draw_rect(sw, theme.colors.control_stroke_default, 4.0f, 1.0f);

        // 中部 Hex 文本
        std::string hex = hex_string();
        Vec2 hs = TextMeasure::measure(hex, 13.0f);
        ctx.draw_text(hex,
                      PointF{sw.max_x + 10.0f, std::round(box.min_y + (box.height() - hs.y) * 0.5f)},
                      13.0f, text_c);

        // 右侧下拉箭头
        Color ic = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.30f : 0.72f}
                                 : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.30f : 0.68f};
        Rect cr = Rect::from_xywh(box.max_x - 26.0f, box.min_y, 22.0f, box.height());
        fluent_icons::draw_centered(ctx, fluent_icons::kChevronDown, cr, 11.0f, ic);
    }

private:
    Rect box_rect() const noexcept {
        float top = m_header.empty() ? 0.0f : 22.0f;
        return Rect::from_xywh(0.0f, top, size().x, std::max(32.0f, size().y - top));
    }

    void toggle_popup() {
        if (m_popup_handle.is_open()) {
            m_popup_handle.close();
            mark_needs_paint();
            return;
        }
        Window* win = Window::active_window();
        if (!win || m_disabled) return;

        auto token = std::make_shared<bool>(true);
        auto popup = make_render_node<RenderColorPickerPopup>(
            m_color, m_alpha_enabled,
            [this, token](const Color& c) {
                if (!*token) return;
                m_color = c;
                mark_needs_paint();
                if (m_on_color_changed) m_on_color_changed(c);
            });

        Vec2 sz = popup->compute_ideal_size();
        OverlayEntry entry;
        entry.content = popup;
        entry.global_pos = FluentPopupSurface::compute_anchored_pos(*win, global_offset(), box_rect(), sz);
        entry.size = sz;
        entry.dismiss_on_outside_click = true;
        entry.is_modal = false;

        m_popup_handle.open(*win, std::move(entry), [this, token]() {
            *token = false;
            mark_needs_paint();
        });
        mark_needs_paint();
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_disabled) { m_hovered = true; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_pressed = false;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            m_pressed = true;
            mark_needs_paint();
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            bool was = m_pressed;
            m_pressed = false;
            mark_needs_paint();
            if (was) toggle_popup();
            e.stop_propagation();
        };
    }

    Color m_color{0.0f, 0.47f, 0.84f, 1.0f};
    std::string m_header;
    bool m_alpha_enabled{true};
    float m_width{190.0f};
    bool m_disabled{false};
    bool m_hovered{false};
    bool m_pressed{false};
    PopupHandle m_popup_handle;
    Fn<void(const Color&)> m_on_color_changed;

public:
    void wrap_on_color_changed(Fn<void(const Color&)> extra) {
        auto prev = std::move(m_on_color_changed);
        m_on_color_changed = [prev = std::move(prev), extra = std::move(extra)](const Color& c) {
            if (extra) extra(c);
            if (prev) prev(c);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 ColorPicker (点击弹出固定尺寸色盘浮层)
struct ColorPicker : BindableMixin<ColorPicker, RenderColorPicker> {
    Color color{Color{0.0f, 0.47f, 0.84f, 1.0f}};
    std::string header;
    bool is_alpha_enabled{true};
    float width{190.0f};
    bool disabled{false};
    Fn<void(const Color&)> on_color_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderColorPicker>& node, Property<Color>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_color_value(prop.get());
    std::weak_ptr<RenderColorPicker> weak = node;
    bg.subscribe(prop, [weak, guard](const Color& c) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_color_value(c);
        }
    });
    Property<Color>* p = &prop;
    node->wrap_on_color_changed([p, guard](const Color& c) {
        *guard = true;
        p->set(c);
        *guard = false;
    });
}

class ColorPickerWidget : public Widget {
public:
    explicit ColorPickerWidget(const ColorPicker& desc) : m_desc(desc) {}
    explicit ColorPickerWidget(ColorPicker&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ColorPickerWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderColorPicker>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderColorPicker>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderColorPicker>& node) {
        node->set_properties(
            m_desc.color, m_desc.header, m_desc.is_alpha_enabled, m_desc.width, m_desc.disabled,
            m_desc.on_color_changed);
    }

    ColorPicker m_desc;
};

inline ColorPicker::operator WidgetPtr() const& {
    return make_widget<ColorPickerWidget>(*this);
}
inline ColorPicker::operator WidgetPtr() && {
    return make_widget<ColorPickerWidget>(std::move(*this));
}

} // namespace ktui
