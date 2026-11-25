import ctypes
import json
import math
import struct
import subprocess
import time
import zlib
from pathlib import Path
import imageio_ffmpeg

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    user32.SetProcessDPIAware()

class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long), ("right", ctypes.c_long), ("bottom", ctypes.c_long)]

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", ctypes.c_uint32),
        ("biWidth", ctypes.c_int32),
        ("biHeight", ctypes.c_int32),
        ("biPlanes", ctypes.c_uint16),
        ("biBitCount", ctypes.c_uint16),
        ("biCompression", ctypes.c_uint32),
        ("biSizeImage", ctypes.c_uint32),
        ("biXPelsPerMeter", ctypes.c_int32),
        ("biYPelsPerMeter", ctypes.c_int32),
        ("biClrUsed", ctypes.c_uint32),
        ("biClrImportant", ctypes.c_uint32),
    ]

EXE = r"D:\KerneTile\build-clang\ktui_gallery.exe"
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()
OUT_DIR = Path(r"D:\KerneTile\video\public\clips")
PREVIEW_DIR = Path(r"D:\KerneTile\video_assets\previews")
OUT_DIR.mkdir(parents=True, exist_ok=True)
PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

FPS = 30


def write_png_bgra(path: Path, width: int, height: int, bgra: bytes):
    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        row = bgra[y * stride : (y + 1) * stride]
        rgb = bytearray(width * 3)
        rgb[0::3] = row[2::4]
        rgb[1::3] = row[1::4]
        rgb[2::3] = row[0::4]
        raw.extend(rgb)
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b"")
    path.write_bytes(png)


