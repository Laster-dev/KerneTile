#pragma once

#include "../widget.hpp"
#include <ktui/core/strings.hpp>
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 轻量级公历日期值类型 (0/0/0 表示未选择)
struct CalendarDate {
    int year{0};
    int month{0};
    int day{0};

    bool operator==(const CalendarDate&) const = default;

    [[nodiscard]] static bool is_leap_year(int y) noexcept {
        return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    }

    [[nodiscard]] static int days_in_month(int y, int m) noexcept {
        static constexpr int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (m < 1 || m > 12) return 30;
        if (m == 2 && is_leap_year(y)) return 29;
        return kDays[m - 1];
    }

    // 返回星期几: 0 = 周日, 1 = 周一, ..., 6 = 周六 (Sakamoto 算法)
    [[nodiscard]] static int day_of_week(int y, int m, int d) noexcept {
        static constexpr int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (m < 3) y -= 1;
        return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    }

    [[nodiscard]] bool is_valid() const noexcept {
        return year >= 1900 && year <= 2200 &&
               month >= 1 && month <= 12 &&
               day >= 1 && day <= days_in_month(year, month);
    }

    [[nodiscard]] int compare(const CalendarDate& o) const noexcept {
        if (year != o.year) return (year < o.year) ? -1 : 1;
        if (month != o.month) return (month < o.month) ? -1 : 1;
        if (day != o.day) return (day < o.day) ? -1 : 1;
        return 0;
    }

    [[nodiscard]] std::string format_iso() const {
        if (!is_valid()) return {};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
        return std::string(buf);
    }

    [[nodiscard]] static CalendarDate today() {
        std::time_t now = std::time(nullptr);
        std::tm tm_val{};
#ifdef _WIN32
        localtime_s(&tm_val, &now);
#else
        localtime_r(&now, &tm_val);
#endif
        return CalendarDate{tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday};
    }
};

} // namespace ktui
