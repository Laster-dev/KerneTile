#include <ktui/layout/box_constraints.hpp>
#include <ktui/layout/flex_engine.hpp>
#include <ktui/tree/render_flex.hpp>
#include <ktui/tree/reconciliation.hpp>
#include <ktui/widgets/wrap.hpp>
#include <ktui/widgets/grid.hpp>
#include <ktui/widgets/zstack.hpp>
#include <ktui/widgets/dock_panel.hpp>
#include <ktui/widgets/layout_primitives.hpp>
#include <iostream>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <cmath>

using namespace ktui;

void test_box_constraints_basics() {
    std::cout << "[Test 1] 盒约束 BoxConstraints 基本功能与安全钳制测试..." << std::endl;

    BoxConstraints loose_c = BoxConstraints::loose(Vec2{800.0f, 600.0f});
    assert(loose_c.min_width == 0.0f && loose_c.min_height == 0.0f);
    assert(loose_c.max_width == 800.0f && loose_c.max_height == 600.0f);
    assert(loose_c.is_loose());
    assert(!loose_c.is_tight());

    BoxConstraints tight_c = BoxConstraints::tight(Vec2{300.0f, 200.0f});
    assert(tight_c.is_tight());
    assert(tight_c.constrain(Vec2{50.0f, 500.0f}) == Vec2(300.0f, 200.0f));

    // 内边距扣除
    EdgeInsets padding(10.0f, 20.0f, 10.0f, 20.0f); // 水平 40, 垂直 20
    BoxConstraints deflated = tight_c.deflate(padding);
    assert(deflated.min_width == 260.0f && deflated.max_width == 260.0f);
    assert(deflated.min_height == 180.0f && deflated.max_height == 180.0f);

    std::cout << "  -> 紧约束/松约束与内边距扣除计算完全正确！" << std::endl;
}

void test_flex_main_grow_allocation() {
    std::cout << "[Test 2] Flex 主轴固定尺寸与 flex_grow 剩余空间分配精度测试..." << std::endl;

    // 容器宽 300px，高 100px (紧约束)
    RenderFlex container(FlexDirection::Row, JustifyContent::FlexStart, AlignItems::FlexStart, 20.0f /* gap */);

    // 子项 1: 固定宽 100px, 高 40px
    auto child1 = std::make_unique<RenderSizedBox>(Vec2{100.0f, 40.0f});

    // 子项 2: flex_grow = 1.0f 弹性填满剩余空间
    auto child2 = std::make_unique<RenderSizedBox>(Vec2{0.0f, 50.0f});
    child2->flex_data().flex_grow = 1.0f;
    child2->flex_data().fit = FlexFit::Tight;

    container.append_child(child1.get());
    container.append_child(child2.get());

    container.layout(BoxConstraints::tight(Vec2{300.0f, 100.0f}));

    // 验证尺寸: 子项 2 应当获得 300 - 100(固定项) - 20(gap) = 180px
    assert(child1->size().x == 100.0f);
    assert(child2->size().x == 180.0f);

    // 验证坐标定位:
    // child1 位于 (0, 0)
    // child2 位于 (100 + 20 = 120, 0)
    assert(child1->offset() == Vec2(0.0f, 0.0f));
    assert(child2->offset() == Vec2(120.0f, 0.0f));

    std::cout << "  -> flex_grow 弹性主轴空间计算准确: 300 - 100 - 20 = 180px!" << std::endl;
}