class GalleryAutomationRecorder:
    def __init__(self, proc: subprocess.Popen, hwnd: int, phys_w: int, phys_h: int, dpi: float):
        self.proc = proc
        self.hwnd = hwnd
        self.w = phys_w
        self.h = phys_h
        self.dpi = dpi
        self.cur_x = (phys_w / dpi) * 0.5
        self.cur_y = (phys_h / dpi) * 0.5
        self.click_ripple_frames = 0
        self.ff_proc = None
        self.frame_count = 0
        self.last_frame = None
        self.clip_durations = {}

        self.hwnd_dc = user32.GetWindowDC(hwnd)
        self.mem_dc = gdi32.CreateCompatibleDC(self.hwnd_dc)
        self.bmp = gdi32.CreateCompatibleBitmap(self.hwnd_dc, phys_w, phys_h)
        self.old_bmp = gdi32.SelectObject(self.mem_dc, self.bmp)
        self.bmi = BITMAPINFOHEADER()
        self.bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        self.bmi.biWidth = phys_w
        self.bmi.biHeight = -phys_h
        self.bmi.biPlanes = 1
        self.bmi.biBitCount = 32
        self.bmi.biCompression = 0
        self.buf = ctypes.create_string_buffer(phys_w * phys_h * 4)

    def send(self, cmd: str) -> str:
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()
        reply = self.proc.stdout.readline().strip()
        return reply

    def list_widgets(self, content_only: bool = True, overlay_only: bool = False):
        resp = self.send("LIST")
        if not resp.startswith("OK "):
            return []
        items = json.loads(resp[3:])
        if overlay_only:
            return [it for it in items if it.get("ov", 0) == 1]
        if content_only:
            # Skip left navigation pane (x < 245) and top titlebar (y < 40)
            return [it for it in items if it["x"] >= 245 and it["y"] >= 40 and it.get("ov", 0) == 0]
        return items

    def close_gdi(self):
        gdi32.SelectObject(self.mem_dc, self.old_bmp)
        gdi32.DeleteObject(self.bmp)
        gdi32.DeleteDC(self.mem_dc)
        user32.ReleaseDC(self.hwnd, self.hwnd_dc)

    def start_clip(self, clip_id: str, filename: str):
        self.current_clip_id = clip_id
        out_path = OUT_DIR / filename
        cmd = [
            FFMPEG, "-y",
            "-f", "rawvideo",
            "-vcodec", "rawvideo",
            "-pix_fmt", "bgra",
            "-s", f"{self.w}x{self.h}",
            "-r", str(FPS),
            "-i", "pipe:0",
            "-c:v", "libx264",
            "-preset", "fast",
            "-crf", "17",
            "-pix_fmt", "yuv420p",
            "-movflags", "+faststart",
            str(out_path),
        ]
        self.ff_proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        self.frame_count = 0
        self.last_frame = None
        print(f"Recording {filename} ...")

    def end_clip(self, preview_name: str | None = None):
        if self.ff_proc:
            self.ff_proc.stdin.close()
            _, err = self.ff_proc.communicate(timeout=30)
            if self.ff_proc.returncode != 0:
                print("ffmpeg err:", err.decode("utf-8", errors="ignore")[-400:])
            self.ff_proc = None
        dur = round(self.frame_count / FPS, 2)
        self.clip_durations[self.current_clip_id] = dur
        if preview_name and self.last_frame:
            write_png_bgra(PREVIEW_DIR / preview_name, self.w, self.h, self.last_frame)
        print(f"  -> recorded {self.frame_count} frames ({dur:.2f}s)")

    def _draw_cursor_overlay(self, data: bytearray):
        cx = int(round(self.cur_x * self.dpi))
        cy = int(round(self.cur_y * self.dpi))
        w, h = self.w, self.h
        if self.click_ripple_frames > 0:
            prog = 1.0 - (self.click_ripple_frames / 10.0)
            radius = int(10 + prog * 26)
            alpha = 0.78 * (1.0 - prog)
            r_min, r_max = max(0, radius - 2), radius + 2
            for dy in range(-r_max, r_max + 1):
                py = cy + dy
                if py < 0 or py >= h:
                    continue
                for dx in range(-r_max, r_max + 1):
                    px = cx + dx
                    if px < 0 or px >= w:
                        continue
                    dist = math.hypot(dx, dy)
                    if r_min <= dist <= r_max:
                        idx = (py * w + px) * 4
                        data[idx] = int(data[idx] * (1 - alpha) + 248 * alpha)
                        data[idx + 1] = int(data[idx + 1] * (1 - alpha) + 189 * alpha)
                        data[idx + 2] = int(data[idx + 2] * (1 - alpha) + 56 * alpha)
            self.click_ripple_frames -= 1

        for dy in range(-9, 10):
            py = cy + dy
            if py < 0 or py >= h:
                continue
            for dx in range(-9, 10):
                px = cx + dx
                if px < 0 or px >= w:
                    continue
                dist = math.hypot(dx, dy)
                if dist <= 8.5:
                    idx = (py * w + px) * 4
                    if dist <= 5.0:
                        data[idx] = 255
                        data[idx + 1] = 255
                        data[idx + 2] = 255
                    else:
                        a = 0.92
                        data[idx] = int(data[idx] * (1 - a) + 248 * a)
                        data[idx + 1] = int(data[idx + 1] * (1 - a) + 189 * a)
                        data[idx + 2] = int(data[idx + 2] * (1 - a) + 56 * a)

    def grab_frame(self):
        user32.PrintWindow(self.hwnd, self.mem_dc, 2)
        gdi32.GetDIBits(self.mem_dc, self.bmp, 0, self.h, self.buf, ctypes.byref(self.bmi), 0)
        if self.ff_proc and self.ff_proc.stdin:
            frame_bytes = bytearray(self.buf.raw)
            self._draw_cursor_overlay(frame_bytes)
            raw = bytes(frame_bytes)
            self.ff_proc.stdin.write(raw)
            self.last_frame = raw
            self.frame_count += 1

    def wait_frames(self, n: int):
        for _ in range(n):
            t0 = time.perf_counter()
            self.grab_frame()
            dt = time.perf_counter() - t0
            rem = (1.0 / FPS) - dt
            if rem > 0:
                time.sleep(rem)

    def move_to(self, x: float, y: float, frames: int = 14):
        sx, sy = self.cur_x, self.cur_y
        for i in range(1, frames + 1):
            t = i / frames
            e = t * t * (3 - 2 * t)
            nx = sx + (x - sx) * e
            ny = sy + (y - sy) * e
            self.cur_x, self.cur_y = nx, ny
            self.send(f"MOVE {nx:.1f} {ny:.1f}")
            t0 = time.perf_counter()
            self.grab_frame()
            dt = time.perf_counter() - t0
            rem = (1.0 / FPS) - dt
            if rem > 0:
                time.sleep(rem)

    def click(self, x: float, y: float, move_frames: int = 12, hold_frames: int = 4, after_frames: int = 16):
        self.move_to(x, y, frames=move_frames)
        self.click_ripple_frames = 10
        self.send(f"DOWN {x:.1f} {y:.1f}")
        self.wait_frames(hold_frames)
        self.send(f"UP {x:.1f} {y:.1f}")
        self.wait_frames(after_frames)

    def drag(self, x1: float, y1: float, x2: float, y2: float, move_frames: int = 12, drag_frames: int = 28, after_frames: int = 14):
        self.move_to(x1, y1, frames=move_frames)
        self.click_ripple_frames = 8
        self.send(f"DOWN {x1:.1f} {y1:.1f}")
        self.wait_frames(3)
        self.move_to(x2, y2, frames=drag_frames)
        self.send(f"UP {x2:.1f} {y2:.1f}")
        self.wait_frames(after_frames)

    def hold(self, x: float, y: float, hold_frames: int = 48, move_frames: int = 14, after_frames: int = 24):
        self.move_to(x, y, frames=move_frames)
        self.click_ripple_frames = 10
        self.send(f"DOWN {x:.1f} {y:.1f}")
        self.wait_frames(hold_frames)
        self.send(f"UP {x:.1f} {y:.1f}")
        self.wait_frames(after_frames)

    def type_text(self, text: str, per_char_frames: int = 3):
        for ch in text:
            self.send(f"TYPE {ch}")
            self.wait_frames(per_char_frames)

    def scroll(self, x: float, y: float, dy: float = -120.0, steps: int = 8):
        self.move_to(x, y, frames=10)
        for _ in range(steps):
            self.send(f"SCROLL {x:.1f} {y:.1f} {dy:.1f}")
            self.wait_frames(5)


