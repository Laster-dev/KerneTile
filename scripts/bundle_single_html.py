#!/usr/bin/env python3
"""Bundle build-web/{ktui_gallery.html,ktui_gallery.js,ktui_gallery.wasm,wallpaper_dark.jpg}
into a compact single-file ktui_gallery-web.html using gzip-9 + native browser
DecompressionStream('gzip') + direct wasmBinary (smaller than raw .wasm, works offline & file://)."""

import base64
import gzip
import pathlib
import sys


def gz_b64(data: bytes) -> str:
    return base64.b64encode(gzip.compress(data, compresslevel=9, mtime=0)).decode("ascii")


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

    # Remove external wallpaper_dark.jpg URL in offline single-file mode (keeps built-in CSS Bloom gradient)
    # so the single HTML stays ultra-compact without +184KB JPEG base64 bloat.
    html = html.replace("url('wallpaper_dark.jpg'),\r\n                ", "")
    html = html.replace("url('wallpaper_dark.jpg'),\n                ", "")
    html = html.replace("url('wallpaper_dark.jpg'),", "")

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
window.__ktuiWasmBinaryPromise = null;
window.__ktuiGetWasmBinary = function() {{
    if (!window.__ktuiWasmBinaryPromise) {{
        window.__ktuiWasmBinaryPromise = window.__ktuiGunzip(window.__KTUI_WASM_GZ__).then(function(buf) {{
            return new Uint8Array(buf);
        }});
    }}
    return window.__ktuiWasmBinaryPromise;
}};
</script>
    <script type="text/javascript">"""

    marker = '    <script type="text/javascript">'
    if marker not in html:
        raise RuntimeError("Could not find main <script> marker in ktui_gallery.html")
    html = html.replace(marker, loader_block, 1)

    # Replace ensureAppScript to decode gzipped inline ktui_gallery.js once
    old_ensure = """        function ensureAppScript(app) {
            if (typeof window[app.factoryName] === 'function') {
                return Promise.resolve(window[app.factoryName]);
            }
            if (loadedScripts[app.id]) return loadedScripts[app.id];
            loadedScripts[app.id] = new Promise(function(resolve, reject) {
                var s = document.createElement('script');
                s.src = withVer(app.jsUrl);
                s.async = true;
                s.onload = function() {
                    if (typeof window[app.factoryName] === 'function') {
                        resolve(window[app.factoryName]);
                    } else {
                        reject(new Error('WASM 工厂函数未找到: ' + app.factoryName));
                    }
                };
                s.onerror = function() {
                    delete loadedScripts[app.id];
                    reject(new Error('拉取 ' + app.jsUrl + ' 失败'));
                };
                document.body.appendChild(s);
            });
            return loadedScripts[app.id];
        }"""

    new_ensure = """        function ensureAppScript(app) {
            if (typeof window[app.factoryName] === 'function') {
                return Promise.resolve(window[app.factoryName]);
            }
            if (loadedScripts[app.id]) return loadedScripts[app.id];
            loadedScripts[app.id] = window.__ktuiGunzip(window.__KTUI_JS_GZ__).then(function(buf) {
                var code = new TextDecoder().decode(buf);
                (0, eval)(code);
                if (typeof window[app.factoryName] === 'function') {
                    return window[app.factoryName];
                }
                throw new Error('WASM 工厂函数未找到: ' + app.factoryName);
            });
            return loadedScripts[app.id];
        }"""

    if old_ensure in html:
        html = html.replace(old_ensure, new_ensure, 1)
    elif old_ensure.replace("\n", "\r\n") in html:
        html = html.replace(old_ensure.replace("\n", "\r\n"), new_ensure, 1)
    else:
        raise RuntimeError("Could not find ensureAppScript block in ktui_gallery.html")

    # Pass decoded wasmBinary directly into Emscripten factory (zero fetch, works on file://)
    old_launch = """            ensureAppScript(app).then(function(factory) {
                return factory({
                    locateFile: function(path, prefix) {
                        return withVer((prefix || '') + path);
                    },"""
    new_launch = """            Promise.all([ensureAppScript(app), window.__ktuiGetWasmBinary()]).then(function(pair) {
                var factory = pair[0], wasmBytes = pair[1];
                return factory({
                    wasmBinary: wasmBytes.slice(),
                    locateFile: function(path, prefix) {
                        return withVer((prefix || '') + path);
                    },"""

    if old_launch in html:
        html = html.replace(old_launch, new_launch, 1)
    elif old_launch.replace("\n", "\r\n") in html:
        html = html.replace(old_launch.replace("\n", "\r\n"), new_launch, 1)
    else:
        raise RuntimeError("Could not find launchWasmApp factory call in ktui_gallery.html")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(html, encoding="utf-8")
    print(f"Bundled compact single-file HTML: {out_path} ({out_path.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
