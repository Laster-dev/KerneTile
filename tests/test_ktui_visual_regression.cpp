#include <ktui/ktui.hpp>
#include <kernetile/renderer.hpp>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

#define KTUI_CHECK(expr)                                                               \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
            std::exit(1);                                                              \
        }                                                                              \
    } while (0)

namespace {

constexpr uint32_t kWidth = 320;
constexpr uint32_t kHeight = 240;

uint64_t fnv1a_rgba8(const std::vector<uint8_t>& pixels) {
    uint64_t h = 14695981039346656037ull;
    for (uint8_t b : pixels) {
        h ^= static_cast<uint64_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

double compute_mean_luma(const std::vector<uint8_t>& pixels) {
    const size_t n = pixels.size() / 4;
    if (n == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double r = pixels[i * 4 + 0];
        const double g = pixels[i * 4 + 1];
        const double b = pixels[i * 4 + 2];
        sum += 0.2126 * r + 0.7152 * g + 0.0722 * b;
    }
    return sum / static_cast<double>(n);
}

size_t count_diff_pixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    KTUI_CHECK(a.size() == b.size());
    size_t diff = 0;
    const size_t n = a.size() / 4;
    for (size_t i = 0; i < n; ++i) {
        if (a[i * 4 + 0] != b[i * 4 + 0] ||
            a[i * 4 + 1] != b[i * 4 + 1] ||
            a[i * 4 + 2] != b[i * 4 + 2] ||
            a[i * 4 + 3] != b[i * 4 + 3]) {
            ++diff;
        }
    }
    return diff;
}

std::unique_ptr<kernetile::KerneTileRenderer> make_cpu_renderer() {
    kernetile::RendererConfig cfg{};
    cfg.width = kWidth;
    cfg.height = kHeight;
    cfg.atlas_size = 512;
    cfg.force_cpu_fallback = true;
    return kernetile::KerneTileRenderer::create(cfg);
}

struct TestScene {
    Property<std::string> status{"State: Idle"};
    Property<bool> checked{false};
    WidgetPtr widget;
    std::shared_ptr<RenderBox> root;

    TestScene() {
        widget = Box {
            .padding = EdgeInsets(14.0f),
            .child = VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text {
                        .text = "KerneTile Visual Regression",
                        .font_size = 16.0f,
                        .font_weight = FontWeight::SemiBold
                    },
                    Box {
                        .padding = EdgeInsets(10.0f),
                        .theme_card = true,
                        .child = VStack {
                            .align_items = AlignItems::Stretch,
                            .spacing = 8.0f,
                            .content = {
                                HStack {
                                    .spacing = 8.0f,
                                    .content = {
                                        Button { .text = "Primary", .variant = ButtonVariant::Accent },
                                        Button { .text = "Secondary" }
                                    }
                                },
                                Checkbox { .label = "Enable Tile Culling" }.bind_two_way(checked),
                                Text { .font_size = 12.5f }.bind(&RenderText::set_text, status)
                            }
                        }
                    }
                }
            }
        };
        root = widget->create_render_node();
    }
};

void render_scene_frame(kernetile::KerneTileRenderer& renderer,
                        RenderBox& root,
                        const Theme& theme,
                        std::vector<uint8_t>& out_rgba
#if KTUI_INSPECTOR_ACTIVE
                        , UiInspectorState* inspector = nullptr
#endif
) {
    ThemeScopeGuard scope(theme);
    root.layout(BoxConstraints::tight(Vec2{static_cast<float>(kWidth), static_cast<float>(kHeight)}));
    root.clear_dirty_recursive();

    renderer.begin_frame();
    PaintContext ctx(&renderer, 1.0f);
    const Color bg = theme.is_dark
        ? Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f}
        : Color{0xF3 / 255.0f, 0xF3 / 255.0f, 0xF3 / 255.0f, 1.0f};
    ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, static_cast<float>(kWidth), static_cast<float>(kHeight)), bg);
    root.paint(ctx);
#if KTUI_INSPECTOR_ACTIVE
    if (inspector && inspector->enabled) {
        renderer.mark_scene_end();
        UiInspector::paint_canvas_overlay(ctx, *inspector, &root, 0.0f, 1.0f);
    }
#endif
    renderer.end_frame();

    out_rgba.resize(static_cast<size_t>(kWidth) * kHeight * 4);
    renderer.render_to_rgba8(out_rgba.data());
}

void test_deterministic_dark_and_light_rendering() {
    std::cout << "[Test 1] 离屏渲染确定性 (Pixel-Exact Determinism) 与深浅主题对比度测试..." << std::endl;

    TestScene scene_a;
    TestScene scene_b;
    auto ren_a = make_cpu_renderer();
    auto ren_b = make_cpu_renderer();
    std::vector<uint8_t> dark_a, dark_b, light_a;

    render_scene_frame(*ren_a, *scene_a.root, Theme::dark(), dark_a);
    render_scene_frame(*ren_b, *scene_b.root, Theme::dark(), dark_b);

    // 两个独立渲染器渲染相同声明式树必须像素级 100% 一致
    KTUI_CHECK(fnv1a_rgba8(dark_a) == fnv1a_rgba8(dark_b));
    KTUI_CHECK(count_diff_pixels(dark_a, dark_b) == 0);

    // 切换到浅色主题并验证显著亮度差异
    render_scene_frame(*ren_a, *scene_a.root, Theme::light(), light_a);
    const double luma_dark = compute_mean_luma(dark_a);
    const double luma_light = compute_mean_luma(light_a);
    KTUI_CHECK(luma_dark > 10.0 && luma_dark < 110.0);
    KTUI_CHECK(luma_light > 160.0);
    KTUI_CHECK(luma_light - luma_dark > 80.0);

    std::cout << "  -> 像素哈希完全确定，Dark Luma=" << luma_dark
              << ", Light Luma=" << luma_light << "！" << std::endl;
}

