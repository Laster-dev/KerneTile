#include <ktui/pal/file_dialog.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <unistd.h>
#endif

namespace ktui {

namespace {

FileDialog::MockHandler& mock_handler_storage() {
    static FileDialog::MockHandler s_mock;
    return s_mock;
}

#if defined(_WIN32)

std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}

std::string wide_to_utf8(const wchar_t* wide) {
    if (!wide || !*wide) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), needed - 1, nullptr, nullptr);
    return out;
}

FileDialogResult show_win32_dialog(const FileDialogOptions& opt) {
    FileDialogResult result;

    HRESULT hr_com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool com_inited = SUCCEEDED(hr_com);

    const bool is_save = (opt.kind == FileDialogKind::SaveFile);
    IFileDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(
        is_save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        is_save ? IID_IFileSaveDialog : IID_IFileOpenDialog,
        reinterpret_cast<void**>(&dialog));

    if (FAILED(hr) || !dialog) {
        if (com_inited) CoUninitialize();
        return result;
    }

    DWORD flags = 0;
    if (SUCCEEDED(dialog->GetOptions(&flags))) {
        flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
        if (opt.kind == FileDialogKind::SelectFolder) {
            flags |= FOS_PICKFOLDERS;
        } else if (opt.kind == FileDialogKind::OpenMultipleFiles) {
            flags |= FOS_ALLOWMULTISELECT;
        }
        dialog->SetOptions(flags);
    }

    if (!opt.title.empty()) {
        std::wstring wtitle = utf8_to_wide(opt.title);
        dialog->SetTitle(wtitle.c_str());
    }
    if (!opt.default_name.empty()) {
        std::wstring wname = utf8_to_wide(opt.default_name);
        dialog->SetFileName(wname.c_str());
    }
    if (!opt.default_dir.empty()) {
        using SHCreateItemFromParsingNameFn = HRESULT(WINAPI*)(PCWSTR, IBindCtx*, REFIID, void**);
        HMODULE hshell = LoadLibraryW(L"shell32.dll");
        auto fn_create_item = hshell
            ? reinterpret_cast<SHCreateItemFromParsingNameFn>(GetProcAddress(hshell, "SHCreateItemFromParsingName"))
            : nullptr;
        if (fn_create_item) {
            std::wstring wdir = utf8_to_wide(opt.default_dir);
            IShellItem* folder_item = nullptr;
            if (SUCCEEDED(fn_create_item(wdir.c_str(), nullptr, IID_PPV_ARGS(&folder_item))) && folder_item) {
                dialog->SetFolder(folder_item);
                folder_item->Release();
            }
        }
    }

    std::vector<std::wstring> filter_names;
    std::vector<std::wstring> filter_specs;
    std::vector<COMDLG_FILTERSPEC> specs;
    if (!opt.filters.empty() && opt.kind != FileDialogKind::SelectFolder) {
        filter_names.reserve(opt.filters.size());
        filter_specs.reserve(opt.filters.size());
        specs.reserve(opt.filters.size());
        for (const auto& f : opt.filters) {
            filter_names.push_back(utf8_to_wide(f.name.empty() ? f.pattern : f.name));
            filter_specs.push_back(utf8_to_wide(f.pattern.empty() ? "*.*" : f.pattern));
            specs.push_back(COMDLG_FILTERSPEC{filter_names.back().c_str(), filter_specs.back().c_str()});
        }
        dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
        dialog->SetFileTypeIndex(1);
    }

    HWND owner = static_cast<HWND>(opt.owner_window_handle);
    hr = dialog->Show(owner);
    if (SUCCEEDED(hr)) {
        if (opt.kind == FileDialogKind::OpenMultipleFiles) {
            IFileOpenDialog* open_dlg = nullptr;
            if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&open_dlg))) && open_dlg) {
                IShellItemArray* items = nullptr;
                if (SUCCEEDED(open_dlg->GetResults(&items)) && items) {
                    DWORD count = 0;
                    items->GetCount(&count);
                    for (DWORD i = 0; i < count; ++i) {
                        IShellItem* item = nullptr;
                        if (SUCCEEDED(items->GetItemAt(i, &item)) && item) {
                            PWSTR path = nullptr;
                            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                                result.paths.push_back(wide_to_utf8(path));
                                CoTaskMemFree(path);
                            }
                            item->Release();
                        }
                    }
                    items->Release();
                }
                open_dlg->Release();
            }
        } else {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                    result.paths.push_back(wide_to_utf8(path));
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        result.accepted = !result.paths.empty();
    }

    dialog->Release();
    if (com_inited) CoUninitialize();
    return result;
}

