#pragma once

#include "kernetile/core/types.hpp"
#include "kernetile/core/primitives.hpp"
#include "kernetile/math/rect.hpp"
#include "kernetile/math/vec.hpp"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace kernetile::fx {

// ============================================================================
// 嵌套裁剪栈 (ClipStack)
// 管理 UI 树层次结构中的多层矩形/圆角裁剪，支持 GPU 结构化缓冲直接打包上传
// 并提供与 GPU 着色器严格等价的层次剔除判定算法
// ============================================================================
class ClipStack {
public:
    ClipStack() = default;

    // 压入一个新裁剪区域，并与当前激活的裁剪区域建立父子级联
    // 返回该裁剪矩形在缓冲中的全局索引 index (可填入 UiPrimitive.clip_index)
    uint32_t push_clip(
        math::Rect rect,
        float corner_radius = 0.0f,
        uint32_t flags = 0,
        uint32_t reserved = 0);

    // 弹出当前栈顶的裁剪区域
    void pop_clip();

    // 获取当前处于激活状态的裁剪索引 (无裁剪时返回 INVALID_INDEX_U32)
    uint32_t current_clip_index() const;

    // 获取当前栈深度
    size_t depth() const { return m_active_stack.size(); }

    // 获取总裁剪矩形节点数 (供 GPU StructuredBuffer 上传)
    size_t count() const { return m_clips.size(); }

    // 获取所有裁剪节点数据 (每项严格为 32 字节的 ClipRect)
    const std::vector<core::ClipRect>& get_clips() const { return m_clips; }

    // 清空裁剪栈与所有已分配的裁剪矩形
    void clear();

    // 像素级裁剪判定 (与 GPU 计算着色器硬件逻辑 100% 对齐)
    // 返回 true: 像素点被剔除 (不可见); false: 像素保留 (可见)
    bool is_pixel_culled(math::Vec2 pixel_pos, uint32_t clip_index) const;

    // 计算两矩形 AABB 交集
    static math::Rect intersect_rect(const math::Rect& a, const math::Rect& b);

private:
    std::vector<core::ClipRect> m_clips;         // 扁平化裁剪节点列表 (供 GPU 共享)
    std::vector<uint32_t>       m_active_stack;  // 活跃层级索引栈
};

} // namespace kernetile::fx
