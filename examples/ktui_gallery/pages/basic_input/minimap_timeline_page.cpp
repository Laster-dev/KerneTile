#include "../gallery_pages.hpp"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

namespace {

// 轻量辅助：捕获任意子控件创建后的 RenderBox 弱引用，供 SpotlightTour 实时定位真实目标控件
class RefCaptureWidget : public Widget {
public:
    RefCaptureWidget(WidgetPtr inner, std::shared_ptr<std::weak_ptr<RenderBox>> slot)
        : m_inner(std::move(inner)), m_slot(std::move(slot)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = m_inner ? m_inner->create_render_node() : nullptr;
        if (m_inner && node) m_inner->set_render_node(node);
        if (m_slot) *m_slot = node;
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (m_slot) *m_slot = node;
        if (m_inner) m_inner->update_render_node(std::move(node));
    }

private:
    WidgetPtr m_inner;
    std::shared_ptr<std::weak_ptr<RenderBox>> m_slot;
};

WidgetPtr with_ref(WidgetPtr child, std::shared_ptr<std::weak_ptr<RenderBox>> slot) {
    return make_widget<RefCaptureWidget>(std::move(child), std::move(slot));
}

} // namespace

// ============================================================================
// 5. 带时间轴缩略图的范围选择器 (MinimapTimelineSlider)
// ============================================================================
WidgetPtr build_minimap_timeline_page(GalleryApp& app) {
    // 48 个半小时监控采样点，包含两处明显异常波峰
    std::vector<float> samples = {
        18, 22, 19, 25, 24, 28, 35, 42, 48, 52, 45, 38,
        40, 44, 58, 96, 88, 62, 49, 44, 41, 39, 36, 34,
        32, 35, 40, 46, 51, 48, 43, 39, 42, 68, 92, 100,
        84, 56, 42, 36, 30, 26, 24, 21, 19, 17, 16, 15
    };

    auto range_text = std::make_shared<Property<std::string>>("当前聚焦窗口: 06:00 ~ 16:48  |  选区内峰值: 96 ms（橙红柱标识异常波峰）");
    auto ref_slot = std::make_shared<std::weak_ptr<RenderBox>>();

    auto fmt_time = [](float ratio) -> std::string {
        int total_min = static_cast<int>(std::round(std::clamp(ratio, 0.0f, 1.0f) * 24.0f * 60.0f));
        int hh = (total_min / 60) % 24;
        int mm = total_min % 60;
        char b[16];
        std::snprintf(b, sizeof(b), "%02d:%02d", hh, mm);
        return b;
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("MinimapTimelineSlider (时间轴缩略图范围选择器)", 24.0f),
            app.make_muted("日志与监控大盘利器：轨道内嵌迷你波形缩略图，异常峰值以橙红色醒目标出；拖拽左右手柄缩放区间，拖拽选区框整体平移聚焦波峰。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    with_ref(MinimapTimelineSlider {
                        .samples = samples,
                        .start_ratio = 0.25f,
                        .end_ratio = 0.70f,
                        .anomaly_threshold = 0.78f,
                        .axis_labels = { "00:00", "06:00", "12:00", "18:00", "24:00" },
                        .on_range_changed = [range_text, samples, fmt_time](float s, float e) {
                            size_t i0 = static_cast<size_t>(std::floor(s * static_cast<float>(samples.size())));
                            size_t i1 = std::min(samples.size(), static_cast<size_t>(std::ceil(e * static_cast<float>(samples.size()))));
                            float peak = 0.0f;
                            for (size_t i = i0; i < i1; ++i) peak = std::max(peak, samples[i]);
                            range_text->set("当前聚焦窗口: " + fmt_time(s) + " ~ " + fmt_time(e) +
                                            "  |  选区内最高波峰: " + std::to_string(static_cast<int>(peak)) + " ms");
                        }
                    }, ref_slot),

                    app.make_accent(*range_text, 12.5f, FontWeight::Medium),

                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "聚焦波峰 #1 (07:30)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderMinimapTimelineSlider*>(b.get())->set_range(0.28f, 0.42f, true);
                                }
                            },
                            Button {
                                .text = "聚焦波峰 #2 (17:30)",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderMinimapTimelineSlider*>(b.get())->set_range(0.66f, 0.82f, true);
                                }
                            },
                            Button {
                                .text = "查看全天 24h",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderMinimapTimelineSlider*>(b.get())->set_range(0.0f, 1.0f, true);
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages
