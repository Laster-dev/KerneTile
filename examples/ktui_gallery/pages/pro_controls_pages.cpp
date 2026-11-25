#include "gallery_pages.hpp"
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
// 1. 结构化查询构建器 (QueryFilterBuilder)
// ============================================================================
WidgetPtr build_query_filter_page(GalleryApp& app) {
    auto expr_text = std::make_shared<Property<std::string>>("当前查询表达式: status:open priority:high");
    auto ref_slot = std::make_shared<std::weak_ptr<RenderBox>>();

    std::vector<QueryFilterField> schema = {
        QueryFilterField{ "status",   "工单状态", { "open", "in_progress", "review", "closed" }, Color{0.10f, 0.65f, 0.95f, 1.0f} },
        QueryFilterField{ "priority", "优先级",   { "urgent", "high", "medium", "low" },          Color{0.94f, 0.45f, 0.14f, 1.0f} },
        QueryFilterField{ "owner",    "负责人",   { "alice", "bob", "charlie", "me" },            Color{0.55f, 0.42f, 0.96f, 1.0f} },
        QueryFilterField{ "env",      "部署环境", { "prod", "staging", "dev" },                   Color{0.12f, 0.72f, 0.45f, 1.0f} }
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("QueryFilterBuilder (结构化查询构建器)", 24.0f),
            app.make_muted("Linear / Jira / GitHub Issues 风格单行分词筛选框：输入 status: 自动弹出枚举下拉，选中化为胶囊，支持方向键/退格逐个删除与纯文本表达式双向切换。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    with_ref(QueryFilterBuilder {
                        .schema = schema,
                        .initial_tokens = {
                            QueryFilterToken{ "status", "open" },
                            QueryFilterToken{ "priority", "high" }
                        },
                        .placeholder = "输入 status: / priority: / owner: 或点击展开字段建议...",
                        .on_changed = [expr_text](const std::vector<QueryFilterToken>& tokens, const std::string& expr) {
                            expr_text->set("已匹配 " + std::to_string(tokens.size()) + " 个条件  |  表达式: " + (expr.empty() ? "(空)" : expr));
                        }
                    }, ref_slot),

                    app.make_accent(*expr_text, 12.5f, FontWeight::Medium),

                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "+ env:prod",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderQueryFilterBuilder*>(b.get())->add_token("env", "prod");
                                }
                            },
                            Button {
                                .text = "+ owner:alice",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderQueryFilterBuilder*>(b.get())->add_token("owner", "alice");
                                }
                            },
                            Button {
                                .text = "切换 胶囊 / 表达式",
                                .variant = ButtonVariant::Accent,
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderQueryFilterBuilder*>(b.get())->toggle_raw_mode();
                                }
                            },
                            Button {
                                .text = "清空筛选",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderQueryFilterBuilder*>(b.get())->clear_all();
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

// ============================================================================
// 2. 微调 Scrubbing 数字输入框 (ScrubNumberBox)
// ============================================================================
WidgetPtr build_scrub_number_box_page(GalleryApp& app) {
    auto summary = std::make_shared<Property<std::string>>("当前参数: X = 120.0 px,  Rotation = 15.0 °,  Opacity = 85 %,  Blur = 12.0 px");
    auto state = std::make_shared<std::array<double, 4>>(std::array<double, 4>{120.0, 15.0, 85.0, 12.0});

    auto refresh = [summary, state]() {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "当前参数: X = %.1f px,  Rotation = %.1f °,  Opacity = %.0f %%,  Blur = %.1f px",
                      (*state)[0], (*state)[1], (*state)[2], (*state)[3]);
        summary->set(buf);
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("ScrubNumberBox (微调 Scrubbing 数字框)", 24.0f),
            app.make_muted("Blender / Figma 风格参数输入框：按住左右拖动无级调节数值，按住 Shift 10× 加速，Alt 0.1× 精细微调；单击进入键盘编辑，双击恢复默认值。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Grid {
                        .columns = 2,
                        .column_spacing = 12.0f,
                        .row_spacing = 10.0f,
                        .content = {
                            ScrubNumberBox {
                                .label = "Position X", .value = 120.0, .min_value = -500.0, .max_value = 500.0, .step = 1.0, .precision = 1, .unit = "px",
                                .on_changed = [state, refresh](double v) { (*state)[0] = v; refresh(); }
                            },
                            ScrubNumberBox {
                                .label = "Rotation", .value = 15.0, .min_value = -180.0, .max_value = 180.0, .step = 0.5, .precision = 1, .unit = "°",
                                .on_changed = [state, refresh](double v) { (*state)[1] = v; refresh(); }
                            },
                            ScrubNumberBox {
                                .label = "Opacity", .value = 85.0, .min_value = 0.0, .max_value = 100.0, .step = 1.0, .precision = 0, .unit = "%",
                                .on_changed = [state, refresh](double v) { (*state)[2] = v; refresh(); }
                            },
                            ScrubNumberBox {
                                .label = "Blur Radius", .value = 12.0, .min_value = 0.0, .max_value = 64.0, .step = 0.5, .precision = 1, .unit = "px",
                                .on_changed = [state, refresh](double v) { (*state)[3] = v; refresh(); }
                            }
                        }
                    },
                    app.make_accent(*summary, 12.5f, FontWeight::Medium)
                }
            })
        }
    });
}

