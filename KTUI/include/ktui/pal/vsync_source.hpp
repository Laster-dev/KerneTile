#pragma once

#include <ktui/core/fn.hpp>
#include <cstdint>

namespace ktui {

// 垂直同步帧时钟源接口 (VSync Frame Ticker)
class IVSyncSource {
public:
    virtual ~IVSyncSource() = default;

    // 请求下一帧回调，返回非零请求 ID
    virtual uint64_t request_frame(Fn<void(double timestamp_seconds)> callback) = 0;

    // 取消指定的帧请求
    virtual void cancel_frame(uint64_t request_id) = 0;

    // 检查时钟源是否处于休眠状态 (当无动画和待绘制请求时应为 true，实现 0% CPU 沉睡)
    virtual bool is_sleeping() const = 0;
};

} // namespace ktui
