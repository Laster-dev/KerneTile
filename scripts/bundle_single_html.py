#!/usr/bin/env python3
"""Bundle build-web/{ktui_gallery.html,ktui_gallery.js,ktui_gallery.wasm}
into a compact single-file ktui_gallery-web.html using gzip-9 + native browser
DecompressionStream('gzip') + direct wasmBinary (works offline & via file://)."""

import base64
import gzip
import pathlib
import sys


def gz_b64(data: bytes) -> str:
    return base64.b64encode(gzip.compress(data, compresslevel=9, mtime=0)).decode("ascii")


def replace_normalized(text: str, old: str, new: str) -> tuple[str, bool]:
    if old in text:
        return text.replace(old, new, 1), True
    old_crlf = old.replace("\n", "\r\n")
    if old_crlf in text:
        return text.replace(old_crlf, new, 1), True
    return text, False


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <build_web_dir> <output_html>")
        return 2

    src_dir = pathlib.Path(sys.argv[1])
    out_path = pathlib.Path(sys.argv[2])

    html = (src_dir / "ktui_gallery.html").read_text(encoding="utf-8")
    js_bytes = (src_dir / "ktui_gallery.js").read_bytes()
    wasm_bytes = (src_dir / "ktui_gallery.wasm").read_bytes()

    js_gz_b64 = gz_b64(js_bytes)
    wasm_gz_b64 = gz_b64(wasm_bytes)

    loader_block = f"""<script>
window.__KTUI_JS_GZ__ = '{js_gz_b64}';
window.__KTUI_WASM_GZ__ = '{wasm_gz_b64}';
window.__ktuiGunzip = async function(b64) {{
    var bin = atob(b64), len = bin.length, u8 = new Uint8Array(len);
    for (var i = 0; i < len; ++i) u8[i] = bin.charCodeAt(i);
    var ds = new DecompressionStream('gzip');
    var w = ds.writable.getWriter();
    w.write(u8); w.close();
    return new Response(ds.readable).arrayBuffer();
}};
</script>
    <script type="text/javascript">"""

    marker = '    <script type="text/javascript">'
    html, ok = replace_normalized(html, marker, loader_block)
    if not ok:
        raise RuntimeError("Could not find main <script> marker in ktui_gallery.html")

    old_loader = """        var s = document.createElement('script');
        s.src = withVer('ktui_gallery.js');
        s.async = true;
        s.onload = function() {
            if (typeof window.createKtuiApp !== 'function') {
                showError('WASM 入口 createKtuiApp 未找到');
                return;
            }
            window.createKtuiApp({
                locateFile: function(path, prefix) {
                    return withVer((prefix || '') + path);
                }
            }).then(function() {
                if (loadingEl) loadingEl.style.display = 'none';
            }).catch(function(err) {
                console.error(err);
                showError('启动失败: ' + (err && err.message ? err.message : err));
            });
        };
        s.onerror = function() {
            showError('加载 ktui_gallery.js 失败');
        };
        document.body.appendChild(s);"""

    new_loader = """        Promise.all([
            window.__ktuiGunzip(window.__KTUI_JS_GZ__),
            window.__ktuiGunzip(window.__KTUI_WASM_GZ__)
        ]).then(function(pair) {
            var code = new TextDecoder().decode(pair[0]);
            (0, eval)(code);
            if (typeof window.createKtuiApp !== 'function') {
                showError('WASM 入口 createKtuiApp 未找到');
                return;
            }
            return window.createKtuiApp({
                wasmBinary: new Uint8Array(pair[1]),
                locateFile: function(path, prefix) {
                    return withVer((prefix || '') + path);
                }
            }).then(function() {
                if (loadingEl) loadingEl.style.display = 'none';
            });
        }).catch(function(err) {
            console.error(err);
            showError('启动失败: ' + (err && err.message ? err.message : err));
        });"""

    html, ok = replace_normalized(html, old_loader, new_loader)
    if not ok:
        raise RuntimeError("Could not find script loader block in ktui_gallery.html")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(html, encoding="utf-8")
    print(f"Bundled compact single-file HTML: {out_path} ({out_path.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