#elif defined(__EMSCRIPTEN__)

FileDialog::Callback& web_pending_callback() {
    static FileDialog::Callback s_cb;
    return s_cb;
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
void ktui_web_on_file_dialog_result(int accepted, const char* paths_joined, const uint8_t* bytes, size_t byte_len) {
    FileDialogResult res;
    res.accepted = (accepted != 0);
    if (paths_joined && *paths_joined) {
        std::string s(paths_joined);
        size_t pos = 0;
        while (pos < s.size()) {
            size_t next = s.find('\n', pos);
            if (next == std::string::npos) {
                res.paths.push_back(s.substr(pos));
                break;
            }
            res.paths.push_back(s.substr(pos, next - pos));
            pos = next + 1;
        }
    }
    if (bytes && byte_len > 0) {
        res.file_bytes.assign(bytes, bytes + byte_len);
    }
    auto cb = std::move(web_pending_callback());
    web_pending_callback() = nullptr;
    if (cb) cb(res);
}
}

EM_JS(void, ktui_js_open_file_dialog, (int kind, const char* accept_str), {
    var input = document.createElement('input');
    input.type = 'file';
    if (kind === 1) input.multiple = true;
    if (kind === 3) {
        input.webkitdirectory = true;
        input.directory = true;
    }
    var accept = accept_str ? UTF8ToString(accept_str) : '';
    if (accept && kind !== 3) input.accept = accept;
    input.style.display = 'none';
    document.body.appendChild(input);

    var finished = false;
    var finish = function(accepted, names, bytes) {
        if (finished) return;
        finished = true;
        if (input.parentNode) input.parentNode.removeChild(input);
        var joined = names.join('\n');
        var nameLen = lengthBytesUTF8(joined) + 1;
        var namePtr = _malloc(nameLen);
        stringToUTF8(joined, namePtr, nameLen);
        var dataPtr = 0;
        var dataLen = 0;
        if (bytes && bytes.length > 0) {
            dataLen = bytes.length;
            dataPtr = _malloc(dataLen);
            HEAPU8.set(bytes, dataPtr);
        }
        _ktui_web_on_file_dialog_result(accepted ? 1 : 0, namePtr, dataPtr, dataLen);
        _free(namePtr);
        if (dataPtr) _free(dataPtr);
    };

    input.addEventListener('change', function() {
        var files = input.files;
        if (!files || files.length === 0) {
            finish(false, [], null);
            return;
        }
        var names = [];
        for (var i = 0; i < files.length; ++i) {
            names.push(files[i].webkitRelativePath || files[i].name);
        }
        var first = files[0];
        if (first && first.arrayBuffer) {
            first.arrayBuffer().then(function(buf) {
                finish(true, names, new Uint8Array(buf));
            }).catch(function() {
                finish(true, names, null);
            });
        } else {
            finish(true, names, null);
        }
    });
    input.addEventListener('cancel', function() { finish(false, [], null); });
    input.click();
});

EM_JS(void, ktui_js_save_file_download, (const char* filename, const uint8_t* data, size_t len), {
    var name = (filename && UTF8ToString(filename)) || 'download.bin';
    var bytes = (data && len > 0) ? HEAPU8.slice(data, data + len) : new Uint8Array(0);
    var blob = new Blob([bytes], {type: 'application/octet-stream'});
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = name;
    document.body.appendChild(a);
    a.click();
    setTimeout(function() {
        URL.revokeObjectURL(url);
        if (a.parentNode) a.parentNode.removeChild(a);
    }, 100);
});

#elif defined(__linux__) && !defined(__ANDROID__)