void test_dirty_tile_incremental_equivalence() {
    std::cout << "[Test 2] 脏瓦片增量渲染 vs 全量重绘像素级黄金等价测试..." << std::endl;

    TestScene inc_scene;
    auto inc_ren = make_cpu_renderer();
    std::vector<uint8_t> inc_frame;

    // 帧 1：初始全量渲染写入持久帧缓冲
    render_scene_frame(*inc_ren, *inc_scene.root, Theme::dark(), inc_frame);
    const auto stats1 = inc_ren->get_last_frame_stats();
    KTUI_CHECK(stats1.dirty_tiles == stats1.total_tiles);

    // 帧 2：仅修改非动画状态文本，在同一持久帧缓冲上增量覆写脏瓦片
    inc_scene.status.set("State: Active (80%)");
    render_scene_frame(*inc_ren, *inc_scene.root, Theme::dark(), inc_frame);
    const auto stats2 = inc_ren->get_last_frame_stats();

    // 验证脏瓦片局部剔除确实生效（局部变动绝不应退化为全屏重绘）
    KTUI_CHECK(stats2.dirty_tiles > 0);
    KTUI_CHECK(stats2.dirty_tiles < stats2.total_tiles);
    KTUI_CHECK(inc_ren->get_last_dirty_tiles().size() == stats2.dirty_tiles);

    // 黄金参考帧：用全新渲染器从零渲染同一目标状态
    TestScene ref_scene;
    ref_scene.status.set("State: Active (80%)");
    auto ref_ren = make_cpu_renderer();
    std::vector<uint8_t> ref_frame;
    render_scene_frame(*ref_ren, *ref_scene.root, Theme::dark(), ref_frame);

    // 核心契约：增量脏块合成结果必须与全量重绘 0 像素差异！
    const size_t diff = count_diff_pixels(inc_frame, ref_frame);
    KTUI_CHECK(diff == 0);

    std::cout << "  -> 脏瓦片增量合成 (" << stats2.dirty_tiles << "/" << stats2.total_tiles
              << " tiles, " << stats2.culling_ratio << "% culled) 与全量黄金帧 0 像素差异！" << std::endl;
}

void test_ui_inspector_overlay_rendering() {
#if KTUI_INSPECTOR_ACTIVE
    std::cout << "[Test 3] Debug-Only F12 DevTools 控件树与脏瓦片热力图淡出测试..." << std::endl;

    TestScene scene;
    auto ren = make_cpu_renderer();
    std::vector<uint8_t> base_frame, inspector_frame, inspector_no_tiles_frame;

    render_scene_frame(*ren, *scene.root, Theme::dark(), base_frame);

    // 触发一次局部变更以产生真实的脏瓦片集合
    scene.status.set("State: Inspecting");
    render_scene_frame(*ren, *scene.root, Theme::dark(), base_frame);

    UiInspectorState inspector{};
    inspector.enabled = true;
    inspector.show_dirty_tiles = true;
    UiInspector::capture_frame_snapshot(inspector, ren.get());
    KTUI_CHECK(!inspector.tile_heats.empty());
    KTUI_CHECK(UiInspector::has_active_animations(inspector));
    KTUI_CHECK(UiInspector::count_nodes(scene.root.get()) >= 6);

    // 带画布热力图 + 选中高亮框渲染
    inspector.selected_node = scene.root.get();
    inspector_frame = base_frame;
    render_scene_frame(*ren, *scene.root, Theme::dark(), inspector_frame, &inspector);
    const size_t diff_with_inspector = count_diff_pixels(base_frame, inspector_frame);
    KTUI_CHECK(diff_with_inspector > 1000);

    // 验证 mark_scene_end() 彻底隔离调试浮层：再画一帧无业务变动的画面，场景脏瓦片必须为 0！
    render_scene_frame(*ren, *scene.root, Theme::dark(), inspector_frame, &inspector);
    KTUI_CHECK(ren->get_last_scene_stats().dirty_tiles == 0);

    // 关闭脏瓦片热力图后像素应有明显变化（仍保留选中控件高亮框）
    inspector.show_dirty_tiles = false;
    inspector.tile_heats.clear();
    inspector_no_tiles_frame = inspector_frame;
    render_scene_frame(*ren, *scene.root, Theme::dark(), inspector_no_tiles_frame, &inspector);
    KTUI_CHECK(count_diff_pixels(inspector_frame, inspector_no_tiles_frame) > 50);

    std::cout << "  -> 独立 DevTools 画布覆盖层、热力图淡出与零自激场景隔离全部验证通过！" << std::endl;
#else
    std::cout << "[Test 3] Release 模式 (NDEBUG) 验证：F12 DevTools 已严格零体积剥离，跳过渲染测试。" << std::endl;
#endif
}

} // namespace

int main() {
    std::cout << "=== KTUI 视觉回归与脏瓦片黄金等价测试 ===" << std::endl;
    test_deterministic_dark_and_light_rendering();
    test_dirty_tile_incremental_equivalence();
    test_ui_inspector_overlay_rendering();
    std::cout << "=== 全部视觉回归测试通过 ===" << std::endl;
    return 0;
}
