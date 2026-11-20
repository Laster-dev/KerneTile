#pragma once

#include <ktui/pal/platform_types.hpp>
#include <vector>
#include <algorithm>

namespace ktui {

class RenderBox;

// 单个命中记录
struct HitTestEntry {
    RenderBox* target{nullptr};
    PointF local_position{0.0f, 0.0f};
};

// 命中测试收集路径 (Hit Test Result Path)
class HitTestResult {
public:
    HitTestResult() = default;

    // 添加命中节点 (通常在子节点命中返回时加入)
    void add(RenderBox* target, PointF local_pos) {
        m_path.push_back({target, local_pos});
    }

    [[nodiscard]] const std::vector<HitTestEntry>& path() const noexcept {
        return m_path;
    }

    [[nodiscard]] std::vector<HitTestEntry>& path() noexcept {
        return m_path;
    }

    [[nodiscard]] bool is_empty() const noexcept {
        return m_path.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        return m_path.size();
    }

    void clear() noexcept {
        m_path.clear();
    }

    // 获取最深层的叶子命中节点 (最前端)
    [[nodiscard]] RenderBox* first_target() const noexcept {
        return m_path.empty() ? nullptr : m_path.front().target;
    }

    // 获取根部命中节点
    [[nodiscard]] RenderBox* last_target() const noexcept {
        return m_path.empty() ? nullptr : m_path.back().target;
    }

private:
    std::vector<HitTestEntry> m_path;
};

} // namespace ktui
