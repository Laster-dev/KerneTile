#pragma once

// ==============================================================================
// 严格仅 Debug 模式生效的内置 F12 DevTools 观测层
// Release 模式 (NDEBUG) 下强制 KTUI_INSPECTOR_ACTIVE == 0，零代码、零内存、零体积影响！
// 架构：
//   - 目标窗口仅绘制画布覆盖层（脏瓦片热力淡出图 + 选中/拾取控件高亮框）
//   - DevTools 界面本身作为独立 ktui::Window，100% 由声明式 KTUI 控件构建（devtools_window.cpp）
// ==============================================================================
#if defined(KTUI_ENABLE_INSPECTOR) && KTUI_ENABLE_INSPECTOR && !defined(NDEBUG)
#define KTUI_INSPECTOR_ACTIVE 1
#else
#define KTUI_INSPECTOR_ACTIVE 0
#endif

#if KTUI_INSPECTOR_ACTIVE

#include <ktui/presentation/paint_context.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/event/hit_test_tree.hpp>
#include <kernetile/renderer.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ktui {

class Window;
struct DevToolsWindowHost;

struct DirtyTileHeat {
    uint32_t tx{0};
    uint32_t ty{0};
    float heat{0.0f}; // 0.0 -> 消失, 0.48 -> 单次翡翠绿闪现, 0.75 -> 暖琥珀橙, 1.0 -> 高频深红
};

struct UiInspectorState {
    bool enabled{false};
    bool is_devtools_window{false};
    bool show_dirty_tiles{true};
    bool pick_mode{false};

    // 目标窗口当前高亮节点
    RenderBox* selected_node{nullptr};
    RenderBox* hovered_node{nullptr};

    // 上一帧纯业务场景快照（已通过 mark_scene_end 隔离调试层，彻底杜绝自反馈循环）
    kernetile::FrameStats prev_stats{};
    bool prev_hw_accel{false};

    // 实时帧率统计（基于 0.4s 滑动窗口 + 瞬时帧间隔）
    float fps{0.0f};
    uint32_t fps_frame_counter{0};
    std::chrono::steady_clock::time_point fps_window_start{};
    std::chrono::steady_clock::time_point last_frame_time{};

    // 脏瓦片热力图淡入淡出表（高频重绘升温变深红，静止后平滑淡出）
    std::unordered_map<uint64_t, DirtyTileHeat> tile_heats;
    std::chrono::steady_clock::time_point last_tick{};

    // 独立 KTUI DevTools 窗口宿主（实现在 KTUI/src/app/devtools_window.cpp）
    std::shared_ptr<DevToolsWindowHost> devtools_host;
};

class UiInspector {
public:
    static constexpr uint32_t kKeyF12Win32 = 123u;     // VK_F12
    static constexpr uint32_t kKeyF12X11   = 0xFFC9u;  // XK_F12

    [[nodiscard]] static bool is_f12_key(uint32_t key_code) noexcept {
        return key_code == kKeyF12Win32 || key_code == kKeyF12X11;
    }

    static void forget_node(UiInspectorState& state, const RenderBox* box) noexcept {
        if (!box) return;
        if (state.selected_node == box) state.selected_node = nullptr;
        if (state.hovered_node == box) state.hovered_node = nullptr;
    }

    [[nodiscard]] static bool has_active_animations(const UiInspectorState& state) noexcept {
        return state.enabled && state.show_dirty_tiles && !state.tile_heats.empty();
    }

    [[nodiscard]] static uint32_t count_nodes(const RenderNode* node) noexcept {
        if (!node) return 0u;
        uint32_t total = 1u;
        for (const RenderNode* c = node->first_child(); c != nullptr; c = c->next_sibling()) {
            total += count_nodes(c);
        }
        return total;
    }

