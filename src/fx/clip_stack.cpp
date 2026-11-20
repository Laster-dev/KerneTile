#include "kernetile/fx/clip_stack.hpp"
#include <algorithm>
#include <cmath>

namespace kernetile::fx {

uint32_t ClipStack::push_clip(
    math::Rect rect,
    float corner_radius,
    uint32_t flags)
{
    core::ClipRect clip{};
    clip.min_x = rect.min_x;
    clip.min_y = rect.min_y;
    clip.max_x = rect.max_x;
    clip.max_y = rect.max_y;

    // 约束圆角半径不超过矩形短边的一半
    float max_r = std::min((rect.max_x - rect.min_x) * 0.5f, (rect.max_y - rect.min_y) * 0.5f);
    clip.corner_radius = std::clamp(corner_radius, 0.0f, std::max(0.0f, max_r));

    clip.flags = flags;
    clip.reserved = 0;

    if (!m_active_stack.empty()) {
        clip.parent_index = m_active_stack.back();
    } else {
        clip.parent_index = core::INVALID_INDEX_U32;
    }

    uint32_t index = static_cast<uint32_t>(m_clips.size());
    m_clips.push_back(clip);
    m_active_stack.push_back(index);

    return index;
}

void ClipStack::pop_clip() {
    if (!m_active_stack.empty()) {
        m_active_stack.pop_back();
    }
}

uint32_t ClipStack::current_clip_index() const {
    if (m_active_stack.empty()) {
        return core::INVALID_INDEX_U32;
    }
    return m_active_stack.back();
}

void ClipStack::clear() {
    m_clips.clear();
    m_active_stack.clear();
}

math::Rect ClipStack::intersect_rect(const math::Rect& a, const math::Rect& b) {
    math::Rect res{};
    res.min_x = std::max(a.min_x, b.min_x);
    res.min_y = std::max(a.min_y, b.min_y);
    res.max_x = std::min(a.max_x, b.max_x);
    res.max_y = std::min(a.max_y, b.max_y);

    if (res.min_x > res.max_x || res.min_y > res.max_y) {
        return math::Rect{0.0f, 0.0f, 0.0f, 0.0f};
    }
    return res;
}

bool ClipStack::is_pixel_culled(math::Vec2 pixel_pos, uint32_t clip_index) const {
    uint32_t curr = clip_index;
    uint32_t safety_counter = 0;

    while (curr != core::INVALID_INDEX_U32 && curr < m_clips.size() && safety_counter < 64) {
        safety_counter++;
        const auto& clip = m_clips[curr];

        // 1. 直角 AABB 边界判定
        if (pixel_pos.x < clip.min_x || pixel_pos.x > clip.max_x ||
            pixel_pos.y < clip.min_y || pixel_pos.y > clip.max_y)
        {
            return true; // 落在裁剪盒外，直接剔除
        }

        // 2. 圆角 SDF 距离判定
        if (clip.corner_radius > 0.0f) {
            float cx = (clip.min_x + clip.max_x) * 0.5f;
            float cy = (clip.min_y + clip.max_y) * 0.5f;
            float hx = (clip.max_x - clip.min_x) * 0.5f;
            float hy = (clip.max_y - clip.min_y) * 0.5f;
            float r = clip.corner_radius;

            // Inigo Quilez sdRoundedBox 距离评估
            float qx = std::abs(pixel_pos.x - cx) - hx + r;
            float qy = std::abs(pixel_pos.y - cy) - hy + r;

            float outside_len = std::sqrt(std::max(0.0f, qx) * std::max(0.0f, qx) +
                                          std::max(0.0f, qy) * std::max(0.0f, qy));
            float inside_dist = std::min(std::max(qx, qy), 0.0f);
            float d = inside_dist + outside_len - r;

            if (d > 0.0f) {
                return true; // 位于圆角外部剔除区域
            }
        }

        // 向上沿父节点级联递归检查
        curr = clip.parent_index;
    }

    return false; // 全部通过检查，像素可见
}

} // namespace kernetile::fx
