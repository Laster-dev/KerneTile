#!/usr/bin/env python3
"""Bundle build-web/{ktui_gallery.html,ktui_gallery.js,ktui_gallery.wasm,wallpaper_dark.jpg}
into a single self-contained ktui_gallery-web.html (works offline and via file://)."""

import base64
import pathlib
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <build_web_dir> <output_html>")
        return 2

    src_dir = pathlib.Path(sys.argv[1])
    out_path = pathlib.Path(sys.argv[2])

    html = (src_dir / "ktui_gallery.html").read_text(encoding="utf-8")
    js = (src_dir / "ktui_gallery.js").read_text(encoding="utf-8")
    wasm_b64 = base64.b64encode((src_dir / "ktui_gallery.wasm").read_bytes()).decode("ascii")

    wallpaper_path = src_dir / "wallpaper_dark.jpg"
    if wallpaper_path.exists():
        jpg_b64 = base64.b64encode(wallpaper_path.read_bytes()).decode("ascii")
        html = html.replace("url('wallpaper_dark.jpg')", f"url('data:image/jpeg;base64,{jpg_b64}')")

    # 1. Inject inline WASM data URI + inline ktui_gallery.js right before the main <script>
    #    Avoid premature </script> closing if ever present in JS string literals.
    safe_js = js.replace("</script>", "<\\/script>")
    inline_block = (
        f"<script>\n"
        f"window.__KTUI_INLINE_WASM__ = 'data:application/wasm;base64,{wasm_b64}';\n"
        f"{safe_js}\n"
        f"</script>\n"
        f"    <script type=\"text/javascript\">"
    )
    marker = '    <script type="text/javascript">'
    if marker not in html:
        raise RuntimeError("Could not find main <script> marker in ktui_gallery.html")
    html = html.replace(marker, inline_block, 1)

    # 2. Override locateFile in shell.html so .wasm resolves from the inlined base64 data URI
    old_locate = (
        "locateFile: function(path, prefix) {\n"
        "                        return withVer((prefix || '') + path);\n"
        "                    },"
    )
    new_locate = (
        "locateFile: function(path, prefix) {\n"
        "                        if (path.endsWith('.wasm') && window.__KTUI_INLINE_WASM__) return window.__KTUI_INLINE_WASM__;\n"
        "                        return withVer((prefix || '') + path);\n"
        "                    },"
    )
    if old_locate not in html:
        # Handle CRLF line endings if present
        old_locate_crlf = old_locate.replace("\n", "\r\n")
        if old_locate_crlf in html:
            html = html.replace(old_locate_crlf, new_locate, 1)
        else:
            raise RuntimeError("Could not find locateFile block in ktui_gallery.html")
    else:
        html = html.replace(old_locate, new_locate, 1)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(html, encoding="utf-8")
    print(f"Bundled single-file HTML: {out_path} ({out_path.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