std::string shell_escape_single(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

bool command_exists(const char* name) {
    std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

std::string run_pipe_capture(const std::string& cmd, int& exit_code) {
    exit_code = -1;
    std::FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    std::string output;
    char buf[512];
    while (std::fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
    exit_code = pclose(pipe);
    return output;
}

// 优先通过 xdg-desktop-portal (经由 zenity/kdialog/xdg-portal 桌面门户协议) 获取原生文件对话框
FileDialogResult show_linux_dialog(const FileDialogOptions& opt) {
    FileDialogResult res;
    std::string cmd;
    bool is_zenity = command_exists("zenity");
    bool is_kdialog = !is_zenity && command_exists("kdialog");

    if (is_zenity) {
        cmd = "zenity --file-selection";
        if (!opt.title.empty()) cmd += " --title=" + shell_escape_single(opt.title);
        if (opt.kind == FileDialogKind::SaveFile) cmd += " --save --confirm-overwrite";
        else if (opt.kind == FileDialogKind::SelectFolder) cmd += " --directory";
        else if (opt.kind == FileDialogKind::OpenMultipleFiles) cmd += " --multiple --separator='\n'";
        if (!opt.default_name.empty()) {
            std::string full = opt.default_dir.empty() ? opt.default_name : (opt.default_dir + "/" + opt.default_name);
            cmd += " --filename=" + shell_escape_single(full);
        } else if (!opt.default_dir.empty()) {
            cmd += " --filename=" + shell_escape_single(opt.default_dir + "/");
        }
        for (const auto& f : opt.filters) {
            std::string pat = f.pattern;
            std::replace(pat.begin(), pat.end(), ';', ' ');
            std::string spec = (f.name.empty() ? pat : (f.name + " | " + pat));
            cmd += " --file-filter=" + shell_escape_single(spec);
        }
    } else if (is_kdialog) {
        cmd = "kdialog";
        if (!opt.title.empty()) cmd += " --title " + shell_escape_single(opt.title);
        if (opt.kind == FileDialogKind::SaveFile) cmd += " --getsavefilename ";
        else if (opt.kind == FileDialogKind::SelectFolder) cmd += " --getexistingdirectory ";
        else if (opt.kind == FileDialogKind::OpenMultipleFiles) cmd += " --getopenfilename --multiple --separate-output ";
        else cmd += " --getopenfilename ";
        cmd += shell_escape_single(opt.default_dir.empty() ? "." : opt.default_dir);
    } else {
        return res;
    }

    int code = -1;
    std::string out = run_pipe_capture(cmd, code);
    if (code != 0 || out.empty()) return res;

    size_t pos = 0;
    while (pos < out.size()) {
        size_t nl = out.find('\n', pos);
        std::string line = (nl == std::string::npos) ? out.substr(pos) : out.substr(pos, nl - pos);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (!line.empty()) res.paths.push_back(std::move(line));
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    res.accepted = !res.paths.empty();
    return res;
}

#endif

} // namespace

void FileDialog::set_mock_handler(MockHandler handler) {
    mock_handler_storage() = std::move(handler);
}

void FileDialog::clear_mock_handler() {
    mock_handler_storage() = nullptr;
}

FileDialogResult FileDialog::show_sync(const FileDialogOptions& options) {
    if (auto& mock = mock_handler_storage()) {
        return mock(options);
    }
#if defined(_WIN32)
    return show_win32_dialog(options);
#elif defined(__linux__) && !defined(__ANDROID__)
    return show_linux_dialog(options);
#else
    return FileDialogResult{};
#endif
}

void FileDialog::show(const FileDialogOptions& options, Callback on_complete) {
    if (auto& mock = mock_handler_storage()) {
        FileDialogResult res = mock(options);
        if (on_complete) on_complete(res);
        return;
    }
#if defined(__EMSCRIPTEN__)
    if (options.kind == FileDialogKind::SaveFile) {
        std::string name = options.default_name.empty() ? "download.bin" : options.default_name;
        ktui_js_save_file_download(name.c_str(), options.save_bytes.data(), options.save_bytes.size());
        FileDialogResult res;
        res.accepted = true;
        res.paths.push_back(name);
        if (on_complete) on_complete(res);
        return;
    }
    web_pending_callback() = std::move(on_complete);
    std::string accept;
    for (const auto& f : options.filters) {
        std::string p = f.pattern;
        size_t pos = 0;
        while (pos < p.size()) {
            size_t sc = p.find(';', pos);
            std::string tok = (sc == std::string::npos) ? p.substr(pos) : p.substr(pos, sc - pos);
            if (tok.size() >= 2 && tok[0] == '*' && tok[1] == '.') {
                if (!accept.empty()) accept += ',';
                accept += tok.substr(1); // ".png"
            }
            if (sc == std::string::npos) break;
            pos = sc + 1;
        }
    }
    ktui_js_open_file_dialog(static_cast<int>(options.kind), accept.c_str());
#else
    FileDialogResult res = show_sync(options);
    if (on_complete) on_complete(res);
#endif
}

} // namespace ktui
