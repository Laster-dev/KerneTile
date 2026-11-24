#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdint>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>

namespace ktui {

class WebBackend;
class WebWindow;

// ==================== C / Wasm 互操作缓存与函数导出 ====================
namespace detail {
    inline std::vector<char> s_text_buf;
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    inline char* ktui_web_get_text_buf(size_t size) {
        detail::s_text_buf.resize(size);
        return detail::s_text_buf.data();
    }

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_ime_composition(const char* text, size_t cursor);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_ime_commit(const char* text);

    EMSCRIPTEN_KEEPALIVE
    int ktui_web_hit_test(int win_id, float x, float y);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_focus_window(int win_id);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_pointer(int win_id, int type, float x, float y, int button, int mods);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_wheel(int win_id, float x, float y, float dx, float dy, int mods);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_drag_move(int win_id, float x, float y);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_drag_resize(int win_id, float x, float y, float w, float h);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_restore_for_drag(int win_id, float x, float y);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_taskbar_click(int win_id);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_show_desktop(int mode);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_viewport_resize();
}

// ==================== Web 桌面环境与多窗口 DOM 桥接 ====================
EM_JS(void, ktui_js_init_desktop, (), {
    // 隐藏旧版单画布占位元素（若 shell.html 中残留）
    var legacy = document.getElementById('canvas');
    if (legacy && !legacy.dataset.ktuiWinId) {
        legacy.style.display = 'none';
    }

    // 注入基础样式（兜底：即使未使用 shell.html 也能获得无蓝闪、圆角边框与任务栏样式）
    if (!document.getElementById('ktui-web-desktop-style')) {
        var style = document.createElement('style');
        style.id = 'ktui-web-desktop-style';
        style.textContent =
            'html,body,#ktui-desktop,.ktui-win,.ktui-win canvas,#ktui-taskbar,#ktui-taskbar button{' +
                '-webkit-tap-highlight-color:transparent;tap-highlight-color:transparent;-webkit-touch-callout:none;}' +
            '.ktui-win{position:absolute;overflow:hidden;border-radius:0;background:#202020;}' +
            '.ktui-win::after{content:"";position:absolute;inset:0;pointer-events:none;z-index:2;' +
                'box-shadow:inset 0 0 0 1px rgba(255,255,255,0.14);transition:box-shadow 160ms ease;}' +
            '.ktui-win.inactive::after{box-shadow:inset 0 0 0 1px rgba(255,255,255,0.08);}' +
            '.ktui-win.maximized{border-radius:0;box-shadow:none!important;}' +
            '.ktui-win.maximized::after{box-shadow:none;}' +
            '.ktui-win canvas{display:block;outline:none;touch-action:none;}';
        document.head.appendChild(style);
    }

    var desktop = document.getElementById('ktui-desktop');
    if (!desktop) {
        desktop = document.createElement('div');
        desktop.id = 'ktui-desktop';
        desktop.style.cssText = 'position:fixed;inset:0;overflow:hidden;user-select:none;-webkit-user-select:none;touch-action:none;';
        document.body.appendChild(desktop);
    }

    var taskbar = document.getElementById('ktui-taskbar');
    if (!taskbar) {
        taskbar = document.createElement('div');
        taskbar.id = 'ktui-taskbar';
        taskbar.style.cssText =
            'position:fixed;left:0;right:0;bottom:0;height:48px;z-index:900000;' +
            'display:grid;grid-template-columns:1fr auto 1fr;align-items:center;padding:0 8px 0 12px;' +
            'background:rgba(22,24,31,0.82);backdrop-filter:blur(26px) saturate(160%);-webkit-backdrop-filter:blur(26px) saturate(160%);' +
            'border-top:1px solid rgba(255,255,255,0.09);font-family:"Segoe UI",system-ui,sans-serif;font-size:12px;';
        var leftSlot = document.createElement('div');
        leftSlot.className = 'taskbar-left';
        var centerSlot = document.createElement('div');
        centerSlot.className = 'taskbar-center';
        var appsSlot = document.createElement('div');
        appsSlot.id = 'ktui-taskbar-apps';
        centerSlot.appendChild(appsSlot);
        var traySlot = document.createElement('div');
        traySlot.className = 'taskbar-tray';
        taskbar.appendChild(leftSlot);
        taskbar.appendChild(centerSlot);
        taskbar.appendChild(traySlot);
        desktop.appendChild(taskbar);
    }

    if (!Module['ktuiWindows']) Module['ktuiWindows'] = new Map();
    if (!Module['ktuiCanvases']) Module['ktuiCanvases'] = new Map();
    if (!Module['ktuiTaskbarItems']) Module['ktuiTaskbarItems'] = new Map();

    // 开始菜单与“显示桌面”交互绑定
    if (!window._ktuiTaskbarBound) {
        window._ktuiTaskbarBound = true;
        var startBtn = document.getElementById('ktui-start-btn');
        var startMenu = document.getElementById('ktui-start-menu');
        var closeStartMenu = function() {
            if (startMenu) startMenu.classList.remove('open');
            if (startBtn) startBtn.classList.remove('open');
        };
        if (startBtn && startMenu) {
            startBtn.addEventListener('click', function(e) {
                e.stopPropagation();
                var willOpen = !startMenu.classList.contains('open');
                startMenu.classList.toggle('open', willOpen);
                startBtn.classList.toggle('open', willOpen);
            });
            document.addEventListener('pointerdown', function(e) {
                if (startMenu.classList.contains('open') &&
                    !startMenu.contains(e.target) && !startBtn.contains(e.target)) {
                    closeStartMenu();
                }
            });
        }
        var showDeskBtn = document.getElementById('ktui-show-desktop');
        if (showDeskBtn) {
            showDeskBtn.addEventListener('click', function() {
                closeStartMenu();
                (window._ktuiDesktopActionCbs || []).forEach(function(fn) { try { fn(0); } catch (_) {} });
            });
        }
        var menuShowDesk = document.getElementById('ktui-menu-show-desktop');
        if (menuShowDesk) {
            menuShowDesk.addEventListener('click', function() {
                closeStartMenu();
                (window._ktuiDesktopActionCbs || []).forEach(function(fn) { try { fn(1); } catch (_) {} });
            });
        }
        var menuRestoreAll = document.getElementById('ktui-menu-restore-all');
        if (menuRestoreAll) {
            menuRestoreAll.addEventListener('click', function() {
                closeStartMenu();
                (window._ktuiDesktopActionCbs || []).forEach(function(fn) { try { fn(2); } catch (_) {} });
            });
        }
    }

    // 注册多实例 WASM 共享的桌面回调总线 (支持多个 WASM 应用同屏多开)
    window._ktuiViewportCbs = window._ktuiViewportCbs || [];
    window._ktuiViewportCbs.push(function() { _ktui_web_on_viewport_resize(); });
    window._ktuiDesktopActionCbs = window._ktuiDesktopActionCbs || [];
    window._ktuiDesktopActionCbs.push(function(mode) { _ktui_web_on_show_desktop(mode); });

    // 全局拖拽 / 缩放状态 (每次拖拽闭包绑定各自 WASM 实例的导出函数，彻底支持多开)
    if (!window._ktuiDesktopListenersInstalled) {
        window._ktuiDesktopListenersInstalled = true;
        window._ktuiDrag = null;
        window._ktuiGlobalZ = window._ktuiGlobalZ || 20;

        window.addEventListener('pointermove', function(e) {
            var drag = window._ktuiDrag;
            if (!drag) return;
            var dx = e.clientX - drag.startX;
            var dy = e.clientY - drag.startY;
            if (drag.mode === 'maximized-pending') {
                // 最大化态按住标题栏拖拽 > 6px：自动还原窗口化并按比例锚定到光标下方继续拖动
                if (Math.hypot(dx, dy) > 6) {
                    var rw = drag.restoredW || 960;
                    var nx0 = Math.round(e.clientX - rw * drag.ratioX);
                    var ny0 = Math.max(0, Math.round(e.clientY - 16));
                    if (drag.onRestoreForDrag) drag.onRestoreForDrag(drag.winId, nx0, ny0);
                    drag.mode = 'move';
                    drag.startX = e.clientX;
                    drag.startY = e.clientY;
                    drag.origX = nx0;
                    drag.origY = ny0;
                }
            } else if (drag.mode === 'move') {
                var nx = drag.origX + dx;
                var ny = Math.max(0, Math.min(window.innerHeight - 48 - 34, drag.origY + dy));
                if (drag.onMove) drag.onMove(drag.winId, nx, ny);
            } else if (drag.mode === 'resize') {
                var x = drag.origX, y = drag.origY, w = drag.origW, h = drag.origH;
                var ht = drag.ht;
                // BorderLeft=5, Right=6, Top=7, Bottom=8, TL=9, TR=10, BL=11, BR=12
                if (ht === 5 || ht === 9 || ht === 11) {
                    var nw = Math.max(drag.minW, Math.min(drag.maxW, drag.origW - dx));
                    x = drag.origX + (drag.origW - nw);
                    w = nw;
                }
                if (ht === 6 || ht === 10 || ht === 12) {
                    w = Math.max(drag.minW, Math.min(drag.maxW, drag.origW + dx));
                }
                if (ht === 7 || ht === 9 || ht === 10) {
                    var nh = Math.max(drag.minH, Math.min(drag.maxH, drag.origH - dy));
                    y = drag.origY + (drag.origH - nh);
                    h = nh;
                }
                if (ht === 8 || ht === 11 || ht === 12) {
                    h = Math.max(drag.minH, Math.min(drag.maxH, drag.origH + dy));
                }
                if (drag.onResize) drag.onResize(drag.winId, x, y, w, h);
            }
        });

        window.addEventListener('pointerup', function() {
            window._ktuiDrag = null;
        });

        window.addEventListener('resize', function() {
            var cbs = window._ktuiViewportCbs || [];
            for (var i = 0; i < cbs.length; ++i) {
                try { cbs[i](); } catch (_) {}
            }
        });

        // 监听浏览器缩放 / 跨屏移动导致的 devicePixelRatio 变化
        var watchDpr = function() {
            if (!window.matchMedia) return;
            var dpr = window.devicePixelRatio || 1;
            var mq = window.matchMedia('(resolution: ' + dpr + 'dppx)');
            var onDprChange = function() {
                var cbs = window._ktuiViewportCbs || [];
                for (var i = 0; i < cbs.length; ++i) {
                    try { cbs[i](); } catch (_) {}
                }
                watchDpr();
            };
            if (mq.addEventListener) mq.addEventListener('change', onDprChange, { once: true });
            else if (mq.addListener) mq.addListener(onDprChange);
        };
        watchDpr();
    }

    // 共享隐藏 IME 输入框
    if (!document.getElementById('ktui_ime_input')) {
        var input = document.createElement('input');
        input.id = 'ktui_ime_input';
        input.type = 'text';
        input.tabIndex = -1;
        input.disabled = true;
        input.autocomplete = 'off';
        input.spellcheck = false;
        input.style.cssText = 'position:fixed;opacity:0;pointer-events:none;width:1px;height:1px;z-index:999999;';
        document.body.appendChild(input);

        var composing = false;
        var pendingCommit = null;
        var send = function(text, cursor) {
            var size = lengthBytesUTF8(text) + 1;
            var buf = _ktui_web_get_text_buf(size);
            stringToUTF8(text, buf, size);
            if (cursor === undefined) _ktui_web_on_ime_commit(buf);
            else _ktui_web_on_ime_composition(buf, lengthBytesUTF8(text.slice(0, cursor)));
        };
        input.ktuiIsComposing = function() { return composing; };
        input.addEventListener('keydown', function(e) {
            if (!composing && !e.isComposing && e.keyCode !== 229) pendingCommit = null;
        });
        input.addEventListener('compositionstart', function() {
            composing = true;
            pendingCommit = null;
        });
        input.addEventListener('compositionupdate', function(e) {
            if (!composing || input.disabled) return;
            var text = e.data || '';
            var cursor = input.value === text ? input.selectionEnd : text.length;
            send(text, cursor);
        });
        input.addEventListener('compositionend', function(e) {
            if (!composing || input.disabled) return;
            composing = false;
            var text = e.data || '';
            pendingCommit = text || null;
            input.value = '';
            if (text) send(text);
            else send('', 0);
        });
        input.addEventListener('input', function(e) {
            if (input.disabled || composing || e.isComposing) return;
            var text = e.data ?? input.value;
            var inputType = e.inputType || '';
            var duplicate = pendingCommit !== null &&
                (inputType === 'insertFromComposition' ||
                    ((inputType === 'insertText' || inputType === 'insertCompositionText') &&
                        text === pendingCommit));
            pendingCommit = null;
            input.value = '';
            if (text && !duplicate && !inputType.startsWith('delete')) send(text);
        });
        input.addEventListener('blur', function() {
            if (composing) {
                composing = false;
                send('', 0);
            }
            pendingCommit = null;
            input.value = '';
        });
        input.addEventListener('paste', function(e) {
            if (e.clipboardData) window._ktui_clipboard = e.clipboardData.getData('text/plain');
        });
    }
});

EM_JS(void, ktui_js_create_window, (int win_id, const char* title, float x, float y, float w, float h,
                                    float min_w, float min_h, float max_w, float max_h, int z_index), {
    var desktop = document.getElementById('ktui-desktop');
    if (!desktop) return;

    var wrap = document.createElement('div');
    wrap.className = 'ktui-win';
    wrap.dataset.ktuiWinId = String(win_id);
    wrap.style.left = Math.round(x) + 'px';
    wrap.style.top = Math.round(y) + 'px';
    wrap.style.width = Math.round(w) + 'px';
    wrap.style.height = Math.round(h) + 'px';
    wrap.style.zIndex = String(z_index);
    wrap.style.boxShadow = '0 22px 58px rgba(0,0,0,0.62),0 2px 12px rgba(0,0,0,0.36)';
    wrap.style.opacity = '0';
    wrap.style.transform = 'translate3d(0, 10px, 0) scale(0.96)';

    var canvas = document.createElement('canvas');
    canvas.id = 'ktui-win-' + win_id;
    canvas.dataset.ktuiWinId = String(win_id);
    canvas.tabIndex = 0;
    canvas.style.cssText = 'display:block;outline:none;touch-action:none;-webkit-tap-highlight-color:transparent;';
    canvas.style.width = Math.round(w) + 'px';
    canvas.style.height = Math.round(h) + 'px';
    wrap.appendChild(canvas);
    desktop.appendChild(wrap);

    var entry = {
        wrap: wrap,
        canvas: canvas,
        imgData: null,
        minW: min_w,
        minH: min_h,
        maxW: max_w,
        maxH: max_h,
        maximized: false,
        minimized: false,
        visible: false,
        initialized: false,
        resizable: true,
        hideTimer: 0
    };
    Module['ktuiWindows'].set(win_id, entry);
    Module['ktuiCanvases'].set(win_id, canvas);

    var getMods = function(e) {
        return (e.shiftKey ? 1 : 0) | (e.ctrlKey ? 2 : 0) | (e.altKey ? 4 : 0) | (e.metaKey ? 8 : 0);
    };
    var getLocal = function(e) {
        var r = canvas.getBoundingClientRect();
        return [e.clientX - r.left, e.clientY - r.top];
    };

    canvas.addEventListener('pointerdown', function(e) {
        window._ktuiGlobalZ = (window._ktuiGlobalZ || 20) + 1;
        wrap.style.zIndex = String(window._ktuiGlobalZ);
        _ktui_web_on_focus_window(win_id);
        var pt = getLocal(e);
        var btn = e.button === 0 ? 1 : (e.button === 2 ? 2 : (e.button === 1 ? 3 : 0));
        var touchFlag = (e.pointerType === 'touch') ? 16 : 0;
        if (btn === 1) {
            var ht = _ktui_web_hit_test(win_id, pt[0], pt[1]);
            if (ht === 1) {
                var rect = wrap.getBoundingClientRect();
                if (!entry.maximized) {
                    window._ktuiDrag = {
                        mode: 'move',
                        winId: win_id,
                        startX: e.clientX,
                        startY: e.clientY,
                        origX: rect.left,
                        origY: rect.top,
                        onMove: _ktui_web_on_drag_move
                    };
                } else {
                    window._ktuiDrag = {
                        mode: 'maximized-pending',
                        winId: win_id,
                        startX: e.clientX,
                        startY: e.clientY,
                        ratioX: Math.max(0.08, Math.min(0.92, pt[0] / Math.max(1, rect.width))),
                        restoredW: entry.normalW || 960,
                        onRestoreForDrag: _ktui_web_on_restore_for_drag,
                        onMove: _ktui_web_on_drag_move
                    };
                }
                // 仍转发 PointerDown 以便支持标题栏双击最大化/还原
                _ktui_web_on_pointer(win_id, 1, pt[0], pt[1], btn, getMods(e) | touchFlag);
                return;
            }
            if (ht >= 5 && ht <= 12 && entry.resizable && !entry.maximized) {
                var rect2 = wrap.getBoundingClientRect();
                window._ktuiDrag = {
                    mode: 'resize',
                    winId: win_id,
                    ht: ht,
                    startX: e.clientX,
                    startY: e.clientY,
                    origX: rect2.left,
                    origY: rect2.top,
                    origW: rect2.width,
                    origH: rect2.height,
                    minW: entry.minW,
                    minH: entry.minH,
                    maxW: entry.maxW,
                    maxH: entry.maxH,
                    onResize: _ktui_web_on_drag_resize
                };
                e.preventDefault();
                return;
            }
        }
        try { canvas.setPointerCapture(e.pointerId); } catch (_) {}
        _ktui_web_on_pointer(win_id, 1, pt[0], pt[1], btn, getMods(e) | touchFlag);
    });

    canvas.addEventListener('pointermove', function(e) {
        if (window._ktuiDrag) return;
        var pt = getLocal(e);
        var touchFlag = (e.pointerType === 'touch') ? 16 : 0;
        _ktui_web_on_pointer(win_id, 0, pt[0], pt[1], 0, getMods(e) | touchFlag);
    });

    canvas.addEventListener('pointerup', function(e) {
        if (window._ktuiDrag) return;
        var pt = getLocal(e);
        var btn = e.button === 0 ? 1 : (e.button === 2 ? 2 : (e.button === 1 ? 3 : 0));
        var touchFlag = (e.pointerType === 'touch') ? 16 : 0;
        _ktui_web_on_pointer(win_id, 2, pt[0], pt[1], btn, getMods(e) | touchFlag);
    });

    canvas.addEventListener('wheel', function(e) {
        e.preventDefault();
        var pt = getLocal(e);
        _ktui_web_on_wheel(win_id, pt[0], pt[1], e.deltaX, e.deltaY, getMods(e));
    }, { passive: false });

    canvas.addEventListener('contextmenu', function(e) { e.preventDefault(); });

    // Windows 11 风格任务栏图标项
    var appsHost = document.getElementById('ktui-taskbar-apps') || document.getElementById('ktui-taskbar');
    if (appsHost) {
        var pill = document.createElement('button');
        pill.type = 'button';
        pill.className = 'ktui-tb-item';
        var titleStr = UTF8ToString(title) || ('Window ' + win_id);
        pill.title = titleStr;

        var icon = document.createElement('span');
        icon.className = 'ktui-tb-icon';
        icon.setAttribute('aria-hidden', 'true');
        icon.innerHTML = '<i></i><i></i><i></i>';

        var label = document.createElement('span');
        label.className = 'ktui-tb-label';
        label.textContent = titleStr;

        var indicator = document.createElement('span');
        indicator.className = 'ktui-tb-indicator';

        pill.appendChild(icon);
        pill.appendChild(label);
        pill.appendChild(indicator);
        pill.addEventListener('click', function() {
            _ktui_web_on_taskbar_click(win_id);
        });
        appsHost.appendChild(pill);
        Module['ktuiTaskbarItems'].set(win_id, pill);
    }
});

EM_JS(void, ktui_js_update_window_geometry, (int win_id, float x, float y, float w, float h, int z_index,
                                             int visible, int minimized, int maximized, int active, int resizable), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (!entry) return;
    var wrap = entry.wrap;
    var canvas = entry.canvas;
    var isVisible = visible !== 0;
    var isMinimized = minimized !== 0;
    var isMaximized = maximized !== 0;
    var isActive = active !== 0;
    var prevVisible = entry.visible;
    var prevMinimized = entry.minimized;
    var prevMaximized = entry.maximized;
    var isDragging = !!(window._ktuiDrag && window._ktuiDrag.winId === win_id);

    entry.maximized = isMaximized;
    entry.minimized = isMinimized;
    entry.visible = isVisible;
    entry.resizable = resizable !== 0;

    if (entry.hideTimer) {
        clearTimeout(entry.hideTimer);
        entry.hideTimer = 0;
    }

    var pill = Module['ktuiTaskbarItems'] && Module['ktuiTaskbarItems'].get(win_id);
    var computeTaskbarDelta = function(winLeft, winTop, winW, winH) {
        var cx = winLeft + winW * 0.5;
        var cy = winTop + winH * 0.5;
        var tx = window.innerWidth * 0.5;
        var ty = window.innerHeight - 24;
        if (pill) {
            var pr = pill.getBoundingClientRect();
            if (pr.width > 0 && pr.height > 0) {
                tx = pr.left + pr.width * 0.5;
                ty = pr.top + pr.height * 0.5;
            }
        }
        return [tx - cx, ty - cy];
    };

    var nx = Math.round(x);
    var ny = Math.round(y);
    var nw = Math.round(w);
    var nh = Math.round(h);
    if (!isMaximized && nw > 100 && nh > 100) {
        entry.normalW = nw;
        entry.normalH = nh;
    }

    var prevRect = (entry.initialized && prevVisible && isVisible && !isDragging && prevMaximized !== isMaximized)
        ? wrap.getBoundingClientRect()
        : null;

    wrap.style.zIndex = String(z_index);
    wrap.classList.toggle('maximized', isMaximized);
    wrap.classList.toggle('inactive', !isActive);
    wrap.style.boxShadow = isMaximized
        ? 'none'
        : (isActive
            ? '0 22px 58px rgba(0,0,0,0.64),0 2px 12px rgba(0,0,0,0.40)'
            : '0 10px 28px rgba(0,0,0,0.42)');

    // 1. 最小化动效：缩放并滑向对应任务栏图标
    if (entry.initialized && prevVisible && !isVisible && isMinimized) {
        var minDelta = computeTaskbarDelta(nx, ny, nw, nh);
        wrap.style.pointerEvents = 'none';
        wrap.style.transformOrigin = '50% 50%';
        wrap.style.transition = 'transform 210ms cubic-bezier(0.4, 0, 1, 1), opacity 180ms ease';
        wrap.style.transform = 'translate3d(' + Math.round(minDelta[0]) + 'px,' + Math.round(minDelta[1]) + 'px,0) scale(0.16)';
        wrap.style.opacity = '0';
        entry.hideTimer = setTimeout(function() {
            entry.hideTimer = 0;
            if (!entry.visible) {
                wrap.style.visibility = 'hidden';
            }
        }, 220);
    } else if (!isVisible) {
        // 隐藏或关闭
        wrap.style.transition = 'none';
        wrap.style.visibility = 'hidden';
        wrap.style.pointerEvents = 'none';
        wrap.style.opacity = '0';
    } else {
        // 可见状态：先更新目标几何与画布尺寸
        wrap.style.left = nx + 'px';
        wrap.style.top = ny + 'px';
        wrap.style.width = nw + 'px';
        wrap.style.height = nh + 'px';
        canvas.style.width = nw + 'px';
        canvas.style.height = nh + 'px';
        wrap.style.visibility = 'visible';
        wrap.style.pointerEvents = 'auto';

        if (!entry.initialized) {
            // 首次显示：Windows 11 弹出入场动效
            entry.initialized = true;
            wrap.style.transition = 'none';
            wrap.style.transformOrigin = '50% 50%';
            wrap.style.transform = 'translate3d(0, 10px, 0) scale(0.96)';
            wrap.style.opacity = '0';
            void wrap.offsetWidth;
            wrap.style.transition = 'transform 190ms cubic-bezier(0.1, 0.9, 0.2, 1), opacity 160ms ease';
            wrap.style.transform = 'translate3d(0, 0, 0) scale(1)';
            wrap.style.opacity = '1';
        } else if (!prevVisible && prevMinimized) {
            // 从任务栏还原动效：从任务栏图标位置弹性展开
            var resDelta = computeTaskbarDelta(nx, ny, nw, nh);
            wrap.style.transition = 'none';
            wrap.style.transformOrigin = '50% 50%';
            wrap.style.transform = 'translate3d(' + Math.round(resDelta[0]) + 'px,' + Math.round(resDelta[1]) + 'px,0) scale(0.16)';
            wrap.style.opacity = '0';
            void wrap.offsetWidth;
            wrap.style.transition = 'transform 240ms cubic-bezier(0.1, 0.9, 0.2, 1), opacity 190ms ease';
            wrap.style.transform = 'translate3d(0, 0, 0) scale(1)';
            wrap.style.opacity = '1';
        } else if (prevRect && prevRect.width > 0 && prevRect.height > 0 && nw > 0 && nh > 0) {
            // 最大化 / 还原 FLIP 丝滑动效（纯 GPU Transform，零闪烁）
            var dx = prevRect.left - nx;
            var dy = prevRect.top - ny;
            var sx = prevRect.width / nw;
            var sy = prevRect.height / nh;
            wrap.style.transition = 'none';
            wrap.style.transformOrigin = '0 0';
            wrap.style.transform = 'translate3d(' + dx.toFixed(1) + 'px,' + dy.toFixed(1) + 'px,0) scale(' + sx.toFixed(4) + ',' + sy.toFixed(4) + ')';
            void wrap.offsetWidth;
            wrap.style.transition = 'transform 220ms cubic-bezier(0.1, 0.9, 0.2, 1)';
            wrap.style.transform = 'translate3d(0, 0, 0) scale(1, 1)';
            wrap.style.opacity = '1';
        } else {
            // 普通拖拽 / 缩放 / 焦点切换：零延迟
            wrap.style.transition = 'none';
            wrap.style.transform = 'none';
            wrap.style.opacity = '1';
        }
    }

    if (pill) {
        pill.classList.toggle('active', isActive && isVisible);
        pill.classList.toggle('minimized', isMinimized || !isVisible);
    }
});