void test_flex_multiple_grow_ratios() {
    std::cout << "[Test 3] 多弹性子项按比例伸缩 (flex_grow = 1 vs 2) 测试..." << std::endl;

    // 容器宽 600px，gap = 15px
    RenderFlex container(FlexDirection::Row, JustifyContent::FlexStart, AlignItems::FlexStart, 15.0f);

    auto childA = std::make_unique<RenderSizedBox>(Vec2{150.0f, 40.0f}); // 固定 150
    auto childB = std::make_unique<RenderSizedBox>(Vec2{0.0f, 40.0f});   // flex 1
    childB->flex_data().flex_grow = 1.0f;
    auto childC = std::make_unique<RenderSizedBox>(Vec2{0.0f, 40.0f});   // flex 2
    childC->flex_data().flex_grow = 2.0f;

    container.append_child(childA.get());
    container.append_child(childB.get());
    container.append_child(childC.get());

    container.layout(BoxConstraints::tight(Vec2{600.0f, 100.0f}));

    // 剩余空间: 600 - 150 - (2 * 15) = 420px
    // childB: 420 * (1 / 3) = 140px
    // childC: 420 * (2 / 3) = 280px
    assert(std::abs(childA->size().x - 150.0f) < 1e-3f);
    assert(std::abs(childB->size().x - 140.0f) < 1e-3f);
    assert(std::abs(childC->size().x - 280.0f) < 1e-3f);

    // 坐标校验:
    // A: 0
    // B: 150 + 15 = 165
    // C: 165 + 140 + 15 = 320
    assert(std::abs(childA->offset().x - 0.0f) < 1e-3f);
    assert(std::abs(childB->offset().x - 165.0f) < 1e-3f);
    assert(std::abs(childC->offset().x - 320.0f) < 1e-3f);

    std::cout << "  -> 1:2 比例弹性分配计算与累加原点严格吻合！" << std::endl;
}

void test_justify_content_distribution() {
    std::cout << "[Test 4] JustifyContent::SpaceBetween / SpaceEvenly / Center 测试..." << std::endl;

    // 容器宽 300px，3 个 50px 宽子项 (总使用 150px，剩余 150px，gap=0)
    auto c1 = std::make_unique<RenderSizedBox>(Vec2{50.0f, 50.0f});
    auto c2 = std::make_unique<RenderSizedBox>(Vec2{50.0f, 50.0f});
    auto c3 = std::make_unique<RenderSizedBox>(Vec2{50.0f, 50.0f});

    RenderFlex container(FlexDirection::Row, JustifyContent::SpaceBetween, AlignItems::FlexStart, 0.0f);
    container.append_child(c1.get());
    container.append_child(c2.get());
    container.append_child(c3.get());

    // 1. SpaceBetween 测试
    container.layout(BoxConstraints::tight(Vec2{300.0f, 60.0f}));
    // 间隙 = 150 / 2 = 75px
    // c1 = 0, c2 = 50 + 75 = 125, c3 = 125 + 50 + 75 = 250
    assert(std::abs(c1->offset().x - 0.0f) < 1e-3f);
    assert(std::abs(c2->offset().x - 125.0f) < 1e-3f);
    assert(std::abs(c3->offset().x - 250.0f) < 1e-3f);

    // 2. SpaceEvenly 测试
    container.set_justify(JustifyContent::SpaceEvenly);
    container.layout(BoxConstraints::tight(Vec2{300.0f, 60.0f}));
    // 4 个等距间隙 = 150 / 4 = 37.5px
    // c1 = 37.5, c2 = 37.5 + 50 + 37.5 = 125, c3 = 125 + 50 + 37.5 = 212.5
    assert(std::abs(c1->offset().x - 37.5f) < 1e-3f);
    assert(std::abs(c2->offset().x - 125.0f) < 1e-3f);
    assert(std::abs(c3->offset().x - 212.5f) < 1e-3f);

    // 3. Center 测试
    container.set_justify(JustifyContent::Center);
    container.layout(BoxConstraints::tight(Vec2{300.0f, 60.0f}));
    // 左右均分剩余 150px -> 起点 75px
    // c1 = 75, c2 = 125, c3 = 175
    assert(std::abs(c1->offset().x - 75.0f) < 1e-3f);
    assert(std::abs(c2->offset().x - 125.0f) < 1e-3f);
    assert(std::abs(c3->offset().x - 175.0f) < 1e-3f);

    std::cout << "  -> JustifyContent 主轴等距/两端/居中排版公式验证 100% 精确！" << std::endl;
}

