#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>

namespace kernetile::cpu {

struct ImageCompareResult {
    uint32_t max_lsb_diff{0};     // 最大像素分量绝对差值 (0-255)
    double   mean_abs_error{0.0}; // 平均绝对误差 (MAE)
    double   mse{0.0};            // 均方误差 (MSE)
    double   psnr{0.0};           // 峰值信噪比 (dB)
    double   ssim{1.0};           // 结构相似度 (SSIM, [0.0, 1.0])
    uint32_t differing_pixels{0}; // 存在任何分量差异的像素数
};

class ImageCompare {
public:
    // 计算两张 RGBA8 格式图像的像素级误差与结构相似度 (SSIM)
    static ImageCompareResult compare_rgba8(
        const uint8_t* img_a,
        const uint8_t* img_b,
        uint32_t width,
        uint32_t height)
    {
        ImageCompareResult res{};
        if (width == 0 || height == 0 || !img_a || !img_b) {
            return res;
        }

        size_t total_pixels = static_cast<size_t>(width) * height;
        size_t total_samples = total_pixels * 4;

        double sum_abs_err = 0.0;
        double sum_sq_err = 0.0;

        double mean_a = 0.0;
        double mean_b = 0.0;

        for (size_t i = 0; i < total_pixels; ++i) {
            bool pixel_diff = false;
            for (int c = 0; c < 4; ++c) {
                size_t idx = i * 4 + c;
                int diff = std::abs(static_cast<int>(img_a[idx]) - static_cast<int>(img_b[idx]));
                if (diff > 0) {
                    pixel_diff = true;
                    if (static_cast<uint32_t>(diff) > res.max_lsb_diff) {
                        res.max_lsb_diff = static_cast<uint32_t>(diff);
                    }
                }
                sum_abs_err += diff;
                sum_sq_err += static_cast<double>(diff * diff);

                mean_a += img_a[idx];
                mean_b += img_b[idx];
            }
            if (pixel_diff) {
                res.differing_pixels++;
            }
        }

        res.mean_abs_error = sum_abs_err / static_cast<double>(total_samples);
        res.mse = sum_sq_err / static_cast<double>(total_samples);

        if (res.mse < 1e-12) {
            res.psnr = 999.0;
            res.ssim = 1.0;
            return res;
        }

        res.psnr = 10.0 * std::log10((255.0 * 255.0) / res.mse);

        // 计算全局结构相似度 SSIM
        mean_a /= static_cast<double>(total_samples);
        mean_b /= static_cast<double>(total_samples);

        double var_a = 0.0;
        double var_b = 0.0;
        double cov_ab = 0.0;

        for (size_t i = 0; i < total_samples; ++i) {
            double da = img_a[i] - mean_a;
            double db = img_b[i] - mean_b;
            var_a += da * da;
            var_b += db * db;
            cov_ab += da * db;
        }

        var_a /= static_cast<double>(total_samples);
        var_b /= static_cast<double>(total_samples);
        cov_ab /= static_cast<double>(total_samples);

        const double c1 = (0.01 * 255.0) * (0.01 * 255.0);
        const double c2 = (0.03 * 255.0) * (0.03 * 255.0);

        double numerator = (2.0 * mean_a * mean_b + c1) * (2.0 * cov_ab + c2);
        double denominator = (mean_a * mean_a + mean_b * mean_b + c1) * (var_a + var_b + c2);

        res.ssim = numerator / denominator;
        return res;
    }
};

} // namespace kernetile::cpu
