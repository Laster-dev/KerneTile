#pragma once

#include "test_common.hpp"
#include "kernetile/core/types.hpp"
#include "kernetile/core/primitives.hpp"
#include "kernetile/math/color.hpp"
#include "kernetile/cpu/image_compare.hpp"
#include "kernetile/text/font_engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#ifndef KT_E2E_REQUIRE_ALL
#define KT_E2E_REQUIRE_ALL 0
#endif

// Always-active check macros (never compiled out under -DNDEBUG in Release builds)
#define KT_E2E_CHECK(expr)                                                               \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " #expr "\n";    \
            std::exit(1);                                                                \
        }                                                                                \
    } while (0)

#define KT_E2E_CHECK_MSG(expr, msg)                                                      \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " #expr          \
                      << " (" << (msg) << ")\n";                                         \
            std::exit(1);                                                                \
        }                                                                                \
    } while (0)

namespace kernetile_e2e {

// ============================================================================
// C++20 Concept Probes for Opt-In R1-R6 APIs (PROJECT.md § Interface Contracts)
// ============================================================================

// R1 & R2: RendererConfig
template <typename T>
concept HasRendererDitheringConfig = requires(T cfg) {
    { cfg.enable_dithering } -> std::convertible_to<bool>;
};

template <typename T>
concept HasRendererPixelSnappingConfig = requires(T cfg) {
    { cfg.enable_pixel_snapping } -> std::convertible_to<bool>;
};

// R1 & R2 & R5: KerneTileRenderer
template <typename T>
concept HasRendererDitheringApi = requires(T& r, const T& cr, bool b) {
    r.set_dithering_enabled(b);
    { cr.is_dithering_enabled() } -> std::convertible_to<bool>;
};

template <typename T>
concept HasRendererPixelSnappingApi = requires(T& r, const T& cr, bool b) {
    r.set_pixel_snapping_enabled(b);
    { cr.is_pixel_snapping_enabled() } -> std::convertible_to<bool>;
};

template <typename T>
concept HasRendererDrawTextTabular = requires(
    T& r,
    std::string_view sv,
    float f,
    const kernetile::math::Color& c,
    uint16_t w,
    bool b) {
    r.draw_text_scaled(sv, f, f, f, c, f, w, b, sv, b);
};

// R3: PrimitiveFlags::IsPointerReveal
template <typename T>
concept HasPointerRevealFlag = requires {
    T::IsPointerReveal;
};

// R5: ITextEngine, layout_text, TextStreamer
template <typename T>
concept HasTextEngineTabular = requires(T& e, std::string_view sv, float f, uint16_t w, bool b) {
    e.shape_text(sv, f, sv, w, b, b);
};

template <typename SV>
concept HasLayoutTextTabular = requires(SV sv, float f, uint16_t w, bool b) {
    kernetile::text::layout_text(sv, f, w, b, sv, b);
};

template <typename T>
concept HasTextStreamerTabular = requires(
    T& s,
    std::string_view sv,
    float f,
    const kernetile::math::Color& c,
    std::vector<kernetile::core::UiPrimitive>& out,
    uint16_t u16,
    uint16_t w,
    bool b) {
    s.stream_text(sv, f, f, f, c, out, u16, f, w, b, sv, b);
};

// R1 & R2: Window
template <typename T>
concept HasWindowDitheringApi = requires(T& win, const T& cwin, bool b) {
    win.set_dithering_enabled(b);
    { cwin.dithering_enabled() } -> std::convertible_to<bool>;
};

template <typename T>
concept HasWindowPixelSnappingApi = requires(T& win, const T& cwin, bool b) {
    win.set_pixel_snapping_enabled(b);
    { cwin.pixel_snapping_enabled() } -> std::convertible_to<bool>;
};

// R2 & R5: PaintContext
template <typename T>
concept HasPaintContextSnappingApi = requires(T& ctx, const T& cctx, bool b) {
    ctx.set_pixel_snapping(b);
    { cctx.pixel_snapping() } -> std::convertible_to<bool>;
};

template <typename T, typename PointT, typename ColorT, typename WeightT>
concept HasPaintContextDrawTextTabular = requires(
    T& ctx,
    std::string_view sv,
    PointT pos,
    float f,
    ColorT col,
    WeightT weight,
    bool b) {
    ctx.draw_text(sv, pos, f, col, weight, b, sv, b);
};

// R5: TextMeasure
template <typename T, typename WeightT>
concept HasTextMeasureTabular = requires(std::string_view sv, float f, WeightT fw, bool b) {
    T::measure(sv, f, fw, b, sv, b);
    T::measure_width(sv, f, fw, b, sv, b);
};

template <typename T>
concept HasTextMeasureCursorShort = requires(std::string_view sv, float f, size_t idx, bool b) {
    T::cursor_x(sv, f, idx, b);
    T::hit_test_cursor(sv, f, f, b);
};

template <typename T, typename WeightT>
concept HasTextMeasureCursorFull = requires(std::string_view sv, float f, size_t idx, WeightT fw, bool b) {
    T::cursor_x(sv, f, idx, fw, b, sv, b);
    T::hit_test_cursor(sv, f, f, fw, b, sv, b);
};

// R3: Button / ButtonData / NavigationView / NavigationViewData / SettingsCard
template <typename T>
concept HasEnablePointerRevealField = requires(T t) {
    { t.enable_pointer_reveal } -> std::convertible_to<bool>;
};

template <typename T>
concept HasPointerRevealBuilder = requires(T t, bool b) {
    t.pointer_reveal(b);
};

template <typename T>
concept HasPointerRevealSetter = requires(T& t, bool b) {
    t.set_pointer_reveal_enabled(b);
};

// R4: SmoothScrollPhysics / ScrollView / VirtualList
template <typename T>
concept HasRubberBandField = requires(T t) {
    { t.rubber_band_overscroll } -> std::convertible_to<bool>;
};

template <typename T>
concept HasRubberBandSetter = requires(T& t, bool b) {
    t.set_rubber_band_enabled(b);
};

template <typename T>
concept HasRubberBandGetter = requires(const T& t) {
    { t.rubber_band_enabled() } -> std::convertible_to<bool>;
};

template <typename T>
concept HasIsRubberBandGetter = requires(const T& t) {
    { t.is_rubber_band_enabled() } -> std::convertible_to<bool>;
};

template <typename T>
concept HasRubberBandBuilder = requires(T t, bool b) {
    t.rubber_band(b);
};

// R5: Text / RenderText
template <typename T>
concept HasTabularNumsField = requires(T t) {
    { t.tabular_nums } -> std::convertible_to<bool>;
};

template <typename T>
concept HasSetTabularNums = requires(T& t, bool b) {
    t.set_tabular_nums(b);
};

// Dependent helper wrappers so discarded branches never trigger non-dependent lookup errors
template <typename T>
inline void set_pointer_reveal_opt(T& target, bool enabled) {
    if constexpr (HasEnablePointerRevealField<T>) {
        target.enable_pointer_reveal = enabled;
    } else if constexpr (HasPointerRevealSetter<T>) {
        target.set_pointer_reveal_enabled(enabled);
    } else if constexpr (HasPointerRevealBuilder<T>) {
        target.pointer_reveal(enabled);
    }
}

template <typename T>
inline bool get_pointer_reveal_default(const T& target) {
    if constexpr (HasEnablePointerRevealField<T>) {
        return static_cast<bool>(target.enable_pointer_reveal);
    } else {
        (void)target;
        return false;
    }
}

template <typename T>
constexpr bool has_pointer_reveal_api() noexcept {
    return HasEnablePointerRevealField<T> || HasPointerRevealBuilder<T> || HasPointerRevealSetter<T>;
}

template <typename T>
inline void set_rubber_band_opt(T& target, bool enabled) {
    if constexpr (HasRubberBandSetter<T>) {
        target.set_rubber_band_enabled(enabled);
    } else if constexpr (HasRubberBandField<T>) {
        target.rubber_band_overscroll = enabled;
    } else if constexpr (HasRubberBandBuilder<T>) {
        target.rubber_band(enabled);
    }
}

template <typename T>
inline bool get_rubber_band_opt(const T& target) {
    if constexpr (HasRubberBandGetter<T>) {
        return static_cast<bool>(target.rubber_band_enabled());
    } else if constexpr (HasIsRubberBandGetter<T>) {
        return static_cast<bool>(target.is_rubber_band_enabled());
    } else if constexpr (HasRubberBandField<T>) {
        return static_cast<bool>(target.rubber_band_overscroll);
    } else {
        (void)target;
        return false;
    }
}

template <typename T>
constexpr bool has_rubber_band_api() noexcept {
    return HasRubberBandField<T> || HasRubberBandSetter<T> || HasRubberBandBuilder<T>;
}

template <typename TM, typename WeightT>
inline float measure_cursor_x_opt(std::string_view text, float size, size_t byte, WeightT weight,
                                  bool italic, std::string_view family, bool tabular_nums) {
    if constexpr (HasTextMeasureCursorFull<TM, WeightT>) {
        return TM::cursor_x(text, size, byte, weight, italic, family, tabular_nums);
    } else if constexpr (HasTextMeasureCursorShort<TM>) {
        (void)weight; (void)italic; (void)family;
        return TM::cursor_x(text, size, byte, tabular_nums);
    } else {
        (void)weight; (void)italic; (void)family; (void)tabular_nums;
        return TM::cursor_x(text, size, byte);
    }
}

template <typename TM, typename WeightT>
inline size_t hit_test_cursor_opt(std::string_view text, float size, float x, WeightT weight,
                                    bool italic, std::string_view family, bool tabular_nums) {
    if constexpr (HasTextMeasureCursorFull<TM, WeightT>) {
        return TM::hit_test_cursor(text, size, x, weight, italic, family, tabular_nums);
    } else if constexpr (HasTextMeasureCursorShort<TM>) {
        (void)weight; (void)italic; (void)family;
        return TM::hit_test_cursor(text, size, x, tabular_nums);
    } else {
        (void)weight; (void)italic; (void)family; (void)tabular_nums;
        return TM::hit_test_cursor(text, size, x);
    }
}

// ============================================================================
// Pixel, Image & Gradient Banding Analysis Helpers
// ============================================================================

inline uint64_t fnv1a_rgba8(const uint8_t* data, size_t byte_count) noexcept {
    uint64_t h = 14695981039346656037ull;
    if (!data) return h;
    for (size_t i = 0; i < byte_count; ++i) {
        h ^= static_cast<uint64_t>(data[i]);
        h *= 1099511628211ull;
    }
    return h;
}

inline uint64_t fnv1a_rgba8(const std::vector<uint8_t>& pixels) noexcept {
    return fnv1a_rgba8(pixels.data(), pixels.size());
}

inline size_t count_diff_pixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    KT_E2E_CHECK(a.size() == b.size());
    KT_E2E_CHECK(a.size() % 4 == 0);
    size_t diff = 0;
    const size_t n = a.size() / 4;
    for (size_t i = 0; i < n; ++i) {
        if (a[i * 4 + 0] != b[i * 4 + 0] ||
            a[i * 4 + 1] != b[i * 4 + 1] ||
            a[i * 4 + 2] != b[i * 4 + 2] ||
            a[i * 4 + 3] != b[i * 4 + 3]) {
            ++diff;
        }
    }
    return diff;
}

