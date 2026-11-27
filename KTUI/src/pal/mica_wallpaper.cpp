#include <ktui/presentation/mica_backdrop.hpp>
#include <ktui/core/light_mutex.hpp>
#include <kernetile/fx/dual_kawase_blur.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace ktui {

namespace {

struct BlurredWallpaperCache {
    LightMutex mutex;
    bool loaded{false};
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint8_t> rgba; // 降采样并 DualKawase 强模糊后的壁纸 RGBA8
};

BlurredWallpaperCache& cache() {
    static BlurredWallpaperCache s_cache;
    return s_cache;
}

uint32_t pack_rgba8(float r, float g, float b, float a) noexcept {
    const auto cr = static_cast<uint32_t>(std::clamp(std::lround(r * 255.0f), 0L, 255L));
    const auto cg = static_cast<uint32_t>(std::clamp(std::lround(g * 255.0f), 0L, 255L));
    const auto cb = static_cast<uint32_t>(std::clamp(std::lround(b * 255.0f), 0L, 255L));
    const auto ca = static_cast<uint32_t>(std::clamp(std::lround(a * 255.0f), 0L, 255L));
    return cr | (cg << 8u) | (cb << 16u) | (ca << 24u);
}

void generate_fallback_ambient_wallpaper(uint32_t& out_w, uint32_t& out_h, std::vector<uint8_t>& out_rgba) {
    out_w = 128;
    out_h = 72;
    out_rgba.resize(static_cast<size_t>(out_w) * out_h * 4);
    for (uint32_t y = 0; y < out_h; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(out_h - 1);
        for (uint32_t x = 0; x < out_w; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(out_w - 1);
            // 真实还原 Windows 11 Dark Bloom (img19) 的深色花瓣光场分布
            const float d_center = std::hypot((u - 0.50f) * 1.2f, (v - 0.52f));
            const float d_left   = std::hypot((u - 0.40f) * 1.4f, (v - 0.45f));
            const float d_right  = std::hypot((u - 0.62f) * 1.3f, (v - 0.58f));
            const float g_core   = std::exp(-d_center * d_center * 5.0f);
            const float g_cyan   = std::exp(-d_left * d_left * 8.5f);
            const float g_violet = std::exp(-d_right * d_right * 7.5f);

            float r = 0.04f + 0.06f * g_core + 0.05f * g_cyan + 0.16f * g_violet;
            float g = 0.055f + 0.22f * g_core + 0.38f * g_cyan + 0.08f * g_violet;
            float b = 0.095f + 0.52f * g_core + 0.62f * g_cyan + 0.42f * g_violet;

            const size_t idx = (static_cast<size_t>(y) * out_w + x) * 4;
            out_rgba[idx + 0] = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
            out_rgba[idx + 1] = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
            out_rgba[idx + 2] = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));
            out_rgba[idx + 3] = 255;
        }
    }
}

#if defined(_WIN32)
std::wstring get_windows_wallpaper_path() {
    WCHAR path[MAX_PATH] = {0};
    if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0) && path[0] != L'\0') {
        const DWORD attrs = GetFileAttributesW(path);
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return std::wstring(path);
        }
    }
    WCHAR app_data[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"APPDATA", app_data, MAX_PATH) > 0) {
        std::wstring fallback = std::wstring(app_data) + L"\\Microsoft\\Windows\\Themes\\TranscodedWallpaper";
        const DWORD attrs = GetFileAttributesW(fallback.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return fallback;
        }
    }
    return L"";
}

