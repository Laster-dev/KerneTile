#pragma once

#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>

inline void configure_test_environment() {
    // 禁用 Windows 断言失败弹出的 GUI 模态窗口，直接向 stderr 输出并退出
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
}
#else
inline void configure_test_environment() {}
#endif
