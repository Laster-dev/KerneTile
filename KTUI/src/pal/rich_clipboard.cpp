#if defined(KTUI_ENABLE_RICH_CLIPBOARD) && KTUI_ENABLE_RICH_CLIPBOARD

#include <ktui/pal/clipboard.hpp>
#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace ktui {

#if defined(_WIN32)

bool Clipboard::has_image() {
    if (!s_backend) return s_fallback_image.valid();
    return IsClipboardFormatAvailable(CF_DIB) != FALSE;
}

ClipboardImage Clipboard::get_image() {
    if (!s_backend) return s_fallback_image;
    ClipboardImage out;
    if (!OpenClipboard(nullptr)) return out;

    HANDLE hdata = GetClipboardData(CF_DIB);
    if (hdata) {
        const auto* bih = static_cast<const BITMAPINFOHEADER*>(GlobalLock(hdata));
        if (bih && bih->biSize >= sizeof(BITMAPINFOHEADER) &&
            bih->biWidth > 0 && bih->biHeight != 0 &&
            (bih->biBitCount == 24 || bih->biBitCount == 32) &&
            (bih->biCompression == BI_RGB || bih->biCompression == BI_BITFIELDS))
        {
            const uint32_t w = static_cast<uint32_t>(bih->biWidth);
            const bool top_down = bih->biHeight < 0;
            const uint32_t h = static_cast<uint32_t>(top_down ? -bih->biHeight : bih->biHeight);
            if (w <= 4096 && h <= 4096) {
                size_t palette_bytes = 0;
                if (bih->biCompression == BI_BITFIELDS && bih->biSize == sizeof(BITMAPINFOHEADER)) {
                    palette_bytes = 12;
                }
                const uint8_t* src = reinterpret_cast<const uint8_t*>(bih) + bih->biSize + palette_bytes;
                const uint32_t src_bpp = bih->biBitCount / 8u;
                const size_t row_stride = ((static_cast<size_t>(w) * src_bpp) + 3u) & ~size_t(3u);

                out.width = w;
                out.height = h;
                out.rgba8.resize(static_cast<size_t>(w) * h * 4u);
                for (uint32_t y = 0; y < h; ++y) {
                    const uint32_t dst_y = top_down ? y : (h - 1u - y);
                    const uint8_t* srow = src + static_cast<size_t>(y) * row_stride;
                    uint8_t* drow = out.rgba8.data() + static_cast<size_t>(dst_y) * w * 4u;
                    for (uint32_t x = 0; x < w; ++x) {
                        const uint8_t* sp = srow + static_cast<size_t>(x) * src_bpp;
                        drow[x * 4u + 0] = sp[2];
                        drow[x * 4u + 1] = sp[1];
                        drow[x * 4u + 2] = sp[0];
                        drow[x * 4u + 3] = (src_bpp == 4u) ? sp[3] : 255u;
                    }
                }
            }
        }
        GlobalUnlock(hdata);
    }

    CloseClipboard();
    return out;
}

bool Clipboard::set_image(const ClipboardImage& image) {
    if (!image.valid()) return false;
    if (!s_backend) {
        s_fallback_image = image;
        return true;
    }
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    const size_t pixel_bytes = static_cast<size_t>(image.width) * image.height * 4u;
    const size_t total_bytes = sizeof(BITMAPINFOHEADER) + pixel_bytes;
    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, total_bytes);
    if (!hmem) {
        CloseClipboard();
        return false;
    }
    auto* dst = static_cast<uint8_t*>(GlobalLock(hmem));
    if (!dst) {
        GlobalFree(hmem);
        CloseClipboard();
        return false;
    }

    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = static_cast<LONG>(image.width);
    bih.biHeight = -static_cast<LONG>(image.height); // top-down DIB
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = static_cast<DWORD>(pixel_bytes);
    std::memcpy(dst, &bih, sizeof(bih));

    uint8_t* dpix = dst + sizeof(bih);
    for (size_t i = 0; i < static_cast<size_t>(image.width) * image.height; ++i) {
        dpix[i * 4u + 0] = image.rgba8[i * 4u + 2]; // B
        dpix[i * 4u + 1] = image.rgba8[i * 4u + 1]; // G
        dpix[i * 4u + 2] = image.rgba8[i * 4u + 0]; // R
        dpix[i * 4u + 3] = image.rgba8[i * 4u + 3]; // A
    }
    GlobalUnlock(hmem);

    bool ok = (SetClipboardData(CF_DIB, hmem) != nullptr);
    if (!ok) GlobalFree(hmem);
    CloseClipboard();
    return ok;
}