EM_JS(void, ktui_js_update_window_constraints, (int win_id, float min_w, float min_h, float max_w, float max_h), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (!entry) return;
    entry.minW = min_w;
    entry.minH = min_h;
    entry.maxW = max_w;
    entry.maxH = max_h;
});

EM_JS(void, ktui_js_set_window_title, (int win_id, const char* title), {
    var str = UTF8ToString(title);
    var pill = Module['ktuiTaskbarItems'] && Module['ktuiTaskbarItems'].get(win_id);
    if (pill) {
        var displayTitle = str || ('Window ' + win_id);
        var label = pill.querySelector('.ktui-tb-label');
        if (label) label.textContent = displayTitle;
        else pill.textContent = displayTitle;
        pill.title = displayTitle;
    }
    if (str) document.title = str;
});

EM_JS(void, ktui_js_focus_window_canvas, (int win_id), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (entry && entry.canvas) {
        var ime = document.getElementById('ktui_ime_input');
        if (!ime || document.activeElement !== ime || ime.disabled) {
            entry.canvas.focus({ preventScroll: true });
        }
    }
});

EM_JS(void, ktui_js_destroy_window, (int win_id), {
    if (Module['ktuiWindows']) {
        var entry = Module['ktuiWindows'].get(win_id);
        if (entry && entry.wrap) entry.wrap.remove();
        Module['ktuiWindows'].delete(win_id);
    }
    if (Module['ktuiCanvases']) Module['ktuiCanvases'].delete(win_id);
    if (Module['ktuiTaskbarItems']) {
        var pill = Module['ktuiTaskbarItems'].get(win_id);
        if (pill) pill.remove();
        Module['ktuiTaskbarItems'].delete(win_id);
    }
    if (typeof window._ktuiNotifyDesktop === 'function') {
        window._ktuiNotifyDesktop();
    }
});

