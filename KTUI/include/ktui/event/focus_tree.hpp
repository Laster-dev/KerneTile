#pragma once

#include "event_types.hpp"
#include <ktui/tree/render_box.hpp>
#include <vector>
#include <algorithm>

namespace ktui {

// 全局焦点管理器 (Focus Manager)
// 管理键盘焦点、Tab / Shift+Tab 焦点链表循环转移与输入状态
class FocusManager {
public:
    FocusManager() = default;

    // 获取当前处于聚焦状态的节点
    [[nodiscard]] RenderBox* focused_node() const noexcept {
        return m_focused_node;
    }

    // 仅在通过键盘 Tab/Shift+Tab 导航获得焦点时显示 WinUI 3 键盘焦点环 (鼠标点击不显示)
    [[nodiscard]] bool is_keyboard_focus_visible() const noexcept {
        return m_keyboard_focus_visible && m_focused_node != nullptr;
    }

    // 请求将焦点转移到指定节点
    void request_focus(RenderBox* node, bool from_keyboard = false) {
        if (m_focused_node == node) {
            m_keyboard_focus_visible = from_keyboard && (node != nullptr);
            return;
        }

        // 旧节点失去焦点
        if (m_focused_node) {
            if (m_focused_node->on_focus) {
                FocusEvent lost_event(false);
                m_focused_node->on_focus(lost_event);
            }
        }

        m_focused_node = node;
        m_keyboard_focus_visible = from_keyboard && (node != nullptr);

        // 新节点获得焦点
        if (m_focused_node) {
            if (m_focused_node->on_focus) {
                FocusEvent gain_event(true);
                m_focused_node->on_focus(gain_event);
            }
        }
    }

    // 节点析构时安全移除悬垂焦点指针 (不触发已销毁对象的回调)
    void forget_node(RenderBox* node) noexcept {
        if (m_focused_node == node) {
            m_focused_node = nullptr;
            m_keyboard_focus_visible = false;
        }
    }

    // 清除焦点
    void clear_focus() {
        m_keyboard_focus_visible = false;
        request_focus(nullptr, false);
    }

    // 处理 Tab / Shift+Tab 导航键
    // reverse 为 true 表示 Shift+Tab 逆向转移
    bool handle_tab_navigation(RenderBox* root, bool reverse = false) {
        if (!root) return false;

        std::vector<RenderBox*> focusable_nodes;
        collect_focusable_nodes(root, focusable_nodes);

        if (focusable_nodes.empty()) {
            return false;
        }

        const size_t N = focusable_nodes.size();
        size_t current_idx = N; // 未找到

        for (size_t i = 0; i < N; ++i) {
            if (focusable_nodes[i] == m_focused_node) {
                current_idx = i;
                break;
            }
        }

        size_t next_idx = 0;
        if (current_idx == N) {
            // 当前未聚焦任何项，根据方向聚焦首项或末项
            next_idx = reverse ? (N - 1) : 0;
        } else {
            // 环形循环流转
            if (!reverse) {
                next_idx = (current_idx + 1) % N;
            } else {
                next_idx = (current_idx + N - 1) % N;
            }
        }

        request_focus(focusable_nodes[next_idx], true);
        return true;
    }

private:
    // 递归先序遍历收集所有可见的可聚焦节点
    void collect_focusable_nodes(RenderNode* current, std::vector<RenderBox*>& out_list) {
        if (!current) return;

        if (auto* box = dynamic_cast<RenderBox*>(current)) {
            if (box->is_focusable && box->size().x > 0.5f && box->size().y > 0.5f) {
                out_list.push_back(box);
            }
        }

        for (RenderNode* child = current->first_child(); child != nullptr; child = child->next_sibling()) {
            collect_focusable_nodes(child, out_list);
        }
    }

    RenderBox* m_focused_node{nullptr};
    bool m_keyboard_focus_visible{false};
};

} // namespace ktui