// ============================================================================
// 3. 防误触“长按确认”按钮 (HoldConfirmButton)
// ============================================================================
WidgetPtr build_hold_confirm_button_page(GalleryApp& app) {
    auto log_text = std::make_shared<Property<std::string>>("操作日志: 尚未触发任何高危操作（按住按钮满时长触发，中途松开将自动回弹取消）");
    auto count = std::make_shared<int>(0);

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("HoldConfirmButton (防误触长按确认按钮)", 24.0f),
            app.make_muted("无需弹窗打断心流：支持胶囊进度条与圆形 360° 进度环两种形态，按住满时长触发高危操作，中途松手平滑回弹取消。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 14.0f,
                .content = {
                    Text { .text = "胶囊进度条样式 (PillBar)", .font_size = 13.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .spacing = 12.0f,
                        .content = {
                            HoldConfirmButton {
                                .text = "长按 1.5s 删除生产数据库",
                                .holding_text = "正在解锁删除",
                                .confirmed_text = "✓ 已安全执行删除",
                                .hold_duration_sec = 1.5f,
                                .danger = true,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 已确认执行「删除生产数据库」");
                                }
                            },
                            HoldConfirmButton {
                                .text = "长按 1.0s 清空 Redis 缓存",
                                .holding_text = "按住清空缓存",
                                .confirmed_text = "✓ 缓存已清空",
                                .hold_duration_sec = 1.0f,
                                .danger = true,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 已确认执行「清空 Redis 缓存」");
                                }
                            },
                            HoldConfirmButton {
                                .text = "长按 1.2s 发布到线上集群",
                                .holding_text = "正在触发发布",
                                .confirmed_text = "✓ 发布流水线已启动",
                                .hold_duration_sec = 1.2f,
                                .danger = false,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 已确认执行「发布到线上集群」");
                                }
                            }
                        }
                    },
                    Text { .text = "圆形进度环样式 (CircularRing)", .font_size = 13.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 20.0f,
                        .content = {
                            HoldConfirmButton {
                                .text = "删除",
                                .hold_duration_sec = 1.5f,
                                .danger = true,
                                .shape = HoldConfirmShape::CircularRing,
                                .diameter = 64.0f,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 圆形长按确认「删除」已执行");
                                }
                            },
                            HoldConfirmButton {
                                .text = "重启",
                                .hold_duration_sec = 1.2f,
                                .danger = false,
                                .shape = HoldConfirmShape::CircularRing,
                                .diameter = 64.0f,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 圆形长按确认「重启服务」已执行");
                                }
                            },
                            HoldConfirmButton {
                                .text = "销毁",
                                .hold_duration_sec = 1.8f,
                                .danger = true,
                                .shape = HoldConfirmShape::CircularRing,
                                .diameter = 74.0f,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 圆形长按确认「销毁实例」已执行");
                                }
                            }
                        }
                    },
                    app.make_accent(*log_text, 12.5f, FontWeight::Medium)
                }
            })
        }
    });
}

