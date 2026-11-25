#pragma once

#include "render_box.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>

namespace ktui {

// 节点标识 Key (用于列表增量调和优化)
struct WidgetKey {
    std::string value;

    bool operator==(const WidgetKey&) const noexcept = default;
};

// 增量调和器 (Reconciler - 支持按位置与按 Key 稳定复用，兼容原始指针与 shared_ptr 所有权)
class Reconciler {
public:
    struct DefaultRawDeleter {
        void operator()(RenderNode* node) const noexcept { delete node; }
    };

    // 按位置索引同步子节点列表 (可选自定义 destroy_fn，托管所有权时可传入空操作避免 Double Free)
    template <typename ItemSpec, typename FactoryFn, typename UpdaterFn, typename DestroyFn = DefaultRawDeleter>
    static void sync_children(RenderNode* parent,
                              const std::vector<ItemSpec>& new_specs,
                              FactoryFn&& create_fn,
                              UpdaterFn&& update_fn,
                              DestroyFn&& destroy_fn = DestroyFn{}) {
        if (!parent) return;

        RenderNode* current_child = parent->first_child();
        size_t new_count = new_specs.size();

        for (size_t i = 0; i < new_count; ++i) {
            const auto& spec = new_specs[i];

            if (current_child != nullptr) {
                RenderNode* next = current_child->next_sibling();
                update_fn(current_child, spec);
                current_child = next;
            } else {
                RenderNode* new_node = create_fn(spec);
                if (new_node) {
                    parent->append_child(new_node);
                }
            }
        }

        while (current_child != nullptr) {
            RenderNode* to_remove = current_child;
            current_child = current_child->next_sibling();
            parent->remove_child(to_remove);
            destroy_fn(to_remove);
        }
    }

    // 基于 WidgetKey 的稳定增量调和 (支持中间插入、删除、重排时保留节点状态，且安全管理 shared_ptr 所有权)
    template <typename ItemSpec, typename KeyFn, typename FactoryFn, typename UpdaterFn>
    static void sync_owned_children_keyed(RenderNode* parent,
                                          std::vector<std::shared_ptr<RenderBox>>& owned_storage,
                                          std::vector<std::string>& owned_keys,
                                          const std::vector<ItemSpec>& new_specs,
                                          KeyFn&& key_fn,
                                          FactoryFn&& create_fn,
                                          UpdaterFn&& update_fn) {
        if (!parent) return;

        std::vector<std::pair<std::string, std::shared_ptr<RenderBox>>> old_map;
        old_map.reserve(owned_storage.size());
        const size_t old_count = std::min(owned_storage.size(), owned_keys.size());
        for (size_t i = 0; i < old_count; ++i) {
            if (owned_storage[i]) {
                old_map.emplace_back(std::move(owned_keys[i]), std::move(owned_storage[i]));
            }
        }

        // 先从父节点摘除全部旧子节点以便按新顺序精确挂接 (不触发析构)
        while (RenderNode* c = parent->first_child()) {
            parent->remove_child(c);
        }

        std::vector<std::shared_ptr<RenderBox>> next_storage;
        std::vector<std::string> next_keys;
        next_storage.reserve(new_specs.size());
        next_keys.reserve(new_specs.size());

        for (const auto& spec : new_specs) {
            std::string key = key_fn(spec);
            std::shared_ptr<RenderBox> node;
            for (size_t i = 0; i < old_map.size(); ++i) {
                if (old_map[i].first == key) {
                    node = std::move(old_map[i].second);
                    old_map[i] = std::move(old_map.back());
                    old_map.pop_back();
                    update_fn(node.get(), spec);
                    break;
                }
            }
            if (!node) {
                node = create_fn(spec);
            }
            if (node) {
                parent->append_child(node.get());
                next_storage.push_back(std::move(node));
                next_keys.push_back(std::move(key));
            }
        }

        owned_storage = std::move(next_storage);
        owned_keys = std::move(next_keys);
        // old_map 中剩余未复用的 shared_ptr 在作用域结束时自动安全释放
    }
};

} // namespace ktui