EM_JS(int, ktui_js_key_target, (), {
    var target = document.activeElement;
    if (!target) return 1;
    if (target.dataset && target.dataset.ktuiWinId) return 1;
    if (target.id === 'ktui_ime_input') {
        if (target.disabled || target.ktuiIsComposing()) return 0;
        return 2;
    }
    return 1;
});

EM_JS(void, ktui_js_present_rgba8_region, (int win_id, const uint8_t* rgba, int width, int height,
                                           int dirty_x, int dirty_y, int dirty_w, int dirty_h), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (!entry || !entry.canvas) return;
    var canvas = entry.canvas;
    var resized = (canvas.width !== width || canvas.height !== height);
    if (resized) {
        canvas.width = width;
        canvas.height = height;
    }
    var ctx = canvas.getContext('2d');
    if (!ctx) return;

    var recreated = !entry.imgData || entry.imgData.width !== width || entry.imgData.height !== height;
    if (recreated) {
        entry.imgData = ctx.createImageData(width, height);
    }
    var data = entry.imgData.data;

    if (resized || recreated || (dirty_x === 0 && dirty_y === 0 && dirty_w >= width && dirty_h >= height)) {
        var src = HEAPU8.subarray(rgba, rgba + width * height * 4);
        data.set(src);
        ctx.putImageData(entry.imgData, 0, 0);
        return;
    }

    var x0 = Math.max(0, Math.min(dirty_x, width));
    var y0 = Math.max(0, Math.min(dirty_y, height));
    var x1 = Math.max(x0, Math.min(dirty_x + dirty_w, width));
    var y1 = Math.max(y0, Math.min(dirty_y + dirty_h, height));
    var rw = x1 - x0;
    var rh = y1 - y0;
    if (rw <= 0 || rh <= 0) return;

    var rowBytes = width * 4;
    var copyBytes = rw * 4;
    for (var y = y0; y < y1; ++y) {
        var rowOffset = y * rowBytes + x0 * 4;
        data.set(HEAPU8.subarray(rgba + rowOffset, rgba + rowOffset + copyBytes), rowOffset);
    }
    ctx.putImageData(entry.imgData, 0, 0, x0, y0, rw, rh);
});