// ============================================================================
// 4. 增量式幽灵文本补全 (GhostTextBox)
// ============================================================================
WidgetPtr build_ghost_text_box_page(GalleryApp& app) {
    auto status_text = std::make_shared<Property<std::string>>("提示: 尝试输入 \"SELECT \"、\"git \" 或 \"kubectl \"，然后按 Tab 键一键上屏（按 Esc 忽略）");
    auto ref_slot = std::make_shared<std::weak_ptr<RenderBox>>();

    std::vector<std::string> suggestions = {
        "SELECT id, status, latency_ms FROM requests WHERE latency_ms > 200 ORDER BY id DESC;",
        "git checkout -b feat/fluent-dock-panel",
        "git commit -m \"feat: add ghost text autocomplete\"",
        "kubectl get pods -n production --watch",
        "docker compose up -d --build"
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("GhostTextBox (增量式幽灵文本补全)", 24.0f),
            app.make_muted("GitHub Copilot 风格行内半透明补全提示：每次按 Tab 仅补全到下一个空格（逐词补全），方便随时改写后续参数，按 Esc 忽略。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    with_ref(GhostTextBox {
                        .text = "SELECT ",
                        .placeholder = "输入 SELECT / git / kubectl / docker 体验行内幽灵补全...",
                        .suggestions = suggestions,
                        .on_changed = [status_text](const std::string& val) {
                            status_text->set("当前输入: " + (val.empty() ? "(空)" : val));
                        },
                        .on_accepted = [status_text](const std::string& val) {
                            status_text->set("✓ 已按 Tab 接受幽灵补全: " + val);
                        }
                    }, ref_slot),

                    app.make_accent(*status_text, 12.5f, FontWeight::Medium),

                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "试试 \"SELECT \"",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderGhostTextBox*>(b.get())->set_text("SELECT ", true);
                                }
                            },
                            Button {
                                .text = "试试 \"git ch\"",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderGhostTextBox*>(b.get())->set_text("git ch", true);
                                }
                            },
                            Button {
                                .text = "试试 \"kubectl \"",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderGhostTextBox*>(b.get())->set_text("kubectl ", true);
                                }
                            },
                            Button {
                                .text = "Tab 逐词补全",
                                .variant = ButtonVariant::Accent,
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderGhostTextBox*>(b.get())->accept_next_word();
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

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

// ============================================================================
// 6. IP 端口输入控件 (IpPortInput)
// ============================================================================
WidgetPtr build_ip_port_input_page(GalleryApp& app) {
    auto ep_text = std::make_shared<Property<std::string>>("当前服务地址: 192.168.1.1:8080  (LAN)");
    auto ref_slot = std::make_shared<std::weak_ptr<RenderBox>>();

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("IpPortInput (IPv4 + 端口分段输入控件)", 24.0f),
            app.make_muted("原生支持 4 段 IPv4（0~255）+ 端口（0~65535）分段输入：输满 3 位或按 '.' 自动跳下一段，按 ':' 直达端口，Backspace 自动回跳，上下键微调并实时识别网段类型。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            Text { .text = "服务端点 (IP:Port):", .font_size = 13.0f, .font_weight = FontWeight::Medium },
                            with_ref(IpPortInput {
                                .ip = "192.168.1.1",
                                .port = 8080,
                                .show_port = true,
                                .on_changed = [ep_text, ref_slot](const std::string& ip, uint16_t port) {
                                    std::string cat = "LAN";
                                    if (auto b = ref_slot->lock()) cat = static_cast<RenderIpPortInput*>(b.get())->address_category();
                                    ep_text->set("当前服务地址: " + ip + ":" + std::to_string(port) + "  (" + cat + ")");
                                }
                            }, ref_slot)
                        }
                    },

                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            Text { .text = "纯 IPv4 网关地址:", .font_size = 13.0f, .font_weight = FontWeight::Medium },
                            IpPortInput { .ip = "10.0.0.1", .port = 0, .show_port = false }
                        }
                    },

                    app.make_accent(*ep_text, 12.5f, FontWeight::Medium),

                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "本地调试 127.0.0.1:3000",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderIpPortInput*>(b.get())->set_address("127.0.0.1", 3000, true);
                                }
                            },
                            Button {
                                .text = "内网服务 192.168.1.100:443",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderIpPortInput*>(b.get())->set_address("192.168.1.100", 443, true);
                                }
                            },
                            Button {
                                .text = "公网 DNS 8.8.8.8:53",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderIpPortInput*>(b.get())->set_address("8.8.8.8", 53, true);
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

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