    [[nodiscard]] static const char* infer_node_tag(const RenderBox* box) noexcept {
        if (!box) return "Node";
        if (const_cast<RenderBox*>(box)->as_text_input_client() != nullptr) return "TextInput";
        if (box->as_positioned() != nullptr) return "Positioned";
        if (box->is_focusable && box->child_count() == 0) return "Control";
        if (box->cursor_shape == CursorShape::PointingHand) return "Interactive";
        if (box->child_count() > 0) return box->is_relayout_boundary() ? "Container*" : "Container";
        return "Leaf";
    }

    static void capture_frame_snapshot(UiInspectorState& state,
                                       const kernetile::KerneTileRenderer* renderer) {
        if (!renderer) return;
        const auto now = std::chrono::steady_clock::now();

        // 1. 实时 FPS 滑动窗口统计
        if (state.fps_window_start.time_since_epoch().count() == 0) {
            state.fps_window_start = now;
            state.last_frame_time = now;
            state.fps_frame_counter = 1;
        } else {
            state.fps_frame_counter++;
            const float win_sec = std::chrono::duration<float>(now - state.fps_window_start).count();
            if (win_sec >= 0.35f) {
                state.fps = static_cast<float>(state.fps_frame_counter) / win_sec;
                state.fps_window_start = now;
                state.fps_frame_counter = 0;
            } else if (state.fps <= 0.01f) {
                const float frame_dt = std::chrono::duration<float>(now - state.last_frame_time).count();
                if (frame_dt > 0.0005f) state.fps = std::min(1000.0f, 1.0f / frame_dt);
            }
            state.last_frame_time = now;
        }

        // 2. 热力瓦片平滑衰减（约 0.8s 柔和淡出）
        float dt = 0.016f;
        if (state.last_tick.time_since_epoch().count() != 0) {
            dt = std::clamp(std::chrono::duration<float>(now - state.last_tick).count(), 0.0f, 0.1f);
        }
        state.last_tick = now;

        for (auto it = state.tile_heats.begin(); it != state.tile_heats.end(); ) {
            it->second.heat -= dt * 1.25f;
            if (it->second.heat <= 0.015f) {
                it = state.tile_heats.erase(it);
            } else {
                ++it;
            }
        }

        // 3. 注入上一帧纯业务场景脏瓦片：单次呈翡翠绿，高频连续刷新升温至橙/深红
        state.prev_stats = renderer->get_last_scene_stats();
        state.prev_hw_accel = renderer->is_hardware_accelerated();
        const auto& dirty = renderer->get_last_scene_dirty_tiles();
        if (state.show_dirty_tiles && !dirty.empty()) {
            const bool is_full = (state.prev_stats.total_tiles > 0 &&
                                  dirty.size() >= state.prev_stats.total_tiles * 9 / 10);
            const float bump = is_full ? 0.32f : 0.48f;
            const size_t cap = std::min<size_t>(dirty.size(), 4096u);
            for (size_t i = 0; i < cap; ++i) {
                const auto& d = dirty[i];
                const uint64_t key = (static_cast<uint64_t>(d.tx) << 32) | static_cast<uint64_t>(d.ty);
                auto& entry = state.tile_heats[key];
                entry.tx = d.tx;
                entry.ty = d.ty;
                entry.heat = std::min(1.0f, entry.heat + bump);
            }
        }
    }

    static Color heat_to_fill_color(float heat) noexcept {
        const float h = std::clamp(heat, 0.0f, 1.0f);
        const float alpha = std::clamp(h * 0.44f, 0.03f, 0.44f);
        if (h <= 0.45f) {
            return Color{0.10f, 0.86f, 0.48f, alpha}; // 单次闪现：清透翡翠绿
        } else if (h <= 0.75f) {
            const float t = (h - 0.45f) / 0.30f;
            return Color{0.10f + 0.90f * t, 0.86f - 0.24f * t, 0.48f * (1.0f - t), alpha}; // 中频：暖琥珀橙
        } else {
            const float t = (h - 0.75f) / 0.25f;
            return Color{1.0f, 0.62f * (1.0f - t) + 0.10f * t, 0.12f * t, std::clamp(alpha * 1.25f, 0.10f, 0.56f)}; // 高频：深绯红
        }
    }

    static Color heat_to_border_color(float heat) noexcept {
        Color c = heat_to_fill_color(heat);
        c.a = std::clamp(heat * 0.88f, 0.06f, 0.88f);
        return c;
    }

    // 仅在目标窗口画布上绘制极轻覆盖层（热力脏瓦片 + 选中/拾取控件高亮框），不塞任何手绘面板
    static void paint_canvas_overlay(PaintContext& ctx,
                                     const UiInspectorState& state,
                                     RenderBox* root,
                                     float titlebar_h,
                                     float render_scale) {
        if (!state.enabled || !root) return;

        const float inv_scale = (render_scale > 0.01f) ? (1.0f / render_scale) : 1.0f;

        // 1. 脏瓦片热力淡入淡出图
        if (state.show_dirty_tiles && !state.tile_heats.empty()) {
            const float tile_sz = 16.0f * inv_scale;
            for (const auto& [key, entry] : state.tile_heats) {
                (void)key;
                if (entry.heat <= 0.015f) continue;
                Rect tr = Rect::from_xywh(
                    static_cast<float>(entry.tx) * tile_sz,
                    static_cast<float>(entry.ty) * tile_sz,
                    tile_sz, tile_sz);
                ctx.draw_box(tr, heat_to_fill_color(entry.heat), heat_to_border_color(entry.heat), 0.0f, 1.0f);
            }
        }

        // 2. 选中/拾取控件高亮框与父容器轮廓
        RenderBox* highlight = (state.pick_mode && state.hovered_node) ? state.hovered_node : state.selected_node;
        if (highlight) {
            if (RenderNode* pnode = highlight->parent()) {
                if (auto* pbox = pnode->as_render_box()) {
                    Vec2 ppos = pbox->global_offset();
                    Rect pr = Rect::from_xywh(ppos.x, ppos.y + titlebar_h, pbox->size().x, pbox->size().y);
                    ctx.draw_rect(pr, Color{1.0f, 0.72f, 0.18f, 0.65f}, 1.0f, 1.0f);
                }
            }
            Vec2 gpos = highlight->global_offset();
            Rect hr = Rect::from_xywh(gpos.x, gpos.y + titlebar_h, highlight->size().x, highlight->size().y);
            ctx.draw_box(hr, Color{0.12f, 0.62f, 1.0f, 0.20f}, Color{0.20f, 0.78f, 1.0f, 0.95f}, 1.0f, 1.5f);
        }
    }

    // 仅在 Pick 模式下处理目标画布的悬停预览与点击拾取
    static bool handle_canvas_event(UiInspectorState& state,
                                    const PlatformEvent& pe,
                                    RenderBox* root,
                                    float titlebar_h) {
        if (!state.enabled || !state.pick_mode || !root) return false;
        const PointF client_pt{pe.position.x, pe.position.y - titlebar_h};
        if (pe.type == PlatformEventType::PointerMove) {
            HitTestResult hit;
            if (client_pt.x >= 0.0f && client_pt.y >= 0.0f && root->hit_test(hit, client_pt)) {
                state.hovered_node = hit.first_target();
            } else {
                state.hovered_node = nullptr;
            }
            return true;
        }
        if (pe.type == PlatformEventType::PointerDown && pe.button == PointerButton::Primary) {
            HitTestResult hit;
            if (client_pt.x >= 0.0f && client_pt.y >= 0.0f && root->hit_test(hit, client_pt)) {
                state.selected_node = hit.first_target();
            }
            state.hovered_node = nullptr;
            state.pick_mode = false;
            return true;
        }
        return false;
    }

    // 独立 KTUI DevTools 窗口生命周期管理（实现在 KTUI/src/app/devtools_window.cpp）
    static void toggle_devtools_window(Window* target);
    static void sync_devtools_window(Window* target, bool force = false);
    static void close_devtools_window(Window* target);
};

} // namespace ktui

#endif // KTUI_INSPECTOR_ACTIVE