void test_column_and_cross_axis_alignment() {
    std::cout << "[Test 5] 垂直 Column 与 AlignItems::Center / Stretch 交叉轴对齐测试..." << std::endl;

    // 容器为 Column (垂直)，宽 200px, 高 400px (紧约束)
    RenderFlex col(FlexDirection::Column, JustifyContent::FlexStart, AlignItems::FlexStart, 10.0f);

    // item1: 50x50, 显式设置为交叉轴居中 (AlignItems::Center)
    auto item1 = std::make_unique<RenderSizedBox>(Vec2{50.0f, 50.0f});
    item1->flex_data().align_self = AlignItems::Center;

    // item2: 高度 60px, 显式设置为拉伸 (AlignItems::Stretch)
    auto item2 = std::make_unique<RenderSizedBox>(Vec2{50.0f, 60.0f});
    item2->flex_data().align_self = AlignItems::Stretch;

    col.append_child(item1.get());
    col.append_child(item2.get());

    col.layout(BoxConstraints::tight(Vec2{200.0f, 400.0f}));

    // 检查 item1 交叉轴 (X 轴) 居中: (200 - 50) / 2 = 75px, Y 轴为 0
    assert(std::abs(item1->offset().x - 75.0f) < 1e-3f);
    assert(std::abs(item1->offset().y - 0.0f) < 1e-3f);

    // 检查 item2 交叉轴拉伸 (Stretch): 宽度应拉伸为容器宽度 200px, X 轴为 0, Y 轴为 50 + 10 = 60px
    assert(std::abs(item2->size().x - 200.0f) < 1e-3f);
    assert(std::abs(item2->size().y - 60.0f) < 1e-3f);
    assert(std::abs(item2->offset().x - 0.0f) < 1e-3f);
    assert(std::abs(item2->offset().y - 60.0f) < 1e-3f);

    std::cout << "  -> 垂直 Column 排布与交叉轴 Center/Stretch 拉伸计算完全吻合！" << std::endl;
}

void test_relayout_boundary_isolation() {
    std::cout << "[Test 6] Relayout Boundary (重排边界) 冒泡阻断机制测试..." << std::endl;

    // 构建层级树: RootFlex -> MiddleBoundaryBox (紧约束，固定宽高) -> LeafBox
    auto root = std::make_unique<RenderFlex>(FlexDirection::Column);
    auto middle = std::make_unique<RenderSizedBox>(Vec2{200.0f, 200.0f});
    middle->set_relayout_boundary(true); // 声明为重排隔离边界
    auto leaf = std::make_unique<RenderSizedBox>(Vec2{50.0f, 50.0f});

    middle->append_child(leaf.get());
    root->append_child(middle.get());

    // 初次布局: middle 受到紧约束 tight(200, 200)，推导为 Relayout Boundary
    root->layout(BoxConstraints::loose(Vec2{800.0f, 600.0f}));

    assert(!root->needs_layout());
    assert(!middle->needs_layout());
    assert(!leaf->needs_layout());
    assert(middle->is_relayout_boundary()); // 成功建立边界！

    // 触发底层叶子节点重新排版
    leaf->set_fixed_size(Vec2{80.0f, 80.0f});

    // 校验状态:
    // leaf 变脏
    assert(leaf->needs_layout());
    // middle 变脏
    assert(middle->needs_layout());
    // 关键契约: root 的 needs_layout 必须依然为 false！重排必须被 middle 边界完全阻断！
    assert(!root->needs_layout());
    assert(root->child_needs_layout());

    // 从根节点再次触发 layout 时，必须穿透到达脏边界并完成 leaf 的重排
    root->layout(BoxConstraints::loose(Vec2{800.0f, 600.0f}));
    assert(!root->needs_layout());
    assert(!root->child_needs_layout());
    assert(!middle->needs_layout());
    assert(!leaf->needs_layout());
    assert(leaf->size() == Vec2(80.0f, 80.0f));

    std::cout << "  -> 核心 Relayout Boundary 成功阻断冒泡，根节点未被污染 (Zero Cascade)!" << std::endl;
}

void test_reconciler_children_sync() {
    std::cout << "[Test 7] Reconciler 树增量调和与节点原地复用测试..." << std::endl;

    auto container = std::make_unique<RenderFlex>();

    struct Spec {
        int id;
        Vec2 size;
    };

    std::vector<Spec> v1 = {
        {1, Vec2{100.0f, 50.0f}},
        {2, Vec2{120.0f, 50.0f}},
        {3, Vec2{140.0f, 50.0f}}
    };

    // 第一次调和：创建 3 个子节点
    Reconciler::sync_children(
        container.get(),
        v1,
        [](const Spec& s) -> RenderNode* { return new RenderSizedBox(s.size); },
        [](RenderNode* n, const Spec& s) { static_cast<RenderSizedBox*>(n)->set_fixed_size(s.size); }
    );

    assert(container->child_count() == 3);
    RenderNode* node0 = container->first_child();
    RenderNode* node1 = node0->next_sibling();
    RenderNode* node2 = node1->next_sibling();
    assert(node2 != nullptr);

    // 第二次调和：减少为 2 个规格并修改尺寸
    std::vector<Spec> v2 = {
        {1, Vec2{110.0f, 60.0f}},
        {2, Vec2{130.0f, 60.0f}}
    };

    Reconciler::sync_children(
        container.get(),
        v2,
        [](const Spec& s) -> RenderNode* { return new RenderSizedBox(s.size); },
        [](RenderNode* n, const Spec& s) { static_cast<RenderSizedBox*>(n)->set_fixed_size(s.size); }
    );

    assert(container->child_count() == 2);
    // 验证前两个节点指针原地复用，未被无谓销毁重建
    assert(container->first_child() == node0);
    assert(container->first_child()->next_sibling() == node1);
    assert(static_cast<RenderSizedBox*>(node0)->fixed_size() == Vec2(110.0f, 60.0f));

    std::cout << "  -> Reconciler 原地复用更新与多余节点回收验证成功！" << std::endl;
}

#define KT_EXPECT(cond) do { if (!(cond)) { std::cerr << "FAIL: " #cond " at line " << __LINE__ << std::endl; std::exit(1); } } while (0)