EM_JS(void, ktui_js_set_cursor, (int win_id, const char* cursor_style), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (entry && entry.canvas) {
        entry.canvas.style.cursor = UTF8ToString(cursor_style);
    }
});

EM_JS(void, ktui_js_set_ime_enabled, (int win_id, bool enabled), {
    var input = document.getElementById('ktui_ime_input');
    if (!input) return;
    if (enabled) {
        input.disabled = false;
        if (document.activeElement !== input) input.focus({preventScroll: true});
    } else if (!input.disabled) {
        var wasFocused = document.activeElement === input;
        input.disabled = true;
        input.value = '';
        if (wasFocused) {
            var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
            if (entry && entry.canvas) entry.canvas.focus({preventScroll: true});
        }
    }
});

EM_JS(void, ktui_js_set_ime_cursor_rect, (int win_id, float x, float y, float w, float h), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    var input = document.getElementById('ktui_ime_input');
    if (!entry || !entry.canvas || !input) return;
    var rect = entry.canvas.getBoundingClientRect();
    input.style.left = (rect.left + x) + 'px';
    input.style.top = (rect.top + y + h) + 'px';
});

EM_JS(void, ktui_js_set_clipboard, (const char* text), {
    var str = UTF8ToString(text);
    window._ktui_clipboard = str;
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(str).catch(function(){});
    }
});

