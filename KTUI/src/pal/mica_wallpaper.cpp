#include <ktui/presentation/mica_backdrop.hpp>
#include <kernetile/fx/dual_kawase_blur.hpp>
#include <algorithm>
#include <cmath>
#include <mutex>
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
    std::mutex mutex;
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
            // 柔和的 Windows 11 Bloom 风格环境色底图（供非 Windows 平台或无壁纸环境兜底）
            const float d1 = std::hypot(u - 0.25f, v - 0.30f);
            const float d2 = std::hypot(u - 0.78f, v - 0.72f);
            const float g1 = std::exp(-d1 * d1 * 4.5f);
            const float g2 = std::exp(-d2 * d2 * 4.0f);
            float r = 0.08f + 0.16f * g1 + 0.36f * g2;
            float g = 0.13f + 0.42f * g1 + 0.22f * g2;
            float b = 0.26f + 0.62f * g1 + 0.56f * g2;
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

    // 使用 KerneTile 自研 DualKawaseBlur 执行离线强模糊 (抹除壁纸纹理，仅留柔和云母光感)
    kernetile::fx::BlurParams params{};
    params.quality = kernetile::fx::BlurQuality::High;
    params.radius = 24.0f;
    params.offset_scale = 2.0f;
    std::vector<uint8_t> pass1(raw.size());
    std::vector<uint8_t> pass2(raw.size());
    kernetile::fx::DualKawaseBlur::blur_cpu(raw.data(), w, h, pass1.data(), params);
    kernetile::fx::DualKawaseBlur::blur_cpu(pass1.data(), w, h, pass2.data(), params);

    c.width = w;
    c.height = h;
    c.rgba = std::move(pass2);
    c.loaded = true;
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
    std::lock_guard<std::mutex> lock(c.mutex);
    c.loaded = false;
    c.width = 0;
    c.height = 0;
    c.rgba.clear();
}

bool MicaBackdropEngine::build_mica_grid(WindowBackdrop backdrop,
                                         bool is_dark,
                                         bool is_active,
                                         Rect monitor_uv_rect,
                                         std::array<uint32_t, 16>& out_grid) {
    if (backdrop == WindowBackdrop::None) {
        return false;
    }

    // 对齐 CUI.Core DrawMicaBackground / DrawMicaAltBackground 的精准色值与混合系数
    float base_r = is_dark ? (0x20 / 255.0f) : (0xEA / 255.0f);
    float base_g = is_dark ? (0x20 / 255.0f) : (0xED / 255.0f);
    float base_b = is_dark ? (0x20 / 255.0f) : (0xF1 / 255.0f);

    if (backdrop == WindowBackdrop::MicaAlt) {
        if (is_dark) {
            base_r *= 0.72f;
            base_g *= 0.72f;
            base_b *= 0.76f;
        } else {
            base_r *= 0.94f;
            base_g *= 0.95f;
            base_b *= 0.97f;
        }
    }

    // 失焦时按 Windows 11 / CUI.Core 规范退化为纯净主题底色
    if (!is_active) {
        const uint32_t solid = pack_rgba8(base_r, base_g, base_b, 1.0f);
        out_grid.fill(solid);
        return true;
    }

    const float wp_opacity = (backdrop == WindowBackdrop::MicaAlt)
        ? (is_dark ? 0.34f : 0.38f)
        : (is_dark ? 0.20f : 0.24f);

    auto& c = cache();
    std::lock_guard<std::mutex> lock(c.mutex);
    ensure_wallpaper_loaded(c);

    for (uint32_t iy = 0; iy < 4; ++iy) {
        const float ty = static_cast<float>(iy) / 3.0f;
        const float v = monitor_uv_rect.min_y + monitor_uv_rect.height() * ty;
        for (uint32_t ix = 0; ix < 4; ++ix) {
            const float tx = static_cast<float>(ix) / 3.0f;
            const float u = monitor_uv_rect.min_x + monitor_uv_rect.width() * tx;
            float wr = base_r, wg = base_g, wb = base_b;
            sample_bilinear(c, u, v, wr, wg, wb);
            // Source-over 混合: wallpaper(opacity) over opaque baseBg
            const float r = wr * wp_opacity + base_r * (1.0f - wp_opacity);
            const float g = wg * wp_opacity + base_g * (1.0f - wp_opacity);
            const float b = wb * wp_opacity + base_b * (1.0f - wp_opacity);
            out_grid[iy * 4u + ix] = pack_rgba8(r, g, b, 1.0f);
        }
    }
    return true;
}

} // namespace ktui
