#pragma once

// ============================================================================
// KerneTile 轻量统一诊断日志宏 (Zero-overhead in Release)
// 默认仅在 Debug 构建或显式定义 KERNETILE_ENABLE_LOGS 时输出诊断信息，
// 避免 Release 产物中残留冗余格式化字符串或污染宿主进程终端。
// ============================================================================

#if defined(KERNETILE_ENABLE_LOGS) || !defined(NDEBUG)
#include <cstdio>
#define KT_LOG_INFO(fmt, ...) std::fprintf(stderr, "[KerneTile] " fmt "\n", ##__VA_ARGS__)
#define KT_LOG_WARN(fmt, ...) std::fprintf(stderr, "[KerneTile WARN] " fmt "\n", ##__VA_ARGS__)
#define KT_LOG_ERR(fmt, ...)  std::fprintf(stderr, "[KerneTile ERROR] " fmt "\n", ##__VA_ARGS__)
#else
#define KT_LOG_INFO(fmt, ...) ((void)0)
#define KT_LOG_WARN(fmt, ...) ((void)0)
#define KT_LOG_ERR(fmt, ...)  ((void)0)
#endif
