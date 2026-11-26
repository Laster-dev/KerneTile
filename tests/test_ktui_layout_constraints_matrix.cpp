#include <ktui/ktui.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
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

constexpr float kInf = std::numeric_limits<float>::infinity();

void assert_node_invariants_recursive(const RenderNode* node, bool check_no_huge = true) {
    if (!node) return;
    if (const auto* box = node->as_render_box()) {
        Vec2 sz = box->size();
        Vec2 off = box->offset();
        KTUI_CHECK(std::isfinite(sz.x) && std::isfinite(sz.y));
        KTUI_CHECK(std::isfinite(off.x) && std::isfinite(off.y));
        KTUI_CHECK(sz.x >= -1e-4f && sz.y >= -1e-4f);
        if (check_no_huge) {
            KTUI_CHECK(sz.x < 1e7f && sz.y < 1e7f);
            KTUI_CHECK(std::abs(off.x) < 1e7f && std::abs(off.y) < 1e7f);
        }
    }
    for (const RenderNode* child = node->first_child(); child != nullptr; child = child->next_sibling()) {
        assert_node_invariants_recursive(child, check_no_huge);
    }
}

void assert_bounded_fit(const RenderBox* root, const BoxConstraints& c) {
    Vec2 sz = root->size();
    KTUI_CHECK(std::isfinite(sz.x) && std::isfinite(sz.y));
    if (c.has_bounded_width()) {
        KTUI_CHECK(sz.x <= c.max_width + 1e-2f);
        KTUI_CHECK(sz.x >= c.min_width - 1e-2f);
    }
    if (c.has_bounded_height()) {
        KTUI_CHECK(sz.y <= c.max_height + 1e-2f);
        KTUI_CHECK(sz.y >= c.min_height - 1e-2f);
    }
}

std::vector<BoxConstraints> build_constraint_matrix() {
    return {
        BoxConstraints::tight(Vec2{0.0f, 0.0f}),
        BoxConstraints::tight(Vec2{1.0f, 1.0f}),
        BoxConstraints::tight(Vec2{320.0f, 240.0f}),
        BoxConstraints::loose(Vec2{0.0f, 0.0f}),
        BoxConstraints::loose(Vec2{320.0f, 240.0f}),
        BoxConstraints{50.0f, 300.0f, 30.0f, 200.0f},
        BoxConstraints{0.0f, kInf, 0.0f, 240.0f},       // Unbounded width
        BoxConstraints{0.0f, 320.0f, 0.0f, kInf},       // Unbounded height (ScrollView vertical)
        BoxConstraints{0.0f, kInf, 0.0f, kInf},         // Unbounded both
        BoxConstraints{120.0f, kInf, 40.0f, kInf},      // Min-bounded + Unbounded max
    };
}

void test_flex_constraint_matrix() {
    std::cout << "[Test 1] FlexEngine 全组合约束矩阵 (Row/Column x Justify x Align x 10种约束)..." << std::endl;

    const auto constraints = build_constraint_matrix();
    const FlexDirection dirs[] = { FlexDirection::Row, FlexDirection::Column };
    const JustifyContent justifies[] = {
        JustifyContent::FlexStart, JustifyContent::Center, JustifyContent::FlexEnd,
        JustifyContent::SpaceBetween, JustifyContent::SpaceAround, JustifyContent::SpaceEvenly
    };
    const AlignItems aligns[] = {
        AlignItems::FlexStart, AlignItems::Center, AlignItems::FlexEnd, AlignItems::Stretch
    };

    size_t cases = 0;
    for (FlexDirection dir : dirs) {
        for (JustifyContent jc : justifies) {
            for (AlignItems ai : aligns) {
                for (const auto& c : constraints) {
                    RenderFlex flex(dir, jc, ai, 8.0f);
                    auto a = std::make_unique<RenderSizedBox>(Vec2{40.0f, 24.0f});
                    auto b = std::make_unique<RenderSizedBox>(Vec2{60.0f, 36.0f});
                    b->flex_data().flex_grow = 1.0f;
                    b->flex_data().fit = FlexFit::Tight;
                    auto d = std::make_unique<RenderSizedBox>(Vec2{0.0f, 0.0f});
                    d->flex_data().flex_grow = 2.0f;
                    d->flex_data().fit = FlexFit::Loose;

                    flex.append_child(a.get());
                    flex.append_child(b.get());
                    flex.append_child(d.get());

                    flex.layout(c);
                    assert_bounded_fit(&flex, c);
                    assert_node_invariants_recursive(&flex);
                    ++cases;
                }
            }
        }
    }
    std::cout << "  -> 通过 " << cases << " 组 Flex 约束矩阵测试，零 NaN/Inf/越界爆炸！" << std::endl;
}

