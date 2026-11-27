#include "../gallery_pages.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace ktui;

namespace gallery::pages {

namespace {

#if defined(_WIN32)
std::string wide_to_utf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), needed, nullptr, nullptr);
    return out;
}

struct SystemUserAvatar {
    std::string username;
    std::string source_hint;
    std::shared_ptr<ImageBuffer> image;
};

const SystemUserAvatar& load_windows_user_avatar() {
    static SystemUserAvatar cached = []() {
        SystemUserAvatar info;

        // 1. 读取当前 Windows 用户名
        WCHAR uname[256] = {0};
        DWORD uname_len = 256;
        if (GetUserNameW(uname, &uname_len) && uname[0] != L'\0') {
            info.username = wide_to_utf8(uname);
        } else {
            info.username = "Windows User";
        }

        // 2. 优先通过 shell32.dll 导出函数 SHGetUserPicturePathW (Ordinal 261) 获取当前系统用户真实头像
        auto try_load_wpath = [&](const wchar_t* wpath, const char* hint) -> bool {
            if (!wpath || !*wpath) return false;
            DWORD attrs = GetFileAttributesW(wpath);
            if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) return false;
            std::string utf8 = wide_to_utf8(wpath);
            auto buf = ImageBuffer::load_file(utf8, 256);
            if (buf && buf->valid()) {
                info.image = std::move(buf);
                info.source_hint = hint;
                return true;
            }
            return false;
        };

        using ShGetUserPicFn = HRESULT(WINAPI*)(LPCWSTR, DWORD, LPWSTR, UINT);
        if (HMODULE shell32 = LoadLibraryW(L"shell32.dll")) {
            auto fn = reinterpret_cast<ShGetUserPicFn>(GetProcAddress(shell32, MAKEINTRESOURCEA(261)));
            if (fn) {
                WCHAR pic_path[MAX_PATH] = {0};
                // 0x80000000 = SHGUPP_FLAG_CREATE，自动将系统账号头像导出为标准位图文件
                if (SUCCEEDED(fn(nullptr, 0x80000000u, pic_path, MAX_PATH))) {
                    try_load_wpath(pic_path, "Windows 当前账户头像");
                }
            }
            FreeLibrary(shell32);
        }

        // 3. 回退至 Windows 系统默认账户头像目录
        if (!info.image) {
            WCHAR prog_data[MAX_PATH] = {0};
            if (GetEnvironmentVariableW(L"ProgramData", prog_data, MAX_PATH) > 0) {
                const std::wstring base = std::wstring(prog_data) + L"\\Microsoft\\User Account Pictures\\";
                const wchar_t* candidates[] = {L"user-192.png", L"user.png", L"user-48.png", L"user.bmp"};
                for (const wchar_t* name : candidates) {
                    std::wstring full = base + name;
                    if (try_load_wpath(full.c_str(), "Windows 默认系统用户头像")) {
                        break;
                    }
                }
            }
        }
        return info;
    }();
    return cached;
}
#endif

} // namespace

WidgetPtr build_person_picture_page(GalleryApp& app) {
#if defined(_WIN32)
    const auto& sys_user = load_windows_user_avatar();
    std::string avatar_caption = sys_user.image
        ? ("当前登录用户: " + sys_user.username + " (" + sys_user.source_hint + ")")
        : ("当前登录用户: " + sys_user.username + " (未检测到系统头像文件，自动回退首字母头像)");
#endif

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("PersonPicture", 22.0f),
            app.make_muted("WinUI 3 用户头像组件，支持本地/内存图片圆形等比裁剪、中英文姓名首字母自动提取、确定性 Fluent 配色与在线状态指示圆点。", 12.5f),
#if defined(_WIN32)
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "图片头像展示 (Windows 系统用户头像)", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 18.0f,
                        .content = {
                            PersonPicture { .display_name = sys_user.username, .diameter = 72.0f, .presence = PersonPresence::Available, .image = sys_user.image },
                            PersonPicture { .display_name = sys_user.username, .diameter = 56.0f, .presence = PersonPresence::Busy,      .image = sys_user.image },
                            PersonPicture { .display_name = sys_user.username, .diameter = 44.0f, .presence = PersonPresence::Away,      .image = sys_user.image },
                            PersonPicture { .display_name = sys_user.username, .diameter = 32.0f, .presence = PersonPresence::None,      .image = sys_user.image }
                        }
                    },
                    app.make_muted(avatar_caption.c_str(), 12.0f)
                }
            }),
#endif
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "多尺寸首字母头像与在线状态指示点", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 18.0f,
                        .content = {
                            PersonPicture { .display_name = "Jane Doe", .diameter = 64.0f, .presence = PersonPresence::Available },
                            PersonPicture { .display_name = "林思远", .diameter = 52.0f, .presence = PersonPresence::Busy },
                            PersonPicture { .display_name = "Alex Chen", .diameter = 44.0f, .presence = PersonPresence::Away },
                            PersonPicture { .display_name = "KerneTile", .diameter = 36.0f, .presence = PersonPresence::Offline }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages
