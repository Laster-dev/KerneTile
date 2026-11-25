#pragma once

#include "event_types.hpp"
#include <ktui/tree/render_box.hpp>

namespace ktui {

// 全局焦点管理器 (Focus Manager)
// 负责管理当前聚焦控件节点与焦点切换通知
class FocusManager {
public:
    FocusManager() = default;

    // 获取当前处于聚焦状态的节点
    [[nodiscard]] RenderBox* focused_node() const noexcept {
        return m_focused_node;
    }

    // 请求将焦点转移到指定节点
    void request_focus(RenderBox* node) {
        if (m_focused_node == node) {
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
        }
    }

    // 清除焦点
    void clear_focus() {
        request_focus(nullptr);
    }

private:
    RenderBox* m_focused_node{nullptr};
};

} // namespace ktui