EM_JS(void, ktui_js_get_clipboard, (char* out_buf, int max_len), {
    var str = window._ktui_clipboard || "";
    stringToUTF8(str, out_buf, max_len);
});

EM_JS(float, ktui_js_get_dpi_scale, (), {
    return Math.max(1, window.devicePixelRatio || 1);
});

EM_JS(void, ktui_js_get_window_size, (int* out_w, int* out_h), {
    HEAP32[out_w >> 2] = window.innerWidth;
    HEAP32[out_h >> 2] = window.innerHeight;
});

EM_JS(void, ktui_js_wake_frame, (), {
    if (window._ktuiRequestFrame) window._ktuiRequestFrame();
});

// ==================== Web 平台任务调度器 ====================
class WebDispatcher : public IPlatformDispatcher {
public:
    void post_task(std::function<void()> task) override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(std::move(task));
        }
        ktui_js_wake_frame();
    }

    size_t process_tasks() override {
        std::vector<std::function<void()>> to_run;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            to_run.swap(m_queue);
        }
        for (auto& t : to_run) {
            if (t) t();
        }
        return to_run.size();
    }

    bool has_pending_tasks() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_queue.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::function<void()>> m_queue;
};

// ==================== Web 平台虚拟帧时钟源 ====================
class WebVSyncSource : public IVSyncSource {
public:
    uint64_t request_frame(std::function<void(double timestamp_seconds)> callback) override {
        uint64_t id = m_next_id++;
        m_requests.push_back({id, std::move(callback)});
        ktui_js_wake_frame();
        return id;
    }

    void cancel_frame(uint64_t request_id) override {
        std::erase_if(m_requests, [request_id](const Request& r) { return r.id == request_id; });
    }

    bool is_sleeping() const override {
        return m_requests.empty();
    }

    void trigger_frame(double timestamp_seconds) {
        auto reqs = std::move(m_requests);
        m_requests.clear();
        for (const auto& r : reqs) {
            if (r.callback) r.callback(timestamp_seconds);
        }
    }

private:
    struct Request {
        uint64_t id = 0;
        std::function<void(double)> callback;
    };
    std::vector<Request> m_requests;
    uint64_t m_next_id = 1;
};

// ==================== Web 字体回退解析器 ====================
class WebFontResolver : public IFontResolver {
public:
    std::string resolve_font(FontFamilyCategory category, FontWeight weight = FontWeight::Regular) override {
        (void)category; (void)weight;
        return "sans-serif";
    }

    std::string resolve_font_by_name(const std::string& family_name) override {
        return family_name.empty() ? "sans-serif" : family_name;
    }
};

// ==================== Web 虚拟桌面多窗口实现 ====================
class WebWindow : public IPlatformWindow {
public:
    static constexpr float kTaskbarHeight = 48.0f;

    WebWindow(int win_id, const WindowDesc& desc, int z_index,
              std::function<void(WebWindow*)> on_activate_req,
              std::function<void(WebWindow*)> on_destroyed)
        : m_win_id(win_id),
          m_title(desc.title),
          m_size(desc.size),
          m_min_size(desc.min_size),
          m_max_size(desc.max_size),
          m_normal_size(desc.size),
          m_z_index(z_index),
          m_resizable(desc.resizable),
          m_decorated(desc.decorated),
          m_always_on_top(desc.always_on_top),
          m_on_activate_req(std::move(on_activate_req)),
          m_on_destroyed(std::move(on_destroyed))
    {
        m_last_dpi_scale = ktui_js_get_dpi_scale();
        Rect desk = work_area();
        // 适配移动端/小屏视口：若初始窗口大于可用桌面区域，自动收缩以完整显示
        const float max_init_w = std::max(280.0f, desk.width() - 16.0f);
        const float max_init_h = std::max(220.0f, desk.height() - 16.0f);
        if (m_size.x > max_init_w) m_size.x = max_init_w;
        if (m_size.y > max_init_h) m_size.y = max_init_h;
        m_normal_size = m_size;

        if (desc.position.x >= 0.0f && desc.position.y >= 0.0f) {
            m_position = desc.position;
        } else {
            // 首窗居中，后续窗口依次级联偏移，贴近真实桌面体验
            float base_x = std::max(8.0f, (desk.width() - m_size.x) * 0.5f);
            float base_y = std::max(8.0f, (desk.height() - m_size.y) * 0.5f);
            float cascade = static_cast<float>((win_id - 1) % 8) * 28.0f;
            m_position.x = std::clamp(base_x + cascade, 8.0f, std::max(8.0f, desk.width() - m_size.x - 8.0f));
            m_position.y = std::clamp(base_y + cascade, 8.0f, std::max(8.0f, desk.height() - m_size.y - 8.0f));
        }
        m_normal_pos = m_position;

        ktui_js_create_window(m_win_id, m_title.c_str(),
                              m_position.x, m_position.y, m_size.x, m_size.y,
                              m_min_size.x, m_min_size.y, m_max_size.x, m_max_size.y,
                              m_z_index);
        sync_dom_geometry();
    }

    ~WebWindow() override {
        close();
        if (m_on_destroyed) m_on_destroyed(this);
        ktui_js_destroy_window(m_win_id);
    }

    [[nodiscard]] int win_id() const noexcept { return m_win_id; }

    // ==================== 几何与尺寸 ====================
    Vec2 size() const override { return m_size; }
    void set_size(Vec2 new_size) override {
        m_size.x = std::clamp(new_size.x, m_min_size.x, m_max_size.x);
        m_size.y = std::clamp(new_size.y, m_min_size.y, m_max_size.y);
        if (m_state == WindowState::Normal) m_normal_size = m_size;
        sync_dom_geometry();
        if (m_resize_handler) m_resize_handler(m_size);
    }

    Vec2 min_size() const override { return m_min_size; }
    void set_min_size(Vec2 min_sz) override {
        m_min_size = min_sz;
        ktui_js_update_window_constraints(m_win_id, m_min_size.x, m_min_size.y, m_max_size.x, m_max_size.y);
    }
    Vec2 max_size() const override { return m_max_size; }
    void set_max_size(Vec2 max_sz) override {
        m_max_size = max_sz;
        ktui_js_update_window_constraints(m_win_id, m_min_size.x, m_min_size.y, m_max_size.x, m_max_size.y);
    }

    Vec2 position() const override { return m_position; }
    void set_position(Vec2 pos) override {
        m_position = pos;
        if (m_state == WindowState::Normal) m_normal_pos = m_position;
        sync_dom_geometry();
        if (m_move_handler) m_move_handler(m_position);
    }

    void center_on_screen() override {
        Rect desk = work_area();
        set_position(Vec2{
            std::max(0.0f, (desk.width() - m_size.x) * 0.5f),
            std::max(0.0f, (desk.height() - m_size.y) * 0.5f)
        });
    }