void test_flex_shrink_and_unbounded_stretch() {
    std::cout << "[Test 8] FlexEngine flex_shrink 溢出压缩与无界交叉轴 Stretch 测试..." << std::endl;

    // 1. flex_shrink: 容器宽 300，放入 100(shrink=0) + 200(shrink=1) + 200(shrink=2)
    // overflow = 200; weights = 1*200 + 2*200 = 600 -> child1 缩 200*(200/600)=66.67, child2 缩 200*(400/600)=133.33
    auto row = std::make_unique<RenderFlex>(FlexDirection::Row, JustifyContent::FlexStart, AlignItems::FlexStart, 0.0f);
    auto c0 = std::make_unique<RenderSizedBox>(Vec2{100.0f, 30.0f});
    c0->set_flex_data(FlexItemData{0.0f, 0.0f, FlexFit::Tight, std::nullopt});
    auto c1 = std::make_unique<RenderSizedBox>(Vec2{200.0f, 30.0f});
    c1->set_flex_data(FlexItemData{0.0f, 1.0f, FlexFit::Tight, std::nullopt});
    auto c2 = std::make_unique<RenderSizedBox>(Vec2{200.0f, 30.0f});
    c2->set_flex_data(FlexItemData{0.0f, 2.0f, FlexFit::Tight, std::nullopt});
    row->append_child(c0.get());
    row->append_child(c1.get());
    row->append_child(c2.get());
    row->layout(BoxConstraints(0.0f, 300.0f, 0.0f, 100.0f));

    KT_EXPECT(std::abs(c0->size().x - 100.0f) < 0.5f);
    KT_EXPECT(std::abs(c1->size().x - 133.33f) < 1.0f);
    KT_EXPECT(std::abs(c2->size().x - 66.67f) < 1.0f);
    KT_EXPECT(std::abs((c0->size().x + c1->size().x + c2->size().x) - 300.0f) < 1.0f);

    // 2. 无界交叉轴 AlignItems::Stretch: 矮子项应拉伸至同行最高子项高度 80
    auto stretch_row = std::make_unique<RenderFlex>(FlexDirection::Row, JustifyContent::FlexStart, AlignItems::Stretch, 0.0f);
    auto short_box = std::make_unique<RenderSizedBox>(Vec2{60.0f, 20.0f});
    auto tall_box  = std::make_unique<RenderSizedBox>(Vec2{60.0f, 80.0f});
    stretch_row->append_child(short_box.get());
    stretch_row->append_child(tall_box.get());
    stretch_row->layout(BoxConstraints(0.0f, 300.0f, 0.0f, BoxConstraints::kInfinity));

    KT_EXPECT(std::abs(stretch_row->size().y - 80.0f) < 0.5f);
    KT_EXPECT(std::abs(short_box->size().y - 80.0f) < 0.5f);

    // 3. 有限但非紧约束交叉轴 AlignItems::Stretch (如 VStack 内的 HStack，min_h=0, max_h=600)
    // 绝不能被强行拉满到 600px，而应先按子项固有高度 32px 度量，再二次拉伸矮子项至 32px!
    auto bounded_loose_row = std::make_unique<RenderFlex>(FlexDirection::Row, JustifyContent::FlexStart, AlignItems::Stretch, 0.0f);
    auto b1 = std::make_unique<RenderSizedBox>(Vec2{50.0f, 32.0f});
    b1->set_flex_data(FlexItemData{1.0f, 1.0f, FlexFit::Tight, std::nullopt});
    auto b2 = std::make_unique<RenderSizedBox>(Vec2{50.0f, 20.0f});
    b2->set_flex_data(FlexItemData{1.0f, 1.0f, FlexFit::Tight, std::nullopt});
    bounded_loose_row->append_child(b1.get());
    bounded_loose_row->append_child(b2.get());
    bounded_loose_row->layout(BoxConstraints(400.0f, 400.0f, 0.0f, 600.0f));

    KT_EXPECT(std::abs(bounded_loose_row->size().x - 400.0f) < 0.5f);
    KT_EXPECT(std::abs(bounded_loose_row->size().y - 32.0f) < 0.5f);
    KT_EXPECT(std::abs(b1->size().y - 32.0f) < 0.5f);
    KT_EXPECT(std::abs(b2->size().y - 32.0f) < 0.5f);

    std::cout << "  -> flex_shrink 压缩与无界交叉轴 Stretch 验证通过！" << std::endl;
}

void test_wrap_layout() {
    std::cout << "[Test 9] Wrap 流式自动换行布局测试..." << std::endl;

    auto wrap = std::make_unique<RenderWrap>(FlexDirection::Row, 10.0f, 12.0f, JustifyContent::FlexStart, AlignItems::Center);
    auto a = make_render_node<RenderSizedBox>(Vec2{100.0f, 30.0f});
    auto b = make_render_node<RenderSizedBox>(Vec2{100.0f, 40.0f});
    auto c = make_render_node<RenderSizedBox>(Vec2{100.0f, 30.0f});
    wrap->set_child_boxes({a, b, c});
    // 容器最大宽 250 -> a(100) + 10 + b(100) = 210 <= 250 (第 1 行高 40); c 折到第 2 行 (y = 40 + 12 = 52)
    wrap->layout(BoxConstraints(0.0f, 250.0f, 0.0f, BoxConstraints::kInfinity));

    KT_EXPECT(std::abs(a->offset().x - 0.0f) < 0.5f);
    KT_EXPECT(std::abs(a->offset().y - 5.0f) < 0.5f); // 在高 40 的行内垂直居中 -> (40-30)/2 = 5
    KT_EXPECT(std::abs(b->offset().x - 110.0f) < 0.5f);
    KT_EXPECT(std::abs(c->offset().x - 0.0f) < 0.5f);
    KT_EXPECT(std::abs(c->offset().y - 52.0f) < 0.5f);
    KT_EXPECT(std::abs(wrap->size().y - 82.0f) < 0.5f);

    std::cout << "  -> Wrap 自动折行、行内居中与总高度计算验证通过！" << std::endl;
}

