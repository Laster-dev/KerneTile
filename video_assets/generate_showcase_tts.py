import asyncio
import json
import os
import shutil
from pathlib import Path
import edge_tts

VOICE = "zh-CN-XiaoxiaoNeural"  # 温暖自然的中文女声，适合产品/UI 设计讲解
RATE = "+10%"
PITCH = "+0Hz"
PAUSE_BETWEEN_LINES_SEC = 0.20
PAUSE_BETWEEN_SCENES_SEC = 0.35

SCENES = [
    {
        "id": "home",
        "title": "Fluent 2 视觉体系",
        "badge": "Mica · 圆形涟漪主题",
        "clip": "clips/clip_01_home.mp4",
        "specs": [
            "对标 WinUI 3 / Fluent 2 设计语言",
            "云母层次卡片 + 1px 极细高光描边",
            "无撕裂圆形涟漪深浅主题切换",
        ],
        "lines": [
            "欢迎来到 KTUI Gallery 实机演示！这是一套完全对标 Windows 11 WinUI 3 与 Fluent 2 设计规范的 C++20 桌面控件库。",
            "首先可以看到整体的云母材质层次、统一的微圆角卡片与极细高光描边；点击右上角主题按钮，还能体验无撕裂的圆形涟漪深浅主题切换。",
        ],
    },
    {
        "id": "button",
        "title": "按钮与长按确认",
        "badge": "Button · HoldConfirm",
        "clip": "clips/clip_02_button.mp4",
        "specs": [
            "Accent / Standard / Danger 清晰层级",
            "解析 SDF 实时抗锯齿圆角与边框",
            "HoldConfirmButton 长按进度防误触",
        ],
        "lines": [
            "进入基础输入，先看按钮家族。标准按钮、强调色主按钮与危险操作按钮层级分明，每个圆角边框都由解析 SDF 实时绘制。",
            "针对高危操作，还内置了 HoldConfirmButton，需要长按填满进度后才触发确认，优雅防误触。",
        ],
    },
    {
        "id": "toggle",
        "title": "开关与复选框",
        "badge": "ToggleSwitch · CheckBox",
        "clip": "clips/clip_03_toggle.mp4",
        "specs": [
            "Spring 弹簧物理动效与微形变反馈",
            "悬停/按压平滑状态插值过渡",
            "完整支持勾选与三态交互规范",
        ],
        "lines": [
            "再来看状态切换控件。ToggleSwitch 拨动时搭载了弹簧物理动效，圆球会在按压与滑动时产生细腻的弹性形变。",
            "CheckBox 则严格还原 Fluent 2 勾选与三态视觉反馈，悬停和点击过渡顺滑自然。",
        ],
    },
    {
        "id": "slider",
        "title": "连续数值滑块",
        "badge": "Slider · 99% 脏块剔除",
        "clip": "clips/clip_04_slider.mp4",
        "specs": [
            "SDF 圆形拇指手柄 + 悬停内芯缩放",
            "轨道强调色实时联动数值绑定",
            "16×16 脏瓦片剔除，极低渲染开销",
        ],
        "lines": [
            "Slider 滑块在拖动时，内芯会自动微缩放，进度轨道实时响应。",
            "得益于底层十六乘十六脏瓦片剔除，即使高频拖拽，整屏也只有滑块附近的极少数瓦片在重绘。",
        ],
    },
    {
        "id": "color",
        "title": "专业拾色器",
        "badge": "ColorPicker · 纯 Shader",
        "clip": "clips/clip_05_color.mp4",
        "specs": [
            "HSV 色盘与色相条纯着色器绘制",
            "零外部位图贴图，任意 DPI 锐利",
            "RGBA / HEX 实时双向响应联动",
        ],
        "lines": [
            "像 ColorPicker 这样的复杂图形控件，同样不依赖任何贴图资源，色盘、色相条与预览色块全部由计算着色器实时渲染。",
        ],
    },
    {
        "id": "text",
        "title": "文本输入与排版",
        "badge": "TextBox · DirectWrite",
        "clip": "clips/clip_06_text.mp4",
        "specs": [
            "经典 Fluent 2 底部 2px 强调聚焦条",
            "桥接 Windows DirectWrite 原生排版",
            "完整 IME 状态机与高清晰小字渲染",
        ],
        "lines": [
            "来到文本输入区。点击 TextBox 聚焦，底部会浮现经典的 Fluent 2 强调色聚焦条，字体直接桥接 Windows DirectWrite 原生引擎，清晰锐利且完整支持输入法。",
        ],
    },
    {
        "id": "dock_data",
        "title": "停靠布局与表格",
        "badge": "DockPanel · DataGrid",
        "clip": "clips/clip_07_dock_data.mp4",
        "specs": [
            "VS 风格 DockPanel 拖拽停靠面板",
            "DataGrid 高性能数据表与滚轮平滑滚动",
            "面向 IDE / 工程桌面软件打造",
        ],
        "lines": [
            "面向专业桌面软件，KTUI 提供了类似 Visual Studio 风格的 DockPanel 拖拽停靠布局，以及支持流畅滚轮浏览的高性能 DataGrid 数据表格。",
        ],
    },
    {
        "id": "feedback",
        "title": "浮层通知与加载",
        "badge": "Toast · ProgressRing",
        "clip": "clips/clip_08_feedback.mp4",
        "specs": [
            "单 Pass 解析高斯阴影营造立体景深",
            "非打断式多级 Toast 状态通知",
            "解析矢量 ProgressRing 丝滑加载环",
        ],
        "lines": [
            "最后看反馈体系：Toast 浮层通知使用单遍解析高斯阴影营造立体景深，而 ProgressRing 则以丝滑的无锯齿动画呈现加载状态。",
            "全部六十余种 Fluent 2 控件打包进单文件不到一兆，这就是 KerneTile 与 KTUI。",
        ],
    },
]