inline kernetile::cpu::ImageCompareResult compare_rgba8(
    const std::vector<uint8_t>& a,
    const std::vector<uint8_t>& b,
    uint32_t width,
    uint32_t height)
{
    KT_E2E_CHECK(a.size() == static_cast<size_t>(width) * height * 4);
    KT_E2E_CHECK(b.size() == static_cast<size_t>(width) * height * 4);
    return kernetile::cpu::ImageCompare::compare_rgba8(a.data(), b.data(), width, height);
}

inline bool has_fractional_alpha_bleed(
    const std::vector<uint8_t>& rgba,
    uint32_t width,
    uint32_t height,
    uint32_t x0,
    uint32_t y0,
    uint32_t x1,
    uint32_t y1)
{
    KT_E2E_CHECK(rgba.size() == static_cast<size_t>(width) * height * 4);
    x1 = std::min(x1, width);
    y1 = std::min(y1, height);
    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) {
            const uint8_t a = rgba[(static_cast<size_t>(y) * width + x) * 4 + 3];
            if (a > 0u && a < 255u) {
                return true;
            }
        }
    }
    return false;
}

inline bool has_fractional_color_bleed_col(
    const std::vector<uint8_t>& rgba,
    uint32_t width,
    uint32_t height,
    uint32_t x,
    uint32_t y0,
    uint32_t y1,
    const std::array<uint8_t, 4>& bg,
    const std::array<uint8_t, 4>& fg,
    uint32_t tolerance = 2)
{
    KT_E2E_CHECK(rgba.size() == static_cast<size_t>(width) * height * 4);
    if (x >= width) return false;
    y1 = std::min(y1, height);
    auto close_to = [tolerance](const uint8_t* px, const std::array<uint8_t, 4>& ref) {
        for (int c = 0; c < 4; ++c) {
            if (static_cast<uint32_t>(std::abs(static_cast<int>(px[c]) - static_cast<int>(ref[c]))) > tolerance) {
                return false;
            }
        }
        return true;
    };
    for (uint32_t y = y0; y < y1; ++y) {
        const uint8_t* px = &rgba[(static_cast<size_t>(y) * width + x) * 4];
        if (!close_to(px, bg) && !close_to(px, fg)) {
            return true;
        }
    }
    return false;
}

