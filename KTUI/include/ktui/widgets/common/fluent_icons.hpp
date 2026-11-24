#pragma once

#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <cmath>

namespace ktui::fluent_icons {

inline constexpr const char* kChevronLeft =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M15.71 4.29a1 1 0 0 1 0 1.42L9.41 12l6.3 6.29a1 1 0 1 1-1.42 1.42l-7-7a1 1 0 0 1 0-1.42l7-7a1 1 0 0 1 1.42 0z'/>"
    "</svg>";

inline constexpr const char* kChevronRight =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M8.29 4.29a1 1 0 0 1 1.42 0l7 7a1 1 0 0 1 0 1.42l-7 7a1 1 0 1 1-1.42-1.42L14.59 12 8.29 5.71a1 1 0 0 1 0-1.42z'/>"
    "</svg>";

inline constexpr const char* kChevronUp =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M4.29 15.71a1 1 0 0 1 0-1.42l7-7a1 1 0 0 1 1.42 0l7 7a1 1 0 1 1-1.42 1.42L12 9.41l-6.29 6.3a1 1 0 0 1-1.42 0z'/>"
    "</svg>";

inline constexpr const char* kChevronDown =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M4.29 8.29a1 1 0 0 1 1.42 0L12 14.59l6.29-6.3a1 1 0 1 1 1.42 1.42l-7 7a1 1 0 0 1-1.42 0l-7-7a1 1 0 0 1 0-1.42z'/>"
    "</svg>";

inline constexpr const char* kClose =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M6.22 6.22a.75.75 0 0 1 1.06 0L12 10.94l4.72-4.72a.75.75 0 1 1 1.06 1.06L13.06 12l4.72 4.72a.75.75 0 1 1-1.06 1.06L12 13.06l-4.72 4.72a.75.75 0 0 1-1.06-1.06L10.94 12 6.22 7.28a.75.75 0 0 1 0-1.06z'/>"
    "</svg>";

inline constexpr const char* kSearch =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M10.5 3.5a7 7 0 1 0 4.38 12.46l4.33 4.33a.75.75 0 1 0 1.06-1.06l-4.33-4.33A7 7 0 0 0 10.5 3.5zm0 1.5a5.5 5.5 0 1 1 0 11 5.5 5.5 0 0 1 0-11z'/>"
    "</svg>";

inline constexpr const char* kCalendar =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M7 2.75a.75.75 0 0 1 1.5 0V4h7V2.75a.75.75 0 0 1 1.5 0V4h1.25A2.75 2.75 0 0 1 21 6.75v11.5A2.75 2.75 0 0 1 18.25 21H5.75A2.75 2.75 0 0 1 3 18.25V6.75A2.75 2.75 0 0 1 5.75 4H7V2.75zM4.5 9.5v8.75c0 .69.56 1.25 1.25 1.25h12.5c.69 0 1.25-.56 1.25-1.25V9.5h-15zm1.25-4c-.69 0-1.25.56-1.25 1.25V8h15V6.75c0-.69-.56-1.25-1.25-1.25H5.75z'/>"
    "</svg>";

inline constexpr const char* kEye =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M12 5C6.5 5 2.2 8.7 1.1 12c1.1 3.3 5.4 7 10.9 7s9.8-3.7 10.9-7C21.8 8.7 17.5 5 12 5zm0 11.5a4.5 4.5 0 1 1 0-9 4.5 4.5 0 0 1 0 9zm0-7a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z'/>"
    "</svg>";

inline constexpr const char* kMoreHorizontal =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
    "<path fill='#FFFFFF' d='M6 10.5a1.5 1.5 0 1 1 0 3 1.5 1.5 0 0 1 0-3zm6 0a1.5 1.5 0 1 1 0 3 1.5 1.5 0 0 1 0-3zm6 0a1.5 1.5 0 1 1 0 3 1.5 1.5 0 0 1 0-3z'/>"
    "</svg>";

inline void draw_centered(PaintContext& ctx, const char* svg_xml, Rect container, float icon_size, Color tint) {
    Rect ir = Rect::from_xywh(
        std::round(container.min_x + (container.width() - icon_size) * 0.5f),
        std::round(container.min_y + (container.height() - icon_size) * 0.5f),
        icon_size,
        icon_size);
    SvgEngine::draw_svg_xml(ctx, svg_xml, ir, tint);
}

} // namespace ktui::fluent_icons
