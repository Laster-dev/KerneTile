#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 轻量级时间值类型 (24 小时制存储: hour 0..23, minute 0..59)
struct TimeOfDay {
    int hour{9};
    int minute{0};

    bool operator==(const TimeOfDay&) const = default;

    [[nodiscard]] bool is_valid() const noexcept {
        return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
    }

    [[nodiscard]] std::string format(bool use_24h = true) const {
        char buf[32];
        if (use_24h) {
            std::snprintf(buf, sizeof(buf), "%02d:%02d", std::clamp(hour, 0, 23), std::clamp(minute, 0, 59));
        } else {
            int h12 = hour % 12;
            if (h12 == 0) h12 = 12;
            std::snprintf(buf, sizeof(buf), "%02d:%02d %s", h12, std::clamp(minute, 0, 59), (hour >= 12) ? "PM" : "AM");
        }
        return std::string(buf);
    }

    [[nodiscard]] static TimeOfDay now() {
        std::time_t t = std::time(nullptr);
        std::tm tm_val{};
#ifdef _WIN32
        localtime_s(&tm_val, &t);
#else
        localtime_r(&t, &tm_val);
#endif
        return TimeOfDay{tm_val.tm_hour, tm_val.tm_min};
    }
};

} // namespace ktui
