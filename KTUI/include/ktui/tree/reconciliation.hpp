#pragma once

#include "render_box.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <string>

namespace ktui {

// 节点标识 Key (用于列表增量调和优化)
struct WidgetKey {
    std::string value;

    bool operator==(const WidgetKey&) const noexcept = default;
};

// 增量调和器 (Reconciler - 借鉴 React Fiber / Flutter Element 调和算法精髓)
// 保持 RenderNode 树的结构稳定，仅在类型改变时重建，在属性变化时直接原地更新，避免无谓的内存重新分配
class Reconciler {
public:
    // 同步子节点列表：通过比较旧 RenderNode 树与新子项规格，完成原地更新、创建或删除
    template <typename ItemSpec, typename FactoryFn, typename UpdaterFn>
    static void sync_children(RenderNode* parent,
                              const std::vector<ItemSpec>& new_specs,
                              FactoryFn&& create_fn,
                              UpdaterFn&& update_fn) {
        if (!parent) return;

        RenderNode* current_child = parent->first_child();
        size_t new_count = new_specs.size();

        for (size_t i = 0; i < new_count; ++i) {
            const auto& spec = new_specs[i];

            if (current_child != nullptr) {
                // 复用并就地更新
                RenderNode* next = current_child->next_sibling();
                update_fn(current_child, spec);
                current_child = next;
            } else {
                // 新增子节点并挂接
                RenderNode* new_node = create_fn(spec);
                if (new_node) {
                    parent->append_child(new_node);
                }
            }
        }

        // 移除多余的旧节点
        while (current_child != nullptr) {
            RenderNode* to_remove = current_child;
            current_child = current_child->next_sibling();
            parent->remove_child(to_remove);
            delete to_remove;
        }
    }
};

} // namespace ktui