    Rect screen_bounds() const override {
        int w = 0, h = 0;
        ktui_js_get_window_size(&w, &h);
        return Rect{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)};
    }

    Rect monitor_uv_rect() const override {
        Rect sb = screen_bounds();
        float sw = std::max(1.0f, sb.width());
        float sh = std::max(1.0f, sb.height());
        float u0 = std::clamp(m_position.x / sw, 0.0f, 1.0f);
        float v0 = std::clamp(m_position.y / sh, 0.0f, 1.0f);
        float u1 = std::clamp((m_position.x + m_size.x) / sw, 0.0f, 1.0f);
        float v1 = std::clamp((m_position.y + m_size.y) / sh, 0.0f, 1.0f);
        return Rect{u0, v0, u1, v1};
    }

    // ==================== 呈现状态 ====================
    void show() override {
        m_visible = true;
        if (m_state == WindowState::Minimized) m_state = WindowState::Normal;
        activate();
        sync_dom_geometry();
    }
    void hide() override {
        m_visible = false;
        sync_dom_geometry();
    }
    bool is_visible() const override { return m_visible && m_state != WindowState::Minimized; }

    void minimize() override {
        if (m_state == WindowState::Minimized) return;
        m_was_maximized_before_min = (m_state == WindowState::Maximized);
        m_state = WindowState::Minimized;
        m_active = false;
        m_focused = false;
        sync_dom_geometry();
        if (m_focus_handler) m_focus_handler(false);
    }

    void maximize() override {
        if (!m_resizable) return;
        if (m_state == WindowState::Normal) {
            m_normal_pos = m_position;
            m_normal_size = m_size;
        }
        m_was_maximized_before_min = false;
        m_state = WindowState::Maximized;
        Rect desk = work_area();
        m_position = Vec2{0.0f, 0.0f};
        m_size = Vec2{std::max(m_min_size.x, desk.width()), std::max(m_min_size.y, desk.height())};
        sync_dom_geometry();
        if (m_move_handler) m_move_handler(m_position);
        if (m_resize_handler) m_resize_handler(m_size);
    }

    void restore() override {
        const bool from_minimized = (m_state == WindowState::Minimized);
        m_visible = true;
        if (from_minimized && m_was_maximized_before_min) {
            m_was_maximized_before_min = false;
            m_state = WindowState::Maximized;
            Rect desk = work_area();
            m_position = Vec2{0.0f, 0.0f};
            m_size = Vec2{std::max(m_min_size.x, desk.width()), std::max(m_min_size.y, desk.height())};
        } else {
            m_was_maximized_before_min = false;
            m_state = WindowState::Normal;
            m_position = m_normal_pos;
            m_size = m_normal_size;
        }
        sync_dom_geometry();
        if (m_move_handler) m_move_handler(m_position);
        if (m_resize_handler) m_resize_handler(m_size);
    }

    bool is_minimized() const override { return m_state == WindowState::Minimized; }
    bool is_maximized() const override { return m_state == WindowState::Maximized; }

    void set_fullscreen(bool fullscreen) override {
        if (fullscreen) {
            if (m_state == WindowState::Normal) {
                m_normal_pos = m_position;
                m_normal_size = m_size;
            }
            m_state = WindowState::Fullscreen;
            Rect sb = screen_bounds();
            m_position = Vec2{0.0f, 0.0f};
            m_size = Vec2{sb.width(), sb.height()};
            sync_dom_geometry();
            if (m_resize_handler) m_resize_handler(m_size);
        } else {
            restore();
        }
    }
    bool is_fullscreen() const override { return m_state == WindowState::Fullscreen; }

    void activate() override {
        if (m_on_activate_req) {
            m_on_activate_req(this);
        } else {
            set_active_internal(true, m_z_index);
        }
    }
    bool is_active() const override { return m_active; }
    void focus() override { activate(); }
    bool has_focus() const override { return m_focused; }

    void set_always_on_top(bool always_on_top) override {
        m_always_on_top = always_on_top;
        sync_dom_geometry();
    }
    bool is_always_on_top() const override { return m_always_on_top; }
    void flash_taskbar(bool flash) override { (void)flash; }

    // ==================== 外观与标题栏 ====================
    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override {
        m_title = title;
        ktui_js_set_window_title(m_win_id, m_title.c_str());
    }
    void set_icon(const std::string& icon_path) override { (void)icon_path; }

    void set_resizable(bool resizable) override {
        m_resizable = resizable;
        sync_dom_geometry();
    }
    bool is_resizable() const override { return m_resizable; }

    void set_opacity(float opacity) override { m_opacity = opacity; }
    float opacity() const override { return m_opacity; }

    void set_decorated(bool decorated) override { m_decorated = decorated; }
    bool is_decorated() const override { return m_decorated; }
    void set_titlebar_hit_test_callback(std::function<TitlebarHitTest(PointF)> cb) override {
        m_hit_test_cb = std::move(cb);
    }

    // ==================== 显示器与 DPI ====================
    float dpi_scale() const override { return m_last_dpi_scale; }
    float render_scale() const override { return m_last_dpi_scale; }
    uint32_t refresh_rate() const override { return 60; }

    // ==================== 原生句柄 ====================
    void* get_compute_display() const override { return nullptr; }
    void* get_native_handle() const override {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(m_win_id));
    }

    // ==================== 帧呈现与重绘请求 ====================
    void present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) override {
        if (!rgba || width == 0 || height == 0) return;
        ktui_js_present_rgba8_region(m_win_id, rgba, static_cast<int>(width), static_cast<int>(height),
                                     0, 0, static_cast<int>(width), static_cast<int>(height));
    }

    void present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                              uint32_t dirty_x, uint32_t dirty_y,
                              uint32_t dirty_w, uint32_t dirty_h) override {
        if (!rgba || width == 0 || height == 0 || dirty_w == 0 || dirty_h == 0) return;
        ktui_js_present_rgba8_region(m_win_id, rgba, static_cast<int>(width), static_cast<int>(height),
                                     static_cast<int>(dirty_x), static_cast<int>(dirty_y),
                                     static_cast<int>(dirty_w), static_cast<int>(dirty_h));
    }

    void invalidate() override {
        ktui_js_wake_frame();
    }
    void on_paint_requested(std::function<void()> handler) override {
        m_paint_handler = std::move(handler);
    }

    // ==================== IME 输入法与光标形状 ====================
    void set_ime_enabled(bool enabled) override {
        ktui_js_set_ime_enabled(m_win_id, enabled);
    }
    void set_ime_cursor_rect(Rect cursor_rect) override {
        ktui_js_set_ime_cursor_rect(m_win_id, cursor_rect.min_x, cursor_rect.min_y,
                                    cursor_rect.width(), cursor_rect.height());
    }
    void set_cursor(CursorShape cursor) override {
        if (m_cursor == cursor) return;
        m_cursor = cursor;
        const char* css_cursor = "default";
        switch (cursor) {
            case CursorShape::Arrow:        css_cursor = "default"; break;
            case CursorShape::IBeam:        css_cursor = "text"; break;
            case CursorShape::PointingHand: css_cursor = "pointer"; break;
            case CursorShape::ResizeEW:     css_cursor = "ew-resize"; break;
            case CursorShape::ResizeNS:     css_cursor = "ns-resize"; break;
            case CursorShape::ResizeNWSE:   css_cursor = "nwse-resize"; break;
            case CursorShape::ResizeNESW:   css_cursor = "nesw-resize"; break;
            case CursorShape::Crosshair:    css_cursor = "crosshair"; break;
            case CursorShape::Forbidden:    css_cursor = "not-allowed"; break;
            default:                        css_cursor = "default"; break;
        }
        ktui_js_set_cursor(m_win_id, css_cursor);
    }

    // ==================== 回调与销毁 ====================
    void on_close_requested(std::function<bool()> handler) override { m_close_handler = std::move(handler); }
    void on_resized(std::function<void(Vec2)> handler) override { m_resize_handler = std::move(handler); }
    void on_moved(std::function<void(Vec2)> handler) override { m_move_handler = std::move(handler); }
    void on_dpi_changed(std::function<void(float)> handler) override { m_dpi_handler = std::move(handler); }
    void on_focus_changed(std::function<void(bool)> handler) override { m_focus_handler = std::move(handler); }

    void close() override {
        if (m_is_closed) return;
        if (m_close_handler && !m_close_handler()) return;
        m_is_closed = true;
        m_visible = false;
        sync_dom_geometry();
    }

    // ==================== 内部虚拟桌面调度辅助 ====================
    TitlebarHitTest perform_hit_test(PointF local_pos) const {
        if (m_hit_test_cb) return m_hit_test_cb(local_pos);
        return TitlebarHitTest::Client;
    }

    void set_active_internal(bool active, int new_z_index) {
        m_z_index = new_z_index;
        bool changed = (m_active != active || m_focused != active);
        m_active = active;
        m_focused = active;
        sync_dom_geometry();
        if (active) {
            ktui_js_focus_window_canvas(m_win_id);
        }
        if (changed && m_focus_handler) {
            m_focus_handler(active);
        }
    }

    void apply_drag_resize(float x, float y, float w, float h) {
        m_position = Vec2{x, y};
        m_size = Vec2{std::clamp(w, m_min_size.x, m_max_size.x),
                      std::clamp(h, m_min_size.y, m_max_size.y)};
        if (m_state == WindowState::Normal) {
            m_normal_pos = m_position;
            m_normal_size = m_size;
        }
        sync_dom_geometry();
        if (m_move_handler) m_move_handler(m_position);
        if (m_resize_handler) m_resize_handler(m_size);
    }

    void handle_viewport_resized() {
        const float cur_dpi = ktui_js_get_dpi_scale();
        if (cur_dpi != m_last_dpi_scale) {
            m_last_dpi_scale = cur_dpi;
            if (m_dpi_handler) m_dpi_handler(cur_dpi);
            ktui_js_wake_frame();
        }
        if (m_state == WindowState::Maximized) {
            Rect desk = work_area();
            m_position = Vec2{0.0f, 0.0f};
            m_size = Vec2{std::max(m_min_size.x, desk.width()), std::max(m_min_size.y, desk.height())};
            sync_dom_geometry();
            if (m_resize_handler) m_resize_handler(m_size);
        } else if (m_state == WindowState::Fullscreen) {
            Rect sb = screen_bounds();
            m_position = Vec2{0.0f, 0.0f};
            m_size = Vec2{sb.width(), sb.height()};
            sync_dom_geometry();
            if (m_resize_handler) m_resize_handler(m_size);
        }
    }