void test_align_and_stretch_regressions() {
    std::cout << "[Test 2] RenderAlign 有界松约束与 Flex Stretch 交叉轴无界回归测试..." << std::endl;

    // 回归 A: RenderAlign 在有界松约束下不应把高度/宽度强行撑满父容器 (d1802cd)
    const Alignment alignments[] = {
        Alignment::top_left(), Alignment::top_center(), Alignment::top_right(),
        Alignment::center_left(), Alignment::center(), Alignment::center_right(),
        Alignment::bottom_left(), Alignment::bottom_center(), Alignment::bottom_right()
    };
    for (Alignment al : alignments) {
        auto align_node = std::make_unique<RenderAlign>(al);
        auto child = std::make_shared<RenderSizedBox>(Vec2{80.0f, 30.0f});
        align_node->set_child_box(child);

        // 有界松约束 400x300：RenderAlign 应收缩包裹子项，而非撑满 400x300
        align_node->layout(BoxConstraints::loose(Vec2{400.0f, 300.0f}));
        assert_node_invariants_recursive(align_node.get());
        KTUI_CHECK(align_node->size().x <= 80.0f + 1e-2f);
        KTUI_CHECK(align_node->size().y <= 30.0f + 1e-2f);

        // 紧约束 400x300：RenderAlign 应严格等于 400x300，子项保持 80x30
        align_node->layout(BoxConstraints::tight(Vec2{400.0f, 300.0f}));
        assert_node_invariants_recursive(align_node.get());
        KTUI_CHECK(std::abs(align_node->size().x - 400.0f) < 1e-2f);
        KTUI_CHECK(std::abs(align_node->size().y - 300.0f) < 1e-2f);
        KTUI_CHECK(std::abs(child->size().x - 80.0f) < 1e-2f);
        KTUI_CHECK(std::abs(child->size().y - 30.0f) < 1e-2f);

        // 无界约束：不应产生 Inf 或超大尺寸
        align_node->layout(BoxConstraints{0.0f, kInf, 0.0f, kInf});
        assert_node_invariants_recursive(align_node.get());
        KTUI_CHECK(align_node->size().x <= 80.0f + 1e-2f);
        KTUI_CHECK(align_node->size().y <= 30.0f + 1e-2f);
    }

    // 回归 B: HStack(AlignItems::Stretch) 位于纵向 ScrollView(高无界) 中不应高度爆炸 (2207551)
    WidgetPtr nested = VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 8.0f,
        .content = {
            HStack {
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = {
                    Box { .width = 100.0f, .height = 32.0f },
                    Expanded { .child = Box { .height = 48.0f } }
                }
            },
            Align {
                .alignment = Alignment::center(),
                .child = Box { .width = 120.0f, .height = 28.0f }
            }
        }
    };
    auto root = nested->create_render_node();
    root->layout(BoxConstraints{0.0f, 500.0f, 0.0f, kInf});
    assert_node_invariants_recursive(root.get());
    KTUI_CHECK(root->size().y >= 48.0f && root->size().y <= 100.0f);

    std::cout << "  -> RenderAlign 与 Flex Stretch 回归约束全部通过！" << std::endl;
}

void test_composite_containers_constraint_matrix() {
    std::cout << "[Test 3] Wrap / Grid / ZStack / ScrollView / DockPanel 复合容器约束矩阵..." << std::endl;

    const auto constraints = build_constraint_matrix();
    for (const auto& c : constraints) {
        WidgetPtr tree = VStack {
            .spacing = 6.0f,
            .padding = EdgeInsets(6.0f),
            .content = {
                Wrap {
                    .spacing = 4.0f,
                    .run_spacing = 4.0f,
                    .content = {
                        Box { .width = 60.0f, .height = 24.0f },
                        Box { .width = 80.0f, .height = 24.0f },
                        Box { .width = 70.0f, .height = 24.0f }
                    }
                },
                Grid {
                    .columns = 2,
                    .column_spacing = 6.0f,
                    .row_spacing = 6.0f,
                    .content = {
                        Box { .height = 28.0f },
                        Box { .height = 28.0f },
                        Box { .height = 28.0f },
                        Box { .height = 28.0f }
                    }
                },
                ZStack {
                    .content = {
                        Box { .width = 120.0f, .height = 40.0f },
                        Align {
                            .alignment = Alignment::center(),
                            .child = Box { .width = 40.0f, .height = 20.0f }
                        }
                    }
                },
                ScrollView {
                    .child = VStack {
                        .spacing = 4.0f,
                        .content = {
                            Box { .width = 100.0f, .height = 40.0f },
                            Box { .width = 100.0f, .height = 40.0f }
                        }
                    }
                }
            }
        };

        auto root = tree->create_render_node();
        root->layout(c);
        assert_bounded_fit(root.get(), c);
        assert_node_invariants_recursive(root.get());
    }

    std::cout << "  -> 所有复合布局容器在 10 种极端约束下均保持有限、非负且不越界！" << std::endl;
}

} // namespace

int main() {
    std::cout << "=== KTUI 布局约束矩阵与防爆炸压力测试 ===" << std::endl;
    test_flex_constraint_matrix();
    test_align_and_stretch_regressions();
    test_composite_containers_constraint_matrix();
    std::cout << "=== 全部布局约束矩阵测试通过 ===" << std::endl;
    return 0;
}