bool load_windows_wallpaper_downsampled(uint32_t& out_w, uint32_t& out_h, std::vector<uint8_t>& out_rgba) {
    const std::wstring path = get_windows_wallpaper_path();
    if (path.empty()) return false;

    const HRESULT co_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool did_co_init = SUCCEEDED(co_hr);

    bool ok = false;
    {
        using Microsoft::WRL::ComPtr;
        ComPtr<IWICImagingFactory> factory;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
        if (SUCCEEDED(hr) && factory) {
            ComPtr<IWICBitmapDecoder> decoder;
            hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &decoder);
            if (SUCCEEDED(hr) && decoder) {
                ComPtr<IWICBitmapFrameDecode> frame;
                if (SUCCEEDED(decoder->GetFrame(0, &frame)) && frame) {
                    UINT orig_w = 0, orig_h = 0;
                    frame->GetSize(&orig_w, &orig_h);
                    if (orig_w > 0 && orig_h > 0) {
                        // 对齐 CUI.Core：降采样到 <=256px 抹除高频细节，仅保留壁纸宏观色彩倾向
                        constexpr UINT kMaxDim = 256;
                        UINT target_w = orig_w;
                        UINT target_h = orig_h;
                        if (target_w > kMaxDim || target_h > kMaxDim) {
                            if (target_w >= target_h) {
                                target_h = static_cast<UINT>((static_cast<uint64_t>(target_h) * kMaxDim) / target_w);
                                target_w = kMaxDim;
                            } else {
                                target_w = static_cast<UINT>((static_cast<uint64_t>(target_w) * kMaxDim) / target_h);
                                target_h = kMaxDim;
                            }
                        }
                        target_w = std::max(target_w, 16u);
                        target_h = std::max(target_h, 16u);

                        ComPtr<IWICBitmapSource> src = frame;
                        ComPtr<IWICBitmapScaler> scaler;
                        if ((target_w != orig_w || target_h != orig_h) &&
                            SUCCEEDED(factory->CreateBitmapScaler(&scaler)) && scaler &&
                            SUCCEEDED(scaler->Initialize(frame.Get(), target_w, target_h, WICBitmapInterpolationModeLinear))) {
                            src = scaler;
                        }

                        ComPtr<IWICFormatConverter> converter;
                        if (SUCCEEDED(factory->CreateFormatConverter(&converter)) && converter &&
                            SUCCEEDED(converter->Initialize(src.Get(), GUID_WICPixelFormat32bppRGBA,
                                                            WICBitmapDitherTypeNone, nullptr, 0.0,
                                                            WICBitmapPaletteTypeMedianCut))) {
                            const UINT stride = target_w * 4;
                            std::vector<uint8_t> pixels(static_cast<size_t>(stride) * target_h);
                            if (SUCCEEDED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()))) {
                                out_w = target_w;
                                out_h = target_h;
                                out_rgba = std::move(pixels);
                                ok = true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (did_co_init) {
        CoUninitialize();
    }
    return ok;
}
#endif

void ensure_wallpaper_loaded(BlurredWallpaperCache& c) {
    if (c.loaded) return;

    uint32_t w = 0, h = 0;
    std::vector<uint8_t> raw;
#if defined(_WIN32)
    if (!load_windows_wallpaper_downsampled(w, h, raw)) {
        generate_fallback_ambient_wallpaper(w, h, raw);
    }
#else
    generate_fallback_ambient_wallpaper(w, h, raw);
#endif

    // 使用 KerneTile 自研 DualKawaseBlur 执行单次离线强模糊 (在 <=256px 缓冲上 radius=12/offset=1.5 ≈ 全屏 90px 高斯核)
    // 既彻底抹除壁纸高频细节，又完整保留桌面宏观色彩空间梯度（避免过度双遍模糊坍缩为单一灰平均色）
    kernetile::fx::BlurParams params{};
    params.quality = kernetile::fx::BlurQuality::High;
    params.radius = 12.0f;
    params.offset_scale = 1.5f;
    std::vector<uint8_t> blurred(raw.size());
    kernetile::fx::DualKawaseBlur::blur_cpu(raw.data(), w, h, blurred.data(), params);

    c.width = w;
    c.height = h;
    c.rgba = std::move(blurred);
    c.loaded = true;
}

// W3C / Direct2D D2D1_BLEND_MODE_LUMINOSITY 精确亮度计算与色度保留 SetLum 实现
// 与 Windows 11 DWM MicaController 内部 Luminosity Blend 管线完全一致
inline float w3c_lum(float r, float g, float b) noexcept {
    return 0.30f * r + 0.59f * g + 0.11f * b;
}

inline void w3c_clip_color(float& r, float& g, float& b) noexcept {
    const float l = w3c_lum(r, g, b);
    const float mn = std::min({r, g, b});
    const float mx = std::max({r, g, b});
    if (mn < 0.0f) {
        const float denom = std::max(1e-6f, l - mn);
        r = l + ((r - l) * l) / denom;
        g = l + ((g - l) * l) / denom;
        b = l + ((b - l) * l) / denom;
    }
    if (mx > 1.0f) {
        const float denom = std::max(1e-6f, mx - l);
        r = l + ((r - l) * (1.0f - l)) / denom;
        g = l + ((g - l) * (1.0f - l)) / denom;
        b = l + ((b - l) * (1.0f - l)) / denom;
    }
}

inline void w3c_set_lum(float& r, float& g, float& b, float target_lum) noexcept {
    const float d = target_lum - w3c_lum(r, g, b);
    r += d;
    g += d;
    b += d;
    w3c_clip_color(r, g, b);
}

void sample_bilinear(const BlurredWallpaperCache& c, float u, float v, float& out_r, float& out_g, float& out_b) noexcept {
    if (c.width == 0 || c.height == 0 || c.rgba.empty()) {
        out_r = out_g = out_b = 0.0f;
        return;
    }
    const float x = std::clamp(u, 0.0f, 1.0f) * static_cast<float>(c.width - 1);
    const float y = std::clamp(v, 0.0f, 1.0f) * static_cast<float>(c.height - 1);
    const uint32_t x0 = static_cast<uint32_t>(x);
    const uint32_t y0 = static_cast<uint32_t>(y);
    const uint32_t x1 = std::min(x0 + 1u, c.width - 1u);
    const uint32_t y1 = std::min(y0 + 1u, c.height - 1u);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);

    auto fetch = [&](uint32_t px, uint32_t py, int ch) -> float {
        return static_cast<float>(c.rgba[(static_cast<size_t>(py) * c.width + px) * 4 + ch]) * (1.0f / 255.0f);
    };

    auto bilerp = [&](int ch) -> float {
        const float top = fetch(x0, y0, ch) + (fetch(x1, y0, ch) - fetch(x0, y0, ch)) * fx;
        const float bot = fetch(x0, y1, ch) + (fetch(x1, y1, ch) - fetch(x0, y1, ch)) * fx;
        return top + (bot - top) * fy;
    };

    out_r = bilerp(0);
    out_g = bilerp(1);
    out_b = bilerp(2);
}

} // namespace

void MicaBackdropEngine::invalidate_wallpaper() noexcept {
    auto& c = cache();
    LightLock lock(c.mutex);
    c.loaded = false;
    c.width = 0;
    c.height = 0;
    c.rgba.clear();
}

bool MicaBackdropEngine::build_mica_grid(WindowBackdrop backdrop,
                                         bool is_dark,
                                         float active_factor,
                                         Rect monitor_uv_rect,
                                         std::array<uint32_t, 16>& out_grid) {
    if (backdrop == WindowBackdrop::None) {
        return false;
    }

    // 严格对齐 Microsoft Windows 11 WinUI 3 MicaController 官方参数表:
    //   Mica (Base)    Dark : Tint=#202020, TintOpacity=0.80, LuminosityOpacity=1.00, Fallback=#202020
    //   Mica (Base)    Light: Tint=#F3F3F3, TintOpacity=0.50, LuminosityOpacity=1.00, Fallback=#F3F3F3
    //   MicaAlt(Tabbed) Dark : Tint=#0A0A0A, TintOpacity=0.00, LuminosityOpacity=0.96, Fallback=#202020
    //   MicaAlt(Tabbed) Light: Tint=#DADADA, TintOpacity=0.50, LuminosityOpacity=1.00, Fallback=#E8E8E8
    float tint_r, tint_g, tint_b;
    float fallback_r, fallback_g, fallback_b;
    float tint_opacity;
    float luminosity_opacity;

    if (backdrop == WindowBackdrop::MicaAlt) {
        if (is_dark) {
            tint_r = tint_g = tint_b = 0x0A / 255.0f;
            fallback_r = fallback_g = fallback_b = 0x20 / 255.0f;
            tint_opacity = 0.00f;
            luminosity_opacity = 0.96f;
        } else {
            tint_r = tint_g = tint_b = 0xDA / 255.0f;
            fallback_r = fallback_g = fallback_b = 0xE8 / 255.0f;
            tint_opacity = 0.50f;
            luminosity_opacity = 1.00f;
        }
    } else {
        if (is_dark) {
            tint_r = tint_g = tint_b = 0x20 / 255.0f;
            fallback_r = fallback_g = fallback_b = 0x20 / 255.0f;
            tint_opacity = 0.80f;
            luminosity_opacity = 1.00f;
        } else {
            tint_r = tint_g = tint_b = 0xF3 / 255.0f;
            fallback_r = fallback_g = fallback_b = 0xF3 / 255.0f;
            tint_opacity = 0.50f;
            luminosity_opacity = 1.00f;
        }
    }

    const float clamped_active = std::clamp(active_factor, 0.0f, 1.0f);

    // 完全失焦时按 Windows 11 规范退化为官方 Fallback 纯色
    if (clamped_active <= 0.001f) {
        const uint32_t solid = pack_rgba8(fallback_r, fallback_g, fallback_b, 1.0f);
        out_grid.fill(solid);
        return true;
    }

    const float tint_lum = w3c_lum(tint_r, tint_g, tint_b);
    const float sat_boost = is_dark ? 1.15f : 1.05f;

    auto& c = cache();
    LightLock lock(c.mutex);
    ensure_wallpaper_loaded(c);

    for (uint32_t iy = 0; iy < 4; ++iy) {
        const float ty = static_cast<float>(iy) / 3.0f;
        const float v = monitor_uv_rect.min_y + monitor_uv_rect.height() * ty;
        for (uint32_t ix = 0; ix < 4; ++ix) {
            const float tx = static_cast<float>(ix) / 3.0f;
            const float u = monitor_uv_rect.min_x + monitor_uv_rect.width() * tx;
            float wr = fallback_r, wg = fallback_g, wb = fallback_b;
            sample_bilinear(c, u, v, wr, wg, wb);

            // 轻微色彩饱和度补偿（对齐 DWM HostBackdrop 鲜活度）
            const float wp_lum = w3c_lum(wr, wg, wb);
            wr = std::clamp(wp_lum + (wr - wp_lum) * sat_boost, 0.0f, 1.0f);
            wg = std::clamp(wp_lum + (wg - wp_lum) * sat_boost, 0.0f, 1.0f);
            wb = std::clamp(wp_lum + (wb - wp_lum) * sat_boost, 0.0f, 1.0f);

            // Stage 1: D2D1 Luminosity Blend (将壁纸亮度拉齐至主题 Tint 亮度，完整保留壁纸色相与饱和度)
            float sr = wr, sg = wg, sb = wb;
            w3c_set_lum(sr, sg, sb, tint_lum);
            const float lr = wr + (sr - wr) * luminosity_opacity;
            const float lg = wg + (sg - wg) * luminosity_opacity;
            const float lb = wb + (sb - wb) * luminosity_opacity;

            // Stage 2: TintColor Source-Over (按官方 TintOpacity 叠加主题色层)
            const float mr = tint_r * tint_opacity + lr * (1.0f - tint_opacity);
            const float mg = tint_g * tint_opacity + lg * (1.0f - tint_opacity);
            const float mb = tint_b * tint_opacity + lb * (1.0f - tint_opacity);

            // Stage 3: 聚焦/失焦平滑过渡至 FallbackColor
            const float r = fallback_r + (mr - fallback_r) * clamped_active;
            const float g = fallback_g + (mg - fallback_g) * clamped_active;
            const float b = fallback_b + (mb - fallback_b) * clamped_active;
            out_grid[iy * 4u + ix] = pack_rgba8(r, g, b, 1.0f);
        }
    }
    return true;
}

} // namespace ktui