private:
    Rect work_area() const {
        Rect sb = screen_bounds();
        return Rect{0.0f, 0.0f, std::max(320.0f, sb.width()), std::max(240.0f, sb.height() - kTaskbarHeight)};
    }

    void sync_dom_geometry() {
        int effective_z = m_always_on_top ? (m_z_index + 100000) : m_z_index;
        int min_flag = (m_state == WindowState::Minimized) ? 1 : 0;
        int vis = (m_visible && !m_is_closed && m_state != WindowState::Minimized) ? 1 : 0;
        int max_flag = (m_state == WindowState::Maximized || m_state == WindowState::Fullscreen) ? 1 : 0;
        ktui_js_update_window_geometry(m_win_id, m_position.x, m_position.y, m_size.x, m_size.y,
                                       effective_z, vis, min_flag, max_flag, m_active ? 1 : 0, m_resizable ? 1 : 0);
        if (vis) ktui_js_wake_frame();
    }

    int m_win_id{0};
    std::string m_title;
    Vec2 m_size{800.0f, 600.0f};
    Vec2 m_min_size{100.0f, 100.0f};
    Vec2 m_max_size{16384.0f, 16384.0f};
    Vec2 m_position{40.0f, 40.0f};
    Vec2 m_normal_pos{40.0f, 40.0f};
    Vec2 m_normal_size{800.0f, 600.0f};
    int m_z_index{10};
    bool m_visible{false};
    bool m_active{false};
    bool m_focused{false};
    bool m_resizable{true};
    bool m_decorated{false};
    bool m_always_on_top{false};
    bool m_is_closed{false};
    bool m_was_maximized_before_min{false};
    float m_opacity{1.0f};
    float m_last_dpi_scale{1.0f};
    WindowState m_state{WindowState::Normal};
    CursorShape m_cursor{CursorShape::Arrow};

    std::function<void(WebWindow*)> m_on_activate_req;
    std::function<void(WebWindow*)> m_on_destroyed;
    std::function<TitlebarHitTest(PointF)> m_hit_test_cb;
    std::function<void()> m_paint_handler;
    std::function<bool()> m_close_handler;
    std::function<void(Vec2)> m_resize_handler;
    std::function<void(Vec2)> m_move_handler;
    std::function<void(float)> m_dpi_handler;
    std::function<void(bool)> m_focus_handler;
};

// ==================== Web 平台抽象层统一后端 (多窗口虚拟桌面) ====================
class WebBackend : public IPlatformBackend {
public:
    WebBackend() {
        s_instance = this;
    }

    ~WebBackend() override {
        shutdown();
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    static WebBackend* instance() noexcept {
        return s_instance;
    }

    bool init() override {
        if (m_initialized) return true;

        ktui_js_init_desktop();

        // 注册全局 HTML5 键盘事件 (自动路由到当前激活窗口)
        emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &on_key_down_event);
        emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &on_key_up_event);

        m_initialized = true;
        return true;
    }

    void poll_events() override {
        m_dispatcher.process_tasks();
    }

    void wait_events(uint32_t timeout_ms = 0) override {
        (void)timeout_ms;
        poll_events();
    }

    std::unique_ptr<IPlatformWindow> create_window(const WindowDesc& desc) override {
        if (!m_initialized) init();
        int win_id = m_next_win_id++;
        int z = m_next_z_index++;
        auto win = std::make_unique<WebWindow>(
            win_id, desc, z,
            [this](WebWindow* w) { activate_window(w); },
            [this](WebWindow* w) { unregister_window(w); }
        );
        m_windows.push_back(win.get());
        activate_window(win.get());
        return win;
    }

    IPlatformDispatcher& get_dispatcher() override { return m_dispatcher; }
    IVSyncSource& get_vsync_source() override { return m_vsync_source; }
    IFontResolver& get_font_resolver() override { return m_font_resolver; }

    std::string get_clipboard_text() const override {
        char buf[1024] = {0};
        ktui_js_get_clipboard(buf, sizeof(buf));
        return std::string(buf);
    }

    void set_clipboard_text(const std::string& utf8_text) override {
        ktui_js_set_clipboard(utf8_text.c_str());
    }

    void set_event_callback(std::function<void(const PlatformEvent&)> callback) override {
        m_event_callback = std::move(callback);
    }

    void shutdown() override {
        m_initialized = false;
        m_windows.clear();
        m_focused_window = nullptr;
    }

    void dispatch_event(const PlatformEvent& pe) {
        if (m_event_callback) {
            m_event_callback(pe);
        }
        ktui_js_wake_frame();
    }

    WebWindow* find_window(int win_id) const {
        for (auto* w : m_windows) {
            if (w && w->win_id() == win_id) return w;
        }
        return nullptr;
    }

    void activate_window(WebWindow* target) {
        if (!target) return;
        for (auto* w : m_windows) {
            if (w && w != target && w->is_active()) {
                w->set_active_internal(false, m_next_z_index - 1);
            }
        }
        m_focused_window = target;
        target->set_active_internal(true, m_next_z_index++);
    }

    void handle_taskbar_click(int win_id) {
        WebWindow* w = find_window(win_id);
        if (!w) return;
        if (w->is_minimized() || !w->is_visible()) {
            w->restore();
            activate_window(w);
        } else if (m_focused_window == w) {
            w->minimize();
            m_focused_window = nullptr;
        } else {
            activate_window(w);
        }
    }

    void handle_show_desktop(int mode) {
        bool any_visible = false;
        for (auto* w : m_windows) {
            if (w && w->is_visible() && !w->is_minimized()) {
                any_visible = true;
                break;
            }
        }
        const bool should_minimize = (mode == 1) || (mode == 0 && any_visible);
        if (should_minimize) {
            for (auto* w : m_windows) {
                if (w && w->is_visible() && !w->is_minimized()) {
                    w->minimize();
                }
            }
            m_focused_window = nullptr;
        } else {
            WebWindow* last = nullptr;
            for (auto* w : m_windows) {
                if (w && w->is_minimized()) {
                    w->restore();
                    last = w;
                }
            }
            if (last) activate_window(last);
        }
    }

    void handle_viewport_resize() {
        for (auto* w : m_windows) {
            if (w) w->handle_viewport_resized();
        }
    }

    void handle_ime_composition(const char* text, size_t cursor) {
        if (!m_focused_window || !text) return;
        PlatformEvent pe{};
        pe.ime_cursor_byte = cursor;
        pe.type = PlatformEventType::ImeComposition;
        pe.window_handle = m_focused_window->get_native_handle();
        pe.text = text;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        dispatch_event(pe);
    }

    void handle_ime_commit(const char* text) {
        if (!m_focused_window || !text || text[0] == '\0') return;
        PlatformEvent pe{};
        pe.type = PlatformEventType::ImeCommit;
        pe.window_handle = m_focused_window->get_native_handle();
        pe.text = text;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        dispatch_event(pe);
    }

    void handle_pointer(int win_id, int type, float x, float y, int button, int mods) {
        WebWindow* w = find_window(win_id);
        if (!w) return;
        PlatformEvent pe{};
        pe.window_handle = w->get_native_handle();
        pe.position = Vec2{x, y};
        pe.modifiers.shift = (mods & 1) != 0;
        pe.modifiers.ctrl  = (mods & 2) != 0;
        pe.modifiers.alt   = (mods & 4) != 0;
        pe.modifiers.meta  = (mods & 8) != 0;
        pe.device_kind = (mods & 16) ? PointerDeviceKind::Touch : PointerDeviceKind::Mouse;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        if (button == 1) pe.button = PointerButton::Primary;
        else if (button == 2) pe.button = PointerButton::Secondary;
        else if (button == 3) pe.button = PointerButton::Auxiliary;
        if (type == 0) pe.type = PlatformEventType::PointerMove;
        else if (type == 1) pe.type = PlatformEventType::PointerDown;
        else if (type == 2) pe.type = PlatformEventType::PointerUp;
        dispatch_event(pe);
    }

    void handle_wheel(int win_id, float x, float y, float dx, float dy, int mods) {
        WebWindow* w = find_window(win_id);
        if (!w) return;
        PlatformEvent pe{};
        pe.type = PlatformEventType::PointerScroll;
        pe.window_handle = w->get_native_handle();
        pe.position = Vec2{x, y};
        pe.delta = Vec2{dx, dy};
        pe.modifiers.shift = (mods & 1) != 0;
        pe.modifiers.ctrl  = (mods & 2) != 0;
        pe.modifiers.alt   = (mods & 4) != 0;
        pe.modifiers.meta  = (mods & 8) != 0;
        pe.device_kind = (mods & 16) ? PointerDeviceKind::Touch : PointerDeviceKind::Mouse;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        dispatch_event(pe);
    }

