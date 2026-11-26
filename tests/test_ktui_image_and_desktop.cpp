#include "test_common.hpp"
#include <ktui/ktui.hpp>
#include <kernetile/renderer.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define KT_ASSERT_TRUE(cond) do { if (!(cond)) { std::fprintf(stderr, "ASSERT FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); std::exit(1); } } while(0)
#define KT_ASSERT_EQ(a, b) do { if (!((a) == (b))) { std::fprintf(stderr, "ASSERT_EQ FAILED at %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); std::exit(1); } } while(0)
#define KT_ASSERT_NEAR(a, b, eps) do { if (std::abs((a) - (b)) > (eps)) { std::fprintf(stderr, "ASSERT_NEAR FAILED at %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while(0)

using namespace ktui;

static void test_rgba_color_atlas_and_image_widget() {
    // 1. 构造 4x4 彩色 RGBA8 位图 (左上红、右上绿、左下蓝、右下半透明黄)
    std::vector<uint8_t> pixels(4 * 4 * 4, 0);
    auto set_px = [&](uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        size_t idx = (size_t(y) * 4 + x) * 4;
        pixels[idx + 0] = r;
        pixels[idx + 1] = g;
        pixels[idx + 2] = b;
        pixels[idx + 3] = a;
    };
    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t x = 0; x < 4; ++x) {
            if (x < 2 && y < 2) set_px(x, y, 240, 20, 20, 255);
            else if (x >= 2 && y < 2) set_px(x, y, 20, 220, 40, 255);
            else if (x < 2 && y >= 2) set_px(x, y, 30, 80, 240, 255);
            else set_px(x, y, 250, 220, 20, 128);
        }
    }

    auto img_buf = ImageBuffer::from_rgba8(4, 4, pixels);
    KT_ASSERT_TRUE(img_buf != nullptr && img_buf->valid());

    // 2. 通过 KerneTileRenderer CPU 后端绘制并验证像素输出
    kernetile::RendererConfig cfg{};
    cfg.width = 64;
    cfg.height = 64;
    cfg.force_cpu_fallback = true;
    auto renderer = kernetile::KerneTileRenderer::create(cfg);
    KT_ASSERT_TRUE(renderer != nullptr);

    renderer->begin_frame();
    renderer->draw_rect({0.0f, 0.0f, 64.0f, 64.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
    renderer->draw_image(img_buf->cache_key, {0.0f, 0.0f, 64.0f, 64.0f},
                         img_buf->width, img_buf->height, img_buf->pixels.data(),
                         0.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    renderer->end_frame();

    KT_ASSERT_TRUE(renderer->has_cached_image(img_buf->cache_key));

    std::vector<uint8_t> fb(64 * 64 * 4, 0);
    renderer->render_to_rgba8(fb.data());

    auto px_at = [&](uint32_t x, uint32_t y) -> const uint8_t* {
        return fb.data() + (size_t(y) * 64 + x) * 4;
    };
    // 左上角应偏红
    const uint8_t* tl = px_at(8, 8);
    KT_ASSERT_TRUE(tl[0] > 200 && tl[1] < 50 && tl[2] < 50 && tl[3] == 255);
    // 右上角应偏绿
    const uint8_t* tr = px_at(56, 8);
    KT_ASSERT_TRUE(tr[1] > 180 && tr[0] < 50 && tr[2] < 70 && tr[3] == 255);
    // 左下角应偏蓝
    const uint8_t* bl = px_at(8, 56);
    KT_ASSERT_TRUE(bl[2] > 200 && bl[0] < 60 && bl[3] == 255);
    // 右下角为半透明黄叠在纯黑底上 (~125, ~110, ~10)
    const uint8_t* br = px_at(56, 56);
    KT_ASSERT_TRUE(br[0] > 90 && br[0] < 160 && br[1] > 80 && br[1] < 140 && br[2] < 30);

    // 3. 验证 invalidate_cached_image
    renderer->invalidate_cached_image(img_buf->cache_key);
    KT_ASSERT_TRUE(!renderer->has_cached_image(img_buf->cache_key));

    // 4. 验证 KTUI 声明式 Image 控件布局与绘制 (含 UniformToFill / CornerRadius)
    WidgetPtr w = Image {
        .source = img_buf,
        .width = 48.0f,
        .height = 32.0f,
        .stretch = ImageStretch::UniformToFill,
        .corner_radius = 6.0f,
        .opacity = 0.9f
    };
    auto node = w->create_render_node();
    KT_ASSERT_TRUE(node != nullptr);
    node->layout(BoxConstraints::loose(Vec2{200.0f, 200.0f}));
    KT_ASSERT_NEAR(node->size().x, 48.0f, 0.01f);
    KT_ASSERT_NEAR(node->size().y, 32.0f, 0.01f);

    renderer->begin_frame();
    PaintContext ctx(renderer.get(), 1.0f);
    node->paint(ctx);
    renderer->end_frame();
    KT_ASSERT_TRUE(ctx.primitive_count() >= 1);
}

static void test_file_dialog_mock() {
    FileDialog::set_mock_handler([](const FileDialogOptions& opt) -> FileDialogResult {
        FileDialogResult res;
        res.accepted = true;
        if (opt.kind == FileDialogKind::SelectFolder) {
            res.paths.push_back("C:/Projects/KerneTile");
        } else if (opt.kind == FileDialogKind::SaveFile) {
            res.paths.push_back("C:/Projects/output.png");
        } else {
            res.paths.push_back("C:/Projects/sample.bmp");
        }
        return res;
    });

    bool cb_called = false;
    std::string picked;
    FileDialog::open_file(FileDialogOptions{
        .title = "Open Image",
        .filters = {{"Images", "*.bmp;*.png"}}
    }, [&](const FileDialogResult& r) {
        cb_called = r.accepted;
        picked = r.first_path();
    });
    KT_ASSERT_TRUE(cb_called);
    KT_ASSERT_EQ(picked, std::string("C:/Projects/sample.bmp"));

    auto folder_res = FileDialog::show_sync(FileDialogOptions{
        .kind = FileDialogKind::SelectFolder,
        .title = "Select Workspace"
    });
    KT_ASSERT_TRUE(folder_res.accepted);
    KT_ASSERT_EQ(folder_res.first_path(), std::string("C:/Projects/KerneTile"));

    FileDialog::clear_mock_handler();
}

static void test_os_file_drop_and_widget_drag_drop() {
    auto backend = create_platform_backend(true); // Headless
    Application app(std::move(backend));
    WindowDesc desc{};
    desc.size = {400.0f, 300.0f};
    desc.custom_titlebar = false;
    auto win = app.create_window(desc);
    KT_ASSERT_TRUE(win != nullptr);

    std::shared_ptr<RenderBox> drag_src;
    std::shared_ptr<RenderBox> drop_dst;

    bool started = false;
    bool entered = false;
    bool over_fired = false;
    bool dropped_internal = false;
    bool ended = false;
    std::string received_payload;
    std::vector<std::string> dropped_os_files_widget;
    std::vector<std::string> dropped_os_files_window;

    win->set_content(HStack {
        .spacing = 10.0f,
        .content = {
            Box { .width = 120.0f, .height = 120.0f },
            Box { .width = 150.0f, .height = 120.0f }
        }
    });

    win->on_files_dropped([&](const std::vector<std::string>& files, PointF) {
        dropped_os_files_window = files;
    });

    win->render_frame();
    auto* root = win->root_render_box();
    KT_ASSERT_TRUE(root != nullptr);
    RenderBox* src_box = root->first_child() ? root->first_child()->as_render_box() : nullptr;
    RenderBox* dst_box = (src_box && src_box->next_sibling()) ? src_box->next_sibling()->as_render_box() : nullptr;
    KT_ASSERT_TRUE(src_box != nullptr && dst_box != nullptr);

    src_box->is_draggable = true;
    src_box->on_drag_start = [&](DragEvent& ev) {
        started = true;
        ev.payload.mime_type = "application/x-ktui-card";
        ev.payload.data = "card-42";
        ev.accepted = true;
    };
    src_box->on_drag_end = [&](DragEvent& ev) {
        ended = ev.accepted;
    };

    dst_box->is_drop_target = true;
    dst_box->on_drag_enter = [&](DragEvent&) { entered = true; };
    dst_box->on_drag_over = [&](DragEvent& ev) {
        over_fired = true;
        ev.accepted = true;
    };
    dst_box->on_drop = [&](DragEvent& ev) {
        if (!ev.payload.files.empty()) {
            dropped_os_files_widget = ev.payload.files;
            ev.accepted = true;
        } else if (ev.payload.mime_type == "application/x-ktui-card") {
            dropped_internal = true;
            received_payload = ev.payload.data;
            ev.accepted = true;
        }
    };

    // 1. 测试窗口内控件拖拽 (PointerDown -> Move > 5px -> Move over drop_dst -> PointerUp)
    auto send_ptr = [&](PlatformEventType t, Vec2 pos, PointerButton btn = PointerButton::None) {
        PlatformEvent pe{};
        pe.type = t;
        pe.position = pos;
        pe.button = btn;
        win->handle_platform_event(pe);
    };

    const Vec2 src_center = src_box->global_offset() + Vec2{src_box->size().x * 0.5f, src_box->size().y * 0.5f};
    const Vec2 dst_center = dst_box->global_offset() + Vec2{dst_box->size().x * 0.5f, dst_box->size().y * 0.5f};

    send_ptr(PlatformEventType::PointerDown, src_center, PointerButton::Primary);
    send_ptr(PlatformEventType::PointerMove, src_center + Vec2{20.0f, 0.0f}, PointerButton::Primary);
    KT_ASSERT_TRUE(started);
    KT_ASSERT_TRUE(win->event_dispatcher().is_dragging());

    send_ptr(PlatformEventType::PointerMove, dst_center, PointerButton::Primary);
    KT_ASSERT_TRUE(entered);
    KT_ASSERT_TRUE(over_fired);

    send_ptr(PlatformEventType::PointerUp, dst_center, PointerButton::Primary);
    KT_ASSERT_TRUE(dropped_internal);
    KT_ASSERT_TRUE(ended);
    KT_ASSERT_EQ(received_payload, std::string("card-42"));
    KT_ASSERT_TRUE(!win->event_dispatcher().is_dragging());

    // 2. 测试 OS 外部文件拖放
    PlatformEvent drop_pe{};
    drop_pe.type = PlatformEventType::FileDrop;
    drop_pe.position = dst_center;
    drop_pe.dropped_files = {"D:/docs/readme.md", "D:/img/logo.bmp"};
    win->handle_platform_event(drop_pe);

    KT_ASSERT_EQ(dropped_os_files_widget.size(), size_t(2));
    KT_ASSERT_EQ(dropped_os_files_widget[0], std::string("D:/docs/readme.md"));
    KT_ASSERT_EQ(dropped_os_files_window.size(), size_t(2));
    KT_ASSERT_EQ(dropped_os_files_window[1], std::string("D:/img/logo.bmp"));
}

static void test_optional_tray_and_rich_clipboard() {
    // 1. 测试 SystemTray (无论是否启用原生编译均应安全可用)
    SystemTray tray;
    bool clicked = false;
    uint32_t selected_id = 0;
    tray.on_click([&]() { clicked = true; });
    tray.on_menu_item([&](uint32_t id) { selected_id = id; });

    TrayIconDesc desc{};
    desc.tooltip = "KerneTile Tray Test";
    desc.menu = {
        TrayMenuItem{1, "Show Window", true, false, false},
        TrayMenuItem{0, "", true, false, true},
        TrayMenuItem{2, "Quit", true, false, false}
    };
    bool created = tray.create(desc);
    if (SystemTray::is_compiled_in()) {
        if (created) {
            KT_ASSERT_TRUE(tray.is_active());
            tray.set_tooltip("Updated Tooltip");
            KT_ASSERT_EQ(tray.tooltip(), std::string("Updated Tooltip"));
            tray.simulate_click();
            tray.simulate_menu_select(1);
            KT_ASSERT_TRUE(clicked);
            KT_ASSERT_EQ(selected_id, 1u);
            tray.destroy();
            KT_ASSERT_TRUE(!tray.is_active());
        } else {
            // 无桌面 Explorer 会话 (如 CI/沙箱) 下 Shell_NotifyIconW 可能返回 FALSE，确认状态机安全
            tray.set_tooltip("Updated Tooltip");
            KT_ASSERT_EQ(tray.tooltip(), std::string("Updated Tooltip"));
            tray.destroy();
        }
    } else {
        KT_ASSERT_TRUE(!created);
    }

    // 2. 测试 Rich Clipboard (无 backend 绑定时使用进程内回退，不污染宿主真实剪贴板)
    Clipboard::bind_backend(nullptr);
    ClipboardImage img{};
    img.width = 2;
    img.height = 2;
    img.rgba8 = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 0, 128
    };
    KT_ASSERT_TRUE(Clipboard::set_image(img));
    KT_ASSERT_TRUE(Clipboard::has_image());
    auto got_img = Clipboard::get_image();
    KT_ASSERT_EQ(got_img.width, 2u);
    KT_ASSERT_EQ(got_img.height, 2u);
    KT_ASSERT_EQ(got_img.rgba8.size(), size_t(16));
    KT_ASSERT_EQ(got_img.rgba8[0], uint8_t(255));

    std::vector<std::string> files = {"C:/a.txt", "C:/b.bmp"};
    KT_ASSERT_TRUE(Clipboard::set_files(files));
    KT_ASSERT_TRUE(Clipboard::has_files());
    auto got_files = Clipboard::get_files();
    KT_ASSERT_EQ(got_files.size(), size_t(2));
    KT_ASSERT_EQ(got_files[0], std::string("C:/a.txt"));
}

static void test_stb_jpg_and_animated_frames() {
    // 1. 验证真实 JPG 壁纸解码与 max_dim 降采样
    auto wp = ImageBuffer::load_file("examples/ktui_gallery/wallpaper_dark.jpg", 256);
    KT_ASSERT_TRUE(wp != nullptr && wp->valid());
    KT_ASSERT_TRUE(wp->width <= 256u && wp->height <= 256u);
    KT_ASSERT_TRUE(!wp->is_animated());
    KT_ASSERT_EQ(wp->frame_count(), size_t(1));

    // 2. 验证多帧动图缓冲构造与原地覆写 200 帧无膨胀/无频闪
    std::vector<ImageFrame> frames(3);
    frames[0].pixels = std::vector<uint8_t>(4 * 4 * 4, 0);
    frames[1].pixels = std::vector<uint8_t>(4 * 4 * 4, 0);
    frames[2].pixels = std::vector<uint8_t>(4 * 4 * 4, 0);
    for (int i = 0; i < 16; ++i) {
        frames[0].pixels[i * 4 + 0] = 240; frames[0].pixels[i * 4 + 3] = 255; // R
        frames[1].pixels[i * 4 + 1] = 240; frames[1].pixels[i * 4 + 3] = 255; // G
        frames[2].pixels[i * 4 + 2] = 240; frames[2].pixels[i * 4 + 3] = 255; // B
    }
    frames[0].delay_ms = frames[1].delay_ms = frames[2].delay_ms = 50;
    auto anim = ImageBuffer::from_frames(4, 4, std::move(frames));
    KT_ASSERT_TRUE(anim != nullptr && anim->valid());
    KT_ASSERT_TRUE(anim->is_animated());
    KT_ASSERT_EQ(anim->frame_count(), size_t(3));

    kernetile::RendererConfig cfg{};
    cfg.width = 64;
    cfg.height = 64;
    cfg.force_cpu_fallback = true;
    auto r = kernetile::KerneTileRenderer::create(cfg);
    std::vector<uint8_t> fb(64 * 64 * 4, 0);
    for (uint32_t step = 0; step < 200; ++step) {
        uint32_t fi = step % 3u;
        r->begin_frame();
        r->draw_image(anim->cache_key, {8.0f, 8.0f, 24.0f, 24.0f}, 4, 4,
                      anim->frames[fi].pixels.data(), 0.0f, {1, 1, 1, 1}, {0, 0, 1, 1}, fi);
        r->end_frame();
        r->render_to_rgba8(fb.data());
        const size_t px_off = (16u * 64u + 16u) * 4u;
        if (fi == 0) KT_ASSERT_TRUE(fb[px_off + 0] > 200 && fb[px_off + 1] < 20);
        if (fi == 1) KT_ASSERT_TRUE(fb[px_off + 1] > 200 && fb[px_off + 0] < 20);
        if (fi == 2) KT_ASSERT_TRUE(fb[px_off + 2] > 200 && fb[px_off + 0] < 20);
    }
}

int main() {
    configure_test_environment();
    test_rgba_color_atlas_and_image_widget();
    test_stb_jpg_and_animated_frames();
    test_file_dialog_mock();
    test_os_file_drop_and_widget_drag_drop();
    test_optional_tray_and_rich_clipboard();
    std::printf("[PASS] test_ktui_image_and_desktop: all assertions passed!\n");
    return 0;
}
