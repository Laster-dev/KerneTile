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
// 7. 交互式新手引导聚光灯 (SpotlightTour)
// ============================================================================
WidgetPtr build_spotlight_tour_page(GalleryApp& app) {
    auto tour = std::make_shared<SpotlightTourController>();
    auto tour_status = std::make_shared<Property<std::string>>("点击「启动交互式新手引导」，在聚光灯高亮镂空框内直接点击真实按钮即可自动推进下一步！");

    auto step1_ref = std::make_shared<std::weak_ptr<RenderBox>>();
    auto step2_ref = std::make_shared<std::weak_ptr<RenderBox>>();
    auto step3_ref = std::make_shared<std::weak_ptr<RenderBox>>();

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            HStack {
                .justify = JustifyContent::SpaceBetween,
                .align_items = AlignItems::Center,
                .content = {
                    VStack {
                        .spacing = 3.0f,
                        .content = {
                            app.make_page_title("SpotlightTour (交互式新手引导聚光灯)", 24.0f),
                            app.make_muted("Driver.js / Shepherd.js 风格全屏聚光灯：呼吸光晕镂空区完全放行真实点击，用户完成该步真实操作后引导气泡自动滑向下一目标。", 13.0f)
                        }
                    },
                    Button {
                        .text = "新手引导",
                        .variant = ButtonVariant::Accent,
                        .on_click = [tour, tour_status, step1_ref, step2_ref, step3_ref]() {
                            std::vector<SpotlightStep> steps = {
                                SpotlightStep::for_node("第 1 步：创建数据源",
                                    "点击下方高亮的真实按钮「① 创建数据源」，即可完成初始化并自动跳到下一步。",
                                    *step1_ref),
                                SpotlightStep::for_node("第 2 步：开启实时同步",
                                    "点击高亮按钮「② 启用实时同步」，聚光灯将平滑滑动到最后一步。",
                                    *step2_ref),
                                SpotlightStep::for_node("第 3 步：发布上线",
                                    "点击「③ 发布仪表盘」完成整套交互式新手引导！",
                                    *step3_ref)
                            };
                            tour->start(std::move(steps),
                                [tour_status](size_t idx) {
                                    tour_status->set("引导进行中：当前停留在第 " + std::to_string(idx + 1) + " / 3 步（可直接点击镂空区内的真实按钮）");
                                },
                                [tour_status](bool completed) {
                                    tour_status->set(completed ? "✓ 恭喜！你已通过真实交互完成全部 3 步新手引导！"
                                                               : "已跳过本次新手引导，可随时重新启动。");
                                });
                            tour_status->set("引导已启动：当前第 1 / 3 步，请直接点击高亮镂空框中的「① 创建数据源」按钮！");
                        }
                    }
                }
            },

            app.make_accent(*tour_status, 12.5f, FontWeight::Medium),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 14.0f,
                .content = {
                    Text { .text = "真实交互演示工作台（引导过程中以下 3 个按钮可直接在聚光灯镂空区内被点击）", .font_size = 13.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .spacing = 16.0f,
                        .content = {
                            with_ref(Button {
                                .text = "① 创建数据源",
                                .on_click = [tour, tour_status]() {
                                    tour_status->set("已点击真实按钮「① 创建数据源」！");
                                    if (tour->is_active() && tour->current_step() == 0) tour->advance();
                                }
                            }, step1_ref),

                            with_ref(Button {
                                .text = "② 启用实时同步",
                                .on_click = [tour, tour_status]() {
                                    tour_status->set("已点击真实按钮「② 启用实时同步」！");
                                    if (tour->is_active() && tour->current_step() == 1) tour->advance();
                                }
                            }, step2_ref),

                            with_ref(Button {
                                .text = "③ 发布仪表盘",
                                .variant = ButtonVariant::Accent,
                                .on_click = [tour, tour_status]() {
                                    tour_status->set("已点击真实按钮「③ 发布仪表盘」！");
                                    if (tour->is_active() && tour->current_step() == 2) tour->advance();
                                }
                            }, step3_ref)
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages
