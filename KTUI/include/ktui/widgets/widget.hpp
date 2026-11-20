#pragma once

#include <ktui/tree/render_box.hpp>
#include <memory>
#include <vector>

namespace ktui {

class Widget;
using WidgetPtr = std::shared_ptr<Widget>;

// KTUI 声明式组件核心抽象基类 (Widget)
// 彻底解耦“声明式结构体描述”、“交互行为 (Behavior)”与“保留式场景图 (RenderBox)”
class Widget : public std::enable_shared_from_this<Widget> {
public:
    virtual ~Widget() = default;

    // 创建对应的保留式渲染节点 (RenderBox)
    [[nodiscard]] virtual std::shared_ptr<RenderBox> create_render_node() = 0;

    // 就地原地更新渲染节点，复用对象，杜绝无谓的析构重建
    virtual void update_render_node(std::shared_ptr<RenderBox> node) = 0;

    // 获取当前关联挂载的 RenderBox
    [[nodiscard]] std::shared_ptr<RenderBox> render_node() const noexcept {
        return m_render_node.lock();
    }

    void set_render_node(std::shared_ptr<RenderBox> node) noexcept {
        m_render_node = node;
    }

protected:
    std::weak_ptr<RenderBox> m_render_node;
};

} // namespace ktui