inline bool has_fractional_color_bleed_row(
    const std::vector<uint8_t>& rgba,
    uint32_t width,
    uint32_t height,
    uint32_t y,
    uint32_t x0,
    uint32_t x1,
    const std::array<uint8_t, 4>& bg,
    const std::array<uint8_t, 4>& fg,
    uint32_t tolerance = 2)
{
    KT_E2E_CHECK(rgba.size() == static_cast<size_t>(width) * height * 4);
    if (y >= height) return false;
    x1 = std::min(x1, width);
    auto close_to = [tolerance](const uint8_t* px, const std::array<uint8_t, 4>& ref) {
        for (int c = 0; c < 4; ++c) {
            if (static_cast<uint32_t>(std::abs(static_cast<int>(px[c]) - static_cast<int>(ref[c]))) > tolerance) {
                return false;
            }
        }
        return true;
    };
    for (uint32_t x = x0; x < x1; ++x) {
        const uint8_t* px = &rgba[(static_cast<size_t>(y) * width + x) * 4];
        if (!close_to(px, bg) && !close_to(px, fg)) {
            return true;
        }
    }
    return false;
}

inline uint32_t count_matching_pixels_col(
    const std::vector<uint8_t>& rgba,
    uint32_t width,
    uint32_t height,
    uint32_t x,
    uint32_t y0,
    uint32_t y1,
    const std::array<uint8_t, 4>& target,
    uint32_t tolerance = 2)
{
    KT_E2E_CHECK(rgba.size() == static_cast<size_t>(width) * height * 4);
    if (x >= width) return 0;
    y1 = std::min(y1, height);
    uint32_t count = 0;
    for (uint32_t y = y0; y < y1; ++y) {
        const uint8_t* px = &rgba[(static_cast<size_t>(y) * width + x) * 4];
        bool match = true;
        for (int c = 0; c < 4; ++c) {
            if (static_cast<uint32_t>(std::abs(static_cast<int>(px[c]) - static_cast<int>(target[c]))) > tolerance) {
                match = false;
                break;
            }
        }
        if (match) ++count;
    }
    return count;
}