bool Clipboard::has_files() {
    if (!s_backend) return !s_fallback_files.empty();
    return IsClipboardFormatAvailable(CF_HDROP) != FALSE;
}

std::vector<std::string> Clipboard::get_files() {
    if (!s_backend) return s_fallback_files;
    std::vector<std::string> out;
    if (!OpenClipboard(nullptr)) return out;

    HANDLE hdata = GetClipboardData(CF_HDROP);
    if (hdata) {
        HDROP hdrop = static_cast<HDROP>(GlobalLock(hdata));
        if (hdrop) {
            UINT count = DragQueryFileW(hdrop, 0xFFFFFFFFu, nullptr, 0);
            out.reserve(count);
            for (UINT i = 0; i < count; ++i) {
                UINT wlen = DragQueryFileW(hdrop, i, nullptr, 0);
                if (wlen > 0) {
                    std::wstring wpath(static_cast<size_t>(wlen) + 1, L'\0');
                    DragQueryFileW(hdrop, i, wpath.data(), wlen + 1);
                    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), static_cast<int>(wlen),
                                                       nullptr, 0, nullptr, nullptr);
                    if (utf8_len > 0) {
                        std::string utf8(static_cast<size_t>(utf8_len), '\0');
                        WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), static_cast<int>(wlen),
                                            utf8.data(), utf8_len, nullptr, nullptr);
                        out.push_back(std::move(utf8));
                    }
                }
            }
            GlobalUnlock(hdata);
        }
    }
    CloseClipboard();
    return out;
}

bool Clipboard::set_files(const std::vector<std::string>& paths) {
    if (paths.empty()) return false;
    if (!s_backend) {
        s_fallback_files = paths;
        return true;
    }
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    std::wstring joined;
    for (const auto& p : paths) {
        int needed = MultiByteToWideChar(CP_UTF8, 0, p.data(), static_cast<int>(p.size()), nullptr, 0);
        if (needed > 0) {
            std::wstring wp(static_cast<size_t>(needed), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, p.data(), static_cast<int>(p.size()), wp.data(), needed);
            joined += wp;
            joined.push_back(L'\0');
        }
    }
    joined.push_back(L'\0'); // 双 '\0' 结尾

    const size_t total_bytes = sizeof(DROPFILES) + joined.size() * sizeof(wchar_t);
    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total_bytes);
    if (!hmem) {
        CloseClipboard();
        return false;
    }
    auto* df = static_cast<DROPFILES*>(GlobalLock(hmem));
    if (!df) {
        GlobalFree(hmem);
        CloseClipboard();
        return false;
    }
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    std::memcpy(reinterpret_cast<uint8_t*>(df) + sizeof(DROPFILES),
                joined.data(), joined.size() * sizeof(wchar_t));
    GlobalUnlock(hmem);

    bool ok = (SetClipboardData(CF_HDROP, hmem) != nullptr);
    if (!ok) GlobalFree(hmem);
    CloseClipboard();
    return ok;
}

#else

bool Clipboard::has_image() { return s_fallback_image.valid(); }
ClipboardImage Clipboard::get_image() { return s_fallback_image; }
bool Clipboard::set_image(const ClipboardImage& image) {
    if (!image.valid()) return false;
    s_fallback_image = image;
    return true;
}
bool Clipboard::has_files() { return !s_fallback_files.empty(); }
std::vector<std::string> Clipboard::get_files() { return s_fallback_files; }
bool Clipboard::set_files(const std::vector<std::string>& paths) {
    s_fallback_files = paths;
    return !paths.empty();
}

#endif

} // namespace ktui

#endif // KTUI_ENABLE_RICH_CLIPBOARD