private:
    void unregister_window(WebWindow* w) {
        std::erase(m_windows, w);
        if (m_focused_window == w) {
            m_focused_window = nullptr;
            for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
                if (*it && (*it)->is_visible()) {
                    activate_window(*it);
                    break;
                }
            }
        }
    }

    static EM_BOOL on_key_down_event(int event_type, const EmscriptenKeyboardEvent* e, void* user_data) {
        (void)event_type;
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_focused_window) return EM_FALSE;

        const int target = ktui_js_key_target();
        if (target == 0 || e->keyCode == 229) return EM_FALSE;

        KeyModifiers mods{};
        mods.shift = e->shiftKey != 0;
        mods.ctrl  = e->ctrlKey != 0 || e->metaKey != 0;
        mods.alt   = e->altKey != 0;
        mods.meta  = e->metaKey != 0;

        uint32_t vk = e->keyCode;
        if (target == 2 && (mods.ctrl || mods.meta) && !mods.alt && vk == 'V') return EM_FALSE;

        PlatformEvent pe_down{};
        pe_down.type = PlatformEventType::KeyDown;
        pe_down.window_handle = self->m_focused_window->get_native_handle();
        pe_down.key_code = vk;
        pe_down.modifiers = mods;
        pe_down.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        self->dispatch_event(pe_down);

        if (target == 2 && (mods.ctrl || mods.meta) && !mods.alt &&
            (vk == 'A' || vk == 'C' || vk == 'X' || vk == 'Y' || vk == 'Z')) return EM_TRUE;
        if (vk == 8 || vk == 9 || vk == 13 || vk == 46 || (vk >= 35 && vk <= 40)) {
            return EM_TRUE;
        }
        return EM_FALSE;
    }

    static EM_BOOL on_key_up_event(int event_type, const EmscriptenKeyboardEvent* e, void* user_data) {
        (void)event_type;
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_focused_window) return EM_FALSE;

        if (!ktui_js_key_target() || e->keyCode == 229) return EM_FALSE;

        PlatformEvent pe{};
        pe.type = PlatformEventType::KeyUp;
        pe.window_handle = self->m_focused_window->get_native_handle();
        pe.key_code = e->keyCode;
        pe.modifiers.shift = e->shiftKey != 0;
        pe.modifiers.ctrl  = e->ctrlKey != 0;
        pe.modifiers.alt   = e->altKey != 0;
        pe.modifiers.meta  = e->metaKey != 0;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());

        self->dispatch_event(pe);
        return EM_TRUE;
    }

    inline static WebBackend* s_instance = nullptr;

    bool m_initialized{false};
    int m_next_win_id{1};
    int m_next_z_index{10};
    WebDispatcher m_dispatcher;
    WebVSyncSource m_vsync_source;
    WebFontResolver m_font_resolver;
    std::vector<WebWindow*> m_windows;
    WebWindow* m_focused_window{nullptr};
    std::function<void(const PlatformEvent&)> m_event_callback;
};

// C 函数桥接实现
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_ime_composition(const char* text, size_t cursor) {
        if (WebBackend::instance()) WebBackend::instance()->handle_ime_composition(text, cursor);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_ime_commit(const char* text) {
        if (WebBackend::instance()) WebBackend::instance()->handle_ime_commit(text);
    }

    EMSCRIPTEN_KEEPALIVE
    inline int ktui_web_hit_test(int win_id, float x, float y) {
        if (WebBackend::instance()) {
            if (auto* w = WebBackend::instance()->find_window(win_id)) {
                return static_cast<int>(w->perform_hit_test(PointF{x, y}));
            }
        }
        return static_cast<int>(TitlebarHitTest::Client);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_focus_window(int win_id) {
        if (WebBackend::instance()) {
            if (auto* w = WebBackend::instance()->find_window(win_id)) {
                WebBackend::instance()->activate_window(w);
            }
        }
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_pointer(int win_id, int type, float x, float y, int button, int mods) {
        if (WebBackend::instance()) WebBackend::instance()->handle_pointer(win_id, type, x, y, button, mods);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_wheel(int win_id, float x, float y, float dx, float dy, int mods) {
        if (WebBackend::instance()) WebBackend::instance()->handle_wheel(win_id, x, y, dx, dy, mods);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_drag_move(int win_id, float x, float y) {
        if (WebBackend::instance()) {
            if (auto* w = WebBackend::instance()->find_window(win_id)) {
                w->set_position(Vec2{x, y});
            }
        }
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_drag_resize(int win_id, float x, float y, float w_px, float h_px) {
        if (WebBackend::instance()) {
            if (auto* w = WebBackend::instance()->find_window(win_id)) {
                w->apply_drag_resize(x, y, w_px, h_px);
            }
        }
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_restore_for_drag(int win_id, float x, float y) {
        if (WebBackend::instance()) {
            if (auto* w = WebBackend::instance()->find_window(win_id)) {
                w->restore();
                w->set_position(Vec2{x, y});
            }
        }
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_taskbar_click(int win_id) {
        if (WebBackend::instance()) WebBackend::instance()->handle_taskbar_click(win_id);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_show_desktop(int mode) {
        if (WebBackend::instance()) WebBackend::instance()->handle_show_desktop(mode);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_viewport_resize() {
        if (WebBackend::instance()) WebBackend::instance()->handle_viewport_resize();
    }
}

} // namespace ktui

#endif // __EMSCRIPTEN__