def main():
    proc = subprocess.Popen(
        [EXE, "--automation"],
        cwd=r"D:\KerneTile\build-clang",
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
    )
    hwnd = 0
    for _ in range(60):
        hwnd = user32.FindWindowW(None, "KTUI Gallery")
        if hwnd:
            break
        time.sleep(0.1)
    if not hwnd:
        proc.terminate()
        raise SystemExit("KTUI Gallery window not found")

    user32.ShowWindow(hwnd, 9)
    user32.MoveWindow(hwnd, 80, 60, 1200, 760, True)
    time.sleep(0.8)

    # Query window info via automation protocol
    proc.stdin.write("INFO\n")
    proc.stdin.flush()
    info_line = proc.stdout.readline().strip()
    print("Automation INFO:", info_line)
    info = json.loads(info_line[3:])
    dpi = float(info.get("dpi", 1.0))

    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    phys_w, phys_h = rect.right - rect.left, rect.bottom - rect.top
    print(f"Physical window rect: {phys_w}x{phys_h}, dpi={dpi}")

    rec = GalleryAutomationRecorder(proc, hwnd, phys_w, phys_h, dpi)

    try:
        # 1. Home & Fluent 2 Ripple Theme Switch
        rec.send("NAV home")
        rec.start_clip("home", "clip_01_home.mp4")
        rec.wait_frames(18)
        home_widgets = rec.list_widgets()
        for w in home_widgets[:3]:
            rec.move_to(w["cx"], w["cy"], frames=14)
            rec.wait_frames(8)
        # Trigger real circular ripple theme switch (Dark -> Light -> Dark)
        rec.move_to(info["w"] - 140, 16, frames=14)
        rec.click_ripple_frames = 10
        rec.send("THEME light")
        rec.wait_frames(50)
        rec.click_ripple_frames = 10
        rec.send("THEME dark")
        rec.wait_frames(50)
        rec.end_clip("01_home.png")

        # 2. Button & HoldConfirmButton (exact RenderBox coordinates)
        rec.send("NAV button")
        rec.start_clip("button", "clip_02_button.mp4")
        rec.wait_frames(16)
        btns = [w for w in rec.list_widgets() if w["w"] >= 50 and w["h"] <= 48 and w["y"] >= 140]
        print(f"  Button page found {len(btns)} buttons")
        for b in btns[:4]:
            rec.click(b["cx"], b["cy"], move_frames=12, hold_frames=4, after_frames=14)
        # Switch to HoldConfirmButton
        rec.send("NAV pro_hold_confirm")
        rec.wait_frames(16)
        holds = [w for w in rec.list_widgets() if w["w"] >= 45 and w["y"] >= 150]
        print(f"  HoldConfirm page found {len(holds)} hold buttons")
        # Hold pill button (~1.6s = 48 frames) then circular ring button (~1.3s = 40 frames)
        pill_btns = [w for w in holds if w["w"] > 100]
        ring_btns = [w for w in holds if 45 <= w["w"] <= 90 and abs(w["w"] - w["h"]) <= 12]
        if pill_btns:
            rec.hold(pill_btns[0]["cx"], pill_btns[0]["cy"], hold_frames=52, after_frames=20)
        if ring_btns:
            rec.hold(ring_btns[1 if len(ring_btns) > 1 else 0]["cx"], ring_btns[1 if len(ring_btns) > 1 else 0]["cy"], hold_frames=42, after_frames=22)
        rec.end_clip("02_button.png")

        # 3. ToggleSwitch & CheckBox (exact RenderBox coordinates)
        rec.send("NAV toggle_switch")
        rec.start_clip("toggle", "clip_03_toggle.mp4")
        rec.wait_frames(16)
        switches = [w for w in rec.list_widgets() if w["y"] >= 140 and w["h"] <= 50]
        print(f"  ToggleSwitch page found {len(switches)} switches")
        for sw in switches[:3]:
            rec.click(sw["x"] + 24, sw["cy"], move_frames=12, after_frames=18)
        if switches:
            rec.click(switches[0]["x"] + 24, switches[0]["cy"], move_frames=12, after_frames=18)
        # CheckBox
        rec.send("NAV checkbox")
        rec.wait_frames(16)
        checks = [w for w in rec.list_widgets() if w["y"] >= 140 and w["h"] <= 46]
        print(f"  CheckBox page found {len(checks)} checkboxes")
        for cb in checks[:4]:
            rec.click(cb["x"] + 16, cb["cy"], move_frames=11, after_frames=15)
        rec.end_clip("03_toggle.png")

        # 4. Slider (exact horizontal & vertical slider bounds)
        rec.send("NAV slider")
        rec.start_clip("slider", "clip_04_slider.mp4")
        rec.wait_frames(18)
        sliders = [w for w in rec.list_widgets() if w["w"] >= 180 and w["h"] <= 46 and w["y"] >= 140]
        print(f"  Slider page found {len(sliders)} horizontal sliders")
        if len(sliders) >= 1:
            s0 = sliders[0]
            rec.drag(s0["x"] + s0["w"] * 0.45, s0["cy"], s0["x"] + s0["w"] * 0.85, s0["cy"], drag_frames=32, after_frames=14)
            rec.drag(s0["x"] + s0["w"] * 0.85, s0["cy"], s0["x"] + s0["w"] * 0.28, s0["cy"], move_frames=6, drag_frames=28, after_frames=14)
            rec.drag(s0["x"] + s0["w"] * 0.28, s0["cy"], s0["x"] + s0["w"] * 0.68, s0["cy"], move_frames=6, drag_frames=24, after_frames=14)
        if len(sliders) >= 2:
            s1 = sliders[1]
            rec.drag(s1["x"] + s1["w"] * 0.40, s1["cy"], s1["x"] + s1["w"] * 0.78, s1["cy"], drag_frames=28, after_frames=20)
        rec.end_clip("04_slider.png")

        # 5. ColorPicker (open popup SV spectrum + drag live color!)
        rec.send("NAV color_picker")
        rec.start_clip("color", "clip_05_color.mp4")
        rec.wait_frames(18)
        cp_items = [w for w in rec.list_widgets() if w["y"] >= 150]
        print(f"  ColorPicker page found {len(cp_items)} controls")
        if cp_items:
            # Click main color picker dropdown bar to open Gaussian-shadow popup!
            picker_btn = max(cp_items, key=lambda it: it["w"])
            rec.click(picker_btn["x"] + 60, picker_btn["cy"], move_frames=14, after_frames=22)
            # Now query overlay controls (SV box + Hue bar)
            ov_items = rec.list_widgets(overlay_only=True)
            print(f"  ColorPicker popup overlay found {len(ov_items)} controls")
            sv_box = next((w for w in ov_items if w["w"] >= 140 and w["h"] >= 120), None)
            hue_bar = next((w for w in ov_items if w["w"] >= 140 and 12 <= w["h"] <= 42), None)
            if sv_box:
                rec.drag(sv_box["x"] + sv_box["w"] * 0.25, sv_box["y"] + sv_box["h"] * 0.25,
                         sv_box["x"] + sv_box["w"] * 0.80, sv_box["y"] + sv_box["h"] * 0.35,
                         drag_frames=28, after_frames=12)
                rec.drag(sv_box["x"] + sv_box["w"] * 0.80, sv_box["y"] + sv_box["h"] * 0.35,
                         sv_box["x"] + sv_box["w"] * 0.65, sv_box["y"] + sv_box["h"] * 0.72,
                         move_frames=6, drag_frames=24, after_frames=12)
            if hue_bar:
                rec.drag(hue_bar["x"] + hue_bar["w"] * 0.15, hue_bar["cy"],
                         hue_bar["x"] + hue_bar["w"] * 0.78, hue_bar["cy"],
                         drag_frames=32, after_frames=20)
            else:
                # Fallback: drag inside popup area if hue bar is drawn inside sv_box
                rec.wait_frames(20)
        rec.end_clip("05_color.png")

        # 6. TextBox (exact text input focus + live typing)
        rec.send("NAV textbox")
        rec.start_clip("text", "clip_06_text.mp4")
        rec.wait_frames(18)
        inputs = [w for w in rec.list_widgets() if w["t"] == 1 and w["y"] >= 140]
        if not inputs:
            inputs = [w for w in rec.list_widgets() if w["f"] == 1 and w["w"] >= 160 and w["y"] >= 140]
        print(f"  TextBox page found {len(inputs)} text inputs")
        if inputs:
            tb = inputs[0]
            rec.click(tb["x"] + tb["w"] * 0.5, tb["cy"], move_frames=14, after_frames=16)
            rec.type_text("  Fluent 2 + DirectWrite!", per_char_frames=3)
            rec.wait_frames(24)
            if len(inputs) > 1:
                tb2 = inputs[1]
                rec.click(tb2["x"] + 60, tb2["cy"], move_frames=14, after_frames=14)
                rec.type_text("Hello KerneTile!", per_char_frames=3)
                rec.wait_frames(24)
        rec.end_clip("06_text.png")

        # 7. DockPanel & DataGrid
        rec.send("NAV layout_dock_panel")
        rec.start_clip("dock_data", "clip_07_dock_data.mp4")
        rec.wait_frames(22)
        dock_ws = rec.list_widgets()
        for w in dock_ws[:3]:
            rec.move_to(w["cx"], w["cy"], frames=14)
            rec.wait_frames(10)
        # Switch to DataGrid
        rec.send("NAV data_grid")
        rec.wait_frames(22)
        dg_ws = rec.list_widgets()
        # Click a column header or row and scroll smoothly
        if dg_ws:
            target = dg_ws[min(2, len(dg_ws) - 1)]
            rec.click(target["cx"], target["cy"], move_frames=14, after_frames=16)
        rec.scroll(660, 420, dy=-120, steps=8)
        rec.wait_frames(18)
        rec.end_clip("07_dock_data.png")

        # 8. Toast & ProgressRing
        rec.send("NAV toast")
        rec.start_clip("feedback", "clip_08_feedback.mp4")
        rec.wait_frames(18)
        toast_btns = [w for w in rec.list_widgets() if w["w"] >= 50 and w["h"] <= 48 and w["y"] >= 150]
        print(f"  Toast page found {len(toast_btns)} trigger buttons")
        for tb in toast_btns[:3]:
            rec.click(tb["cx"], tb["cy"], move_frames=12, after_frames=22)
        rec.wait_frames(28)
        # Switch to ProgressRing
        rec.send("NAV progress_ring")
        rec.wait_frames(64)
        rec.end_clip("08_feedback.png")

        # Save real clip durations for Remotion exact sync
        Path(r"D:\KerneTile\video\src\clip_durations.json").write_text(
            json.dumps(rec.clip_durations, indent=2), encoding="utf-8"
        )
        print("Clip durations:", rec.clip_durations)

    finally:
        rec.close_gdi()
        try:
            rec.send("QUIT")
        except Exception:
            pass
        try:
            proc.wait(timeout=3)
        except Exception:
            proc.kill()
    print("All 8 live clips recorded via --automation successfully!")


if __name__ == "__main__":
    main()