inline uint32_t count_matching_pixels_row(
    const std::vector<uint8_t>& rgba,
    uint32_t width,
    uint32_t height,
    uint32_t y,
    uint32_t x0,
    uint32_t x1,
    const std::array<uint8_t, 4>& target,
    uint32_t tolerance = 2)
{
    KT_E2E_CHECK(rgba.size() == static_cast<size_t>(width) * height * 4);
    if (y >= height) return 0;
    x1 = std::min(x1, width);
    uint32_t count = 0;
    for (uint32_t x = x0; x < x1; ++x) {
        const uint8_t* px = &rgba[(static_cast<size_t>(y) * width + x) * 4];
        bool match = true;
        for (int c = 0; c < 4; ++c) {
            if (static_cast<uint32_t>(std::abs(static_cast<int>(px[c]) - static_cast<int>(target[c]))) > tolerance) {
                match = false;
                break;
            }
        }
        if (match) ++count;
    }
    return count;
}

struct GradientBandingStats {
    bool     monotonic_blocks{false};
    uint32_t max_flat_run{0};
    double   max_lsb_error{0.0};
    double   mean_abs_lsb_error{0.0};
    uint32_t spatial_dither_transitions{0};
};

// Analyze a 1D slice of RGBA8 values against an ideal linear ramp [ideal_start_0_255, ideal_end_0_255]
inline GradientBandingStats analyze_gradient_banding(
    const std::vector<uint8_t>& samples_u8,
    double ideal_start_255,
    double ideal_end_255)
{
    GradientBandingStats st{};
    if (samples_u8.empty()) return st;

    const size_t n = samples_u8.size();
    uint32_t cur_run = 1;
    st.max_flat_run = 1;
    double sum_abs_err = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double t = (n > 1) ? (static_cast<double>(i) / static_cast<double>(n - 1)) : 0.0;
        const double ideal = ideal_start_255 + (ideal_end_255 - ideal_start_255) * t;
        const double err = std::abs(static_cast<double>(samples_u8[i]) - ideal);
        st.max_lsb_error = std::max(st.max_lsb_error, err);
        sum_abs_err += err;

        if (i > 0) {
            if (samples_u8[i] == samples_u8[i - 1]) {
                ++cur_run;
                st.max_flat_run = std::max(st.max_flat_run, cur_run);
            } else {
                cur_run = 1;
                if (std::abs(static_cast<int>(samples_u8[i]) - static_cast<int>(samples_u8[i - 1])) == 1) {
                    ++st.spatial_dither_transitions;
                }
            }
        }
    }
    st.mean_abs_lsb_error = sum_abs_err / static_cast<double>(n);

    // Check monotonic trend across 16-pixel averaged blocks (tolerating ±0.5 LSB local IGN jitter)
    const size_t block_size = std::min<size_t>(16, std::max<size_t>(1, n / 4));
    std::vector<double> block_means;
    for (size_t off = 0; off + block_size <= n; off += block_size) {
        double s = 0.0;
        for (size_t k = 0; k < block_size; ++k) s += samples_u8[off + k];
        block_means.push_back(s / static_cast<double>(block_size));
    }
    st.monotonic_blocks = true;
    const bool ascending = (ideal_end_255 >= ideal_start_255);
    for (size_t b = 1; b < block_means.size(); ++b) {
        if (ascending) {
            if (block_means[b] + 0.25 < block_means[b - 1]) {
                st.monotonic_blocks = false;
                break;
            }
        } else {
            if (block_means[b] - 0.25 > block_means[b - 1]) {
                st.monotonic_blocks = false;
                break;
            }
        }
    }
    return st;
}