def _mp3_duration_seconds(mp3_bytes: bytes) -> float:
    bitrate_table_v1_l3 = [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0]
    bitrate_table_v2_l3 = [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0]
    sr_table = {3: [44100, 48000, 32000], 2: [22050, 24000, 16000], 0: [11025, 12000, 8000]}
    i, n = 0, len(mp3_bytes)
    if n >= 10 and mp3_bytes[:3] == b"ID3":
        size = ((mp3_bytes[6] & 0x7F) << 21) | ((mp3_bytes[7] & 0x7F) << 14) | ((mp3_bytes[8] & 0x7F) << 7) | (mp3_bytes[9] & 0x7F)
        i = 10 + size
    total_sec = 0.0
    while i + 4 <= n:
        b0, b1, b2 = mp3_bytes[i], mp3_bytes[i + 1], mp3_bytes[i + 2]
        if b0 == 0xFF and (b1 & 0xE0) == 0xE0:
            version_id = (b1 >> 3) & 0x03
            layer = (b1 >> 1) & 0x03
            bitrate_idx = (b2 >> 4) & 0x0F
            sr_idx = (b2 >> 2) & 0x03
            padding = (b2 >> 1) & 0x01
            if version_id in sr_table and layer == 1 and 0 < bitrate_idx < 15 and sr_idx < 3:
                sr = sr_table[version_id][sr_idx]
                if version_id == 3:
                    br, samples = bitrate_table_v1_l3[bitrate_idx] * 1000, 1152
                    frame_len = int(144 * br // sr + padding)
                else:
                    br, samples = bitrate_table_v2_l3[bitrate_idx] * 1000, 576
                    frame_len = int(72 * br // sr + padding)
                if frame_len > 0:
                    total_sec += samples / sr
                    i += frame_len
                    continue
        i += 1
    return total_sec


async def synth_line(text: str, out_file: Path, proxy: str | None) -> float:
    comm = edge_tts.Communicate(text=text, voice=VOICE, rate=RATE, pitch=PITCH, proxy=proxy)
    audio_chunks = bytearray()
    async for chunk in comm.stream():
        if chunk["type"] == "audio":
            audio_chunks.extend(chunk["data"])
    out_file.write_bytes(audio_chunks)
    return _mp3_duration_seconds(bytes(audio_chunks))


async def main():
    video_dir = Path(r"D:\KerneTile\video")
    audio_dir = video_dir / "public" / "showcase_audio"
    if audio_dir.exists():
        shutil.rmtree(audio_dir)
    audio_dir.mkdir(parents=True, exist_ok=True)
    proxy = os.environ.get("HTTPS_PROXY") or os.environ.get("HTTP_PROXY")

    fps = 30
    cursor = 0.35
    scene_timeline = []
    subtitles = []
    clips = []

    line_idx = 0
    for s_idx, scene in enumerate(SCENES):
        scene_start = 0.0 if s_idx == 0 else cursor
        for l_idx, text in enumerate(scene["lines"]):
            line_idx += 1
            clip_name = f"sc_{line_idx:02d}.mp3"
            print(f"[{line_idx:02d}] Synthesizing: {text[:24]}...")
            dur = await synth_line(text, audio_dir / clip_name, proxy)
            line_start = cursor
            line_end = cursor + dur
            clips.append({
                "index": line_idx,
                "sceneId": scene["id"],
                "file": f"showcase_audio/{clip_name}",
                "text": text,
                "start": round(line_start, 3),
                "end": round(line_end, 3),
                "startFrame": int(round(line_start * fps)),
                "durationFrames": max(1, int(round(dur * fps))),
            })
            subtitles.append({
                "id": line_idx,
                "sceneId": scene["id"],
                "startFrame": int(round(line_start * fps)),
                "endFrame": int(round(line_end * fps)),
                "text": text,
            })
            cursor = line_end + (PAUSE_BETWEEN_LINES_SEC if l_idx < len(scene["lines"]) - 1 else 0.0)
        scene_end = cursor + (PAUSE_BETWEEN_SCENES_SEC if s_idx < len(SCENES) - 1 else 0.9)
        start_f = int(round(scene_start * fps))
        end_f = int(round(scene_end * fps))
        scene_timeline.append({
            "id": scene["id"],
            "title": scene["title"],
            "badge": scene["badge"],
            "clip": scene["clip"],
            "specs": scene["specs"],
            "startFrame": start_f,
            "durationFrames": max(1, end_f - start_f),
            "duration": round(scene_end - scene_start, 3),
        })
        cursor = scene_end

    total_frames = int(round(cursor * fps))
    out = {
        "voice": VOICE,
        "fps": fps,
        "totalDuration": round(cursor, 3),
        "totalFrames": total_frames,
        "scenes": scene_timeline,
        "clips": clips,
        "subtitles": subtitles,
    }
    (video_dir / "src" / "showcase_timeline.json").write_text(
        json.dumps(out, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"Showcase TTS Done! Total: {cursor:.2f}s ({total_frames} frames @ {fps}fps)")


if __name__ == "__main__":
    asyncio.run(main())