void test_grid_layout() {
    std::cout << "[Test 10] Grid 二维网格 (固定列 / 权重轨道 / 响应式 / 跨列) 测试..." << std::endl;

    // 1. 权重轨道 {1, 2, 1} + 跨列 (span=2)
    auto grid = std::make_unique<RenderGrid>(3, 0.0f, std::vector<float>{1.0f, 2.0f, 1.0f}, 10.0f, 8.0f, 40.0f);
    auto span_item = make_render_node<RenderGridItem>(2);
    span_item->set_child_box(make_render_node<RenderSizedBox>(Vec2{10.0f, 10.0f}));
    auto cell1 = make_render_node<RenderSizedBox>(Vec2{10.0f, 10.0f});
    auto cell2 = make_render_node<RenderSizedBox>(Vec2{10.0f, 10.0f});
    grid->set_child_boxes({span_item, cell1, cell2});
    // avail_w = 420, gaps = 20 -> track_avail = 400 -> tracks = 100, 200, 100
    // span_item (col 0..1) = 100 + 10 + 200 = 310; cell1 (col 2) = 100 at x=320; cell2 wraps to row 2 (y=48)
    grid->layout(BoxConstraints(420.0f, 420.0f, 0.0f, BoxConstraints::kInfinity));

    KT_EXPECT(std::abs(span_item->size().x - 310.0f) < 0.5f);
    KT_EXPECT(std::abs(cell1->offset().x - 320.0f) < 0.5f);
    KT_EXPECT(std::abs(cell1->size().x - 100.0f) < 0.5f);
    KT_EXPECT(std::abs(cell2->offset().y - 48.0f) < 0.5f);
    KT_EXPECT(std::abs(grid->size().y - 88.0f) < 0.5f);

    // 2. 响应式 min_item_width = 150 -> 在 500px 宽容器内应解析为 3 列 (150*3 + 10*2 = 470 <= 500)
    auto resp = std::make_unique<RenderGrid>(1, 150.0f, std::vector<float>{}, 10.0f, 10.0f, 40.0f);
    resp->set_child_boxes({make_render_node<RenderSizedBox>(Vec2{10.0f, 10.0f})});
    resp->layout(BoxConstraints(500.0f, 500.0f, 0.0f, BoxConstraints::kInfinity));
    KT_EXPECT(resp->resolved_columns() == 3);

    std::cout << "  -> Grid 权重轨道、跨列合并与响应式自适应列数验证通过！" << std::endl;
}

void test_zstack_alignment_and_aspect_ratio() {
    std::cout << "[Test 11] ZStack 九宫格对齐与 AspectRatio 固定宽高比测试..." << std::endl;

    auto z = std::make_unique<RenderZStack>(Alignment::bottom_right(), StackFit::Loose, EdgeInsets(10.0f, 10.0f));
    auto bg = make_render_node<RenderSizedBox>(Vec2{100.0f, 80.0f});
    auto badge = make_render_node<RenderSizedBox>(Vec2{20.0f, 20.0f});
    z->set_child_boxes({bg, badge});
    z->layout(BoxConstraints::tight(Vec2{200.0f, 120.0f}));
    // inner = 180x100 -> bottom_right offset for 20x20 = (160, 80) + padding (10, 10) = (170, 90)
    KT_EXPECT(std::abs(badge->offset().x - 170.0f) < 0.5f);
    KT_EXPECT(std::abs(badge->offset().y - 90.0f) < 0.5f);

    auto ar = std::make_unique<RenderAspectRatio>(16.0f / 9.0f);
    ar->set_child_box(make_render_node<RenderSizedBox>(Vec2{10.0f, 10.0f}));
    ar->layout(BoxConstraints(0.0f, 320.0f, 0.0f, 500.0f));
    KT_EXPECT(std::abs(ar->size().x - 320.0f) < 0.5f);
    KT_EXPECT(std::abs(ar->size().y - 180.0f) < 0.5f);

    // 4. RenderAlign 在有界但非紧约束垂直空间 (如 VStack 内 Stretch 宽、松散有界高 min_h=0, max_h=600)
    // 不应撑满 600px 高度，而应保持宽 400px 以便 right_align、高收缩至子项 40px；
    // 仅当纵轴同样为紧约束 (min_h == max_h == 200) 时才撑满 200px。
    auto align_box = std::make_unique<RenderAlign>(Alignment::center_right());
    align_box->set_child_box(make_render_node<RenderSizedBox>(Vec2{80.0f, 40.0f}));
    align_box->layout(BoxConstraints(400.0f, 400.0f, 0.0f, 600.0f));
    KT_EXPECT(std::abs(align_box->size().x - 400.0f) < 0.5f);
    KT_EXPECT(std::abs(align_box->size().y - 40.0f) < 0.5f);
    KT_EXPECT(std::abs(align_box->first_child()->as_render_box()->offset().x - 320.0f) < 0.5f);

    align_box->layout(BoxConstraints::tight(Vec2{400.0f, 200.0f}));
    KT_EXPECT(std::abs(align_box->size().y - 200.0f) < 0.5f);
    KT_EXPECT(std::abs(align_box->first_child()->as_render_box()->offset().y - 80.0f) < 0.5f);

    std::cout << "  -> ZStack 九宫格对齐与 AspectRatio 16:9 约束验证通过！" << std::endl;
}

