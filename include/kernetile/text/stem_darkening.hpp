#pragma once

#include <cstdint>
#include <span>
#include <cmath>
#include <algorithm>
#include <array>

namespace kernetile::text {

class StemDarkener {
public:
    // 计算特定字号下的感知等效 Gamma 缩放因子
    // 依据 Adobe CFF 模型: 字号越小，人眼在线性空间下对暗部细节敏感度衰减越严重
    // 小字号 (<= 12px) 对应 gamma ~ 1.5; 大字号 (>= 24px) 对应 gamma = 1.0 (保持原生矢量)
    [[nodiscard]] static constexpr float compute_gamma(float font_size) noexcept {
        if (font_size >= 24.0f) {
            return 1.0f; // 大字号无需补偿
        }
        if (font_size <= 4.0f) {
            return 1.8f; // 极端极限保护
        }
        // 当 s=11px 时 gamma ~ 1.50; 当 s=16px 时 gamma ~ 1.25; 当 s=24px 时 gamma = 1.0
        float k = 5.13f * (1.0f / std::sqrt(font_size) - 1.0f / std::sqrt(24.0f));
        float gamma = 1.0f + std::clamp(k, 0.0f, 0.8f);
        return gamma;
    }

    // 生成单字节快速覆盖率重映射查找表 (LUT: 256 字节)
    [[nodiscard]] static std::array<uint8_t, 256> build_lut(float font_size) noexcept {
        std::array<uint8_t, 256> lut{};
        float gamma = compute_gamma(font_size);
        if (gamma <= 1.001f) {
            for (uint32_t i = 0; i < 256; ++i) {
                lut[i] = static_cast<uint8_t>(i);
            }
            return lut;
        }

        float inv_gamma = 1.0f / gamma;
        for (uint32_t i = 0; i < 256; ++i) {
            float a = static_cast<float>(i) / 255.0f;
            float darkened = std::pow(a, inv_gamma);
            lut[i] = static_cast<uint8_t>(std::clamp(darkened * 255.0f + 0.5f, 0.0f, 255.0f));
        }
        return lut;
    }

    // 对 R8 灰度字形点阵应用 Stem Darkening 笔划补偿
    static void apply(std::span<uint8_t> bitmap, float font_size) noexcept {
        if (font_size >= 24.0f || bitmap.empty()) {
            return;
        }
        auto lut = build_lut(font_size);
        for (auto& pixel : bitmap) {
            pixel = lut[pixel];
        }
    }

    // ========================================================================
    // 暗色/亮色文字光渗补偿 (Irradiation Illusion Compensation / Font Gamma)
    // - 深色背景浅字 (polarity > 0): 人眼视觉光渗使白字显得偏粗，施加 gamma > 1.0 抑制溢光
    // - 浅色背景深字 (polarity < 0): 亮底向暗字笔画渗入使其偏细，施加 gamma < 1.0 增强笔画权重
    // - 随字号增大在 12px..28px 间平滑收敛至 1.0，保障中英文小字观感与笔画粗细一致
    // ========================================================================
    [[nodiscard]] static inline float compute_irradiation_gamma(
        float font_size_px,
        float fg_luma,
        float bg_luma,
        float bg_alpha = 1.0f) noexcept
    {
        const float size_weight = std::clamp((28.0f - font_size_px) / 16.0f, 0.0f, 1.0f);
        if (size_weight <= 0.001f) {
            return 1.0f;
        }
        const float polarity = std::clamp(fg_luma - bg_luma, -1.0f, 1.0f) * std::clamp(bg_alpha, 0.0f, 1.0f);
        if (polarity >= 0.0f) {
            return 1.0f + 0.36f * size_weight * polarity;
        }
        return 1.0f / (1.0f + 0.48f * size_weight * (-polarity));
    }

    [[nodiscard]] static inline float evaluate_irradiation_coverage(
        float cov,
        float font_size_px,
        float fg_luma,
        float bg_luma,
        float bg_alpha = 1.0f) noexcept
    {
        if (cov <= 0.0001f || cov >= 0.9999f) {
            return cov;
        }
        const float gamma = compute_irradiation_gamma(font_size_px, fg_luma, bg_luma, bg_alpha);
        if (std::abs(gamma - 1.0f) <= 0.001f) {
            return cov;
        }
        return std::clamp(std::pow(cov, gamma), 0.0f, 1.0f);
    }
};

} // namespace kernetile::text