inline std::vector<uint8_t> extract_row_channel(
    const std::vector<uint8_t>& rgba,
    uint32_t width,
    uint32_t y,
    uint32_t x0,
    uint32_t x1,
    uint32_t channel)
{
    std::vector<uint8_t> out;
    out.reserve(x1 > x0 ? (x1 - x0) : 0);
    for (uint32_t x = x0; x < x1; ++x) {
        out.push_back(rgba[(static_cast<size_t>(y) * width + x) * 4 + channel]);
    }
    return out;
}

inline std::string read_workspace_file(std::string_view rel_path) {
    const std::array<std::string, 3> candidates = {
        std::string(rel_path),
        "../" + std::string(rel_path),
        "d:/KerneTile/" + std::string(rel_path)
    };
    for (const auto& p : candidates) {
        std::ifstream ifs(p, std::ios::binary);
        if (ifs.good()) {
            std::ostringstream ss;
            ss << ifs.rdbuf();
            return ss.str();
        }
    }
    return {};
}

// ============================================================================
// Test Registry & Runner
// ============================================================================

enum class CaseStatus {
    Passed,
    Pending,
    Failed
};

struct CaseResult {
    CaseStatus status{CaseStatus::Passed};
    std::string reason;
};

inline CaseResult passed_case() { return {CaseStatus::Passed, {}}; }
inline CaseResult pending_case(std::string reason) { return {CaseStatus::Pending, std::move(reason)}; }

class TestRunner {
public:
    explicit TestRunner(std::string suite_name) : m_suite_name(std::move(suite_name)) {
        configure_test_environment();
        std::cout << "=== " << m_suite_name
                  << " (KT_E2E_REQUIRE_ALL=" << KT_E2E_REQUIRE_ALL << ") ===\n";
    }

    template <typename Fn>
    void run_case(std::string_view id, Fn&& fn) {
        ++m_total;
        CaseResult res = fn();
        if (res.status == CaseStatus::Passed) {
            ++m_passed;
            std::cout << "[PASS] " << id << "\n";
        } else if (res.status == CaseStatus::Pending) {
            ++m_pending;
            std::cout << "[PENDING] " << id << ": " << res.reason << "\n";
        } else {
            ++m_failed;
            std::cerr << "[FAIL] " << id << ": " << res.reason << "\n";
        }
    }

    [[nodiscard]] uint32_t total() const noexcept { return m_total; }
    [[nodiscard]] uint32_t passed() const noexcept { return m_passed; }
    [[nodiscard]] uint32_t pending() const noexcept { return m_pending; }
    [[nodiscard]] uint32_t failed() const noexcept { return m_failed; }

    int finalize() const {
        std::cout << "=== Summary [" << m_suite_name << "]: total=" << m_total
                  << ", passed=" << m_passed
                  << ", pending=" << m_pending
                  << ", failed=" << m_failed << " ===\n";
        if (m_failed > 0) {
            std::cerr << "[FAIL] " << m_failed << " E2E test(s) failed.\n";
            return 1;
        }
        if (KT_E2E_REQUIRE_ALL != 0 && m_pending > 0) {
            std::cerr << "[FAIL] KT_E2E_REQUIRE_ALL!=0 and " << m_pending
                      << " test(s) remained [PENDING].\n";
            return 1;
        }
        return 0;
    }

private:
    std::string m_suite_name;
    uint32_t m_total{0};
    uint32_t m_passed{0};
    uint32_t m_pending{0};
    uint32_t m_failed{0};
};

} // namespace kernetile_e2e