void test_positioned_dock_and_visibility() {
    std::cout << "[Test 12] Positioned 边角锚定、DockPanel 五区停靠与 Visibility 显隐测试..." << std::endl;

    // 1. ZStack + Positioned 在松有界约束 (min=0, max=200x120) 下也必须自动撑满可用区域
    auto z = std::make_unique<RenderZStack>(Alignment::center(), StackFit::Loose, EdgeInsets::zero());
    auto bg = make_render_node<RenderSizedBox>(Vec2{80.0f, 18.0f}); // 模拟单行文本，不撑满容器
    auto tr = make_render_node<RenderPositioned>(std::nullopt, 8.0f, 12.0f, std::nullopt, 30.0f, 20.0f);
    tr->set_child_box(make_render_node<RenderSizedBox>(Vec2{30.0f, 20.0f}));
    auto bar = make_render_node<RenderPositioned>(10.0f, std::nullopt, 10.0f, 6.0f, std::nullopt, 24.0f);
    bar->set_child_box(make_render_node<RenderSizedBox>(Vec2{10.0f, 10.0f}));
    z->set_child_boxes({bg, tr, bar});
    z->layout(BoxConstraints::loose(Vec2{200.0f, 120.0f}));
    KT_EXPECT(std::abs(z->size().x - 200.0f) < 0.5f && std::abs(z->size().y - 120.0f) < 0.5f);
    KT_EXPECT(std::abs(tr->offset().x - (200.0f - 12.0f - 30.0f)) < 0.5f);
    KT_EXPECT(std::abs(tr->offset().y - 8.0f) < 0.5f);
    KT_EXPECT(std::abs(bar->size().x - 180.0f) < 0.5f);
    KT_EXPECT(std::abs(bar->offset().y - (120.0f - 6.0f - 24.0f)) < 0.5f);

    // 2. Visual Studio 风格 DockPanel 动态停靠、独立窗口反复撕出/回停靠与重置压测
    auto dock = std::make_unique<RenderDockPanel>(160.0f, 160.0f, 100.0f, 110.0f);
    std::vector<RenderDockPanel::LivePane> panes;
    panes.push_back({"p_left",   "Left",   DockZone::Left,   DockZone::Left,   false, false, false, make_render_node<RenderSizedBox>(Vec2{10, 10}), {}, {}});
    panes.push_back({"p_center", "Center", DockZone::Center, DockZone::Center, false, false, false, make_render_node<RenderSizedBox>(Vec2{10, 10}), {}, {}});
    panes.push_back({"p_bottom", "Bottom", DockZone::Bottom, DockZone::Bottom, false, false, false, make_render_node<RenderSizedBox>(Vec2{10, 10}), {}, {}});
    dock->set_panes(std::move(panes));
    dock->layout(BoxConstraints::tight(Vec2{600.0f, 400.0f}));
    KT_EXPECT(!dock->zone_rect(DockZone::Left).is_empty());
    KT_EXPECT(!dock->zone_rect(DockZone::Center).is_empty());
    KT_EXPECT(dock->zone_rect(DockZone::Right).is_empty());

    // 将 p_left 重新停靠至 Right -> Left 应清空，Right 应非空
    KT_EXPECT(dock->dock_pane("p_left", DockZone::Right));
    dock->layout(BoxConstraints::tight(Vec2{600.0f, 400.0f}));
    KT_EXPECT(dock->pane_zone("p_left") == DockZone::Right);
    KT_EXPECT(dock->zone_rect(DockZone::Left).is_empty());
    KT_EXPECT(!dock->zone_rect(DockZone::Right).is_empty());

    // 反复撕出/回停靠 5 轮压测，验证 RenderBox 父子关系与状态零损坏
    Fn<void()> saved_redock;
    dock->set_on_detach_pane([&](const std::string&, const std::string&, std::shared_ptr<RenderBox> box, Fn<void()> redock) -> bool {
        KT_EXPECT(box != nullptr && box->parent() == nullptr);
        saved_redock = std::move(redock);
        return true;
    });
    for (int round = 0; round < 5; ++round) {
        KT_EXPECT(dock->detach_pane("p_bottom"));
        KT_EXPECT(dock->is_pane_detached("p_bottom"));
        dock->layout(BoxConstraints::tight(Vec2{600.0f, 400.0f}));
        KT_EXPECT(dock->zone_rect(DockZone::Bottom).is_empty());
        saved_redock();
        KT_EXPECT(!dock->is_pane_detached("p_bottom"));
        KT_EXPECT(dock->pane_zone("p_bottom") == DockZone::Bottom);
        dock->layout(BoxConstraints::tight(Vec2{600.0f, 400.0f}));
        KT_EXPECT(!dock->zone_rect(DockZone::Bottom).is_empty());
    }

    // 一键恢复初始停靠布局
    dock->reset_layout();
    dock->layout(BoxConstraints::tight(Vec2{600.0f, 400.0f}));
    KT_EXPECT(dock->pane_zone("p_left") == DockZone::Left);

    // 3. Visibility: Collapse (0x0) vs Hidden (maintain_size=true -> 100x40)
    auto vis_col = std::make_unique<RenderVisibility>(false, false);
    vis_col->set_child_box(make_render_node<RenderSizedBox>(Vec2{100.0f, 40.0f}));
    vis_col->layout(BoxConstraints::loose(Vec2{300.0f, 300.0f}));
    KT_EXPECT(vis_col->size().x == 0.0f && vis_col->size().y == 0.0f);

    auto vis_hid = std::make_unique<RenderVisibility>(false, true);
    vis_hid->set_child_box(make_render_node<RenderSizedBox>(Vec2{100.0f, 40.0f}));
    vis_hid->layout(BoxConstraints::loose(Vec2{300.0f, 300.0f}));
    KT_EXPECT(std::abs(vis_hid->size().x - 100.0f) < 0.5f && std::abs(vis_hid->size().y - 40.0f) < 0.5f);

    std::cout << "  -> Positioned、DockPanel 与 Visibility (Collapse/Hidden) 验证通过！" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "   KTUI 阶段 5: RenderTree 与全布局系统测试       " << std::endl;
    std::cout << "==================================================" << std::endl;

    test_box_constraints_basics();
    test_flex_main_grow_allocation();
    test_flex_multiple_grow_ratios();
    test_justify_content_distribution();
    test_column_and_cross_axis_alignment();
    test_relayout_boundary_isolation();
    test_reconciler_children_sync();
    test_flex_shrink_and_unbounded_stretch();
    test_wrap_layout();
    test_grid_layout();
    test_zstack_alignment_and_aspect_ratio();
    test_positioned_dock_and_visibility();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 5 RenderTree 与全布局系统 100% 通过! " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
