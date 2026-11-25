import asyncio
import json
import os
import shutil
from pathlib import Path
import edge_tts

VOICE = "zh-CN-YunyangNeural"
RATE = "+12%"
PITCH = "+0Hz"
PAUSE_BETWEEN_LINES_SEC = 0.18
PAUSE_BETWEEN_SCENES_SEC = 0.35

SCENES = [
    {
        "id": "intro",
        "title": "KerneTile & KTUI",
        "lines": [
            "跨平台桌面应用动辄上百兆，一套完整的现代 UI 引擎，能否压缩到一兆以内？",
            "这就是 KerneTile 与 KTUI——纯 C++20 零异常、零 RTTI 构建的计算着色器 2D 渲染引擎，与 Fluent 2 声明式框架。",
        ],
    },
    {
        "id": "architecture",
        "title": "四层解耦架构",
        "lines": [
            "KerneTile 采用清晰的四层解耦架构。顶层 KTUI 将控件树扁平化为紧凑图元数组。",
            "中层内核通过十六乘十六瓦片分箱与脏块剔除，局部交互时可跳过百分之九十九以上的无效计算。",
            "底层仅用六个 HAL 接口驱动 D3D12、Vulkan 与 WebGPU，并内置同源 CPU 光栅化器，搭配 PAL 平台层轻松覆盖全端。",
        ],
    },
    {
        "id": "core_code",
        "title": "80 字节图元与解析 SDF",
        "lines": [
            "看核心代码：每个元素压缩为严格对齐的八十字节 UiPrimitive，与 GPU 端二进制精确镜像。",
            "渲染无需三角网格或贴图，计算着色器逐像素求值无分支圆角 SDF，任意缩放依然锐利无锯齿。",
            "配合四阶误差函数逼近的解析高斯阴影，单遍即得平滑软阴影，彻底省去多遍离屏模糊开销。",
        ],
    },
    {
        "id": "declarative_ui",
        "title": "KTUI C++20 声明式框架",
        "lines": [
            "开发体验上，KTUI 借助 C++20 指定初始化器，写出如现代前端般简洁的声明式代码。",
            "内置响应式绑定、弹簧动效与六十余种控件，直接桥接系统原生字体，彻底告别几十兆运行时捆绑。",
        ],
    },
    {
        "id": "terminal_bench",
        "title": "终端构建、测试与体积对比",
        "lines": [
            "终端实测 Clang-CL 与 ThinLTO 极致构建，含全部控件的 Gallery 单文件仅 973 KB，零外部运行库依赖。",
            "三十九项核心单元测试瞬间全绿通过，相比传统方案体积缩减一个数量级以上。",
        ],
    },
    {
        "id": "outro",
        "title": "全平台与开源生态",
        "lines": [
            "轻量、高性能且极易移植，欢迎访问 KerneTile 仓库或体验 WebAssembly 在线演示。",
        ],
    },
]


def _mp3_duration_seconds(mp3_bytes: bytes) -> float:
    bitrate_table_v1_l3 = [
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
    ]
    bitrate_table_v2_l3 = [
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
    ]
    sr_table = {
        3: [44100, 48000, 32000],
        2: [22050, 24000, 16000],
        0: [11025, 12000, 8000],
    }

    i = 0
    n = len(mp3_bytes)
    if n >= 10 and mp3_bytes[:3] == b"ID3":
        size = (
            ((mp3_bytes[6] & 0x7F) << 21)
            | ((mp3_bytes[7] & 0x7F) << 14)
            | ((mp3_bytes[8] & 0x7F) << 7)
            | (mp3_bytes[9] & 0x7F)
        )
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
                    br = bitrate_table_v1_l3[bitrate_idx] * 1000
                    samples = 1152
                    frame_len = int(144 * br // sr + padding)
                else:
                    br = bitrate_table_v2_l3[bitrate_idx] * 1000
                    samples = 576
                    frame_len = int(72 * br // sr + padding)
                if frame_len > 0:
                    total_sec += samples / sr
                    i += frame_len
                    continue
        i += 1
    return total_sec


def _format_srt_time(seconds: float) -> str:
    ms = int(round(seconds * 1000))
    hours = ms // 3_600_000
    ms %= 3_600_000
    minutes = ms // 60_000
    ms %= 60_000
    secs = ms // 1000
    ms %= 1000
    return f"{hours:02d}:{minutes:02d}:{secs:02d},{ms:03d}"


async def synth_line(text: str, out_file: Path, proxy: str | None):
    comm = edge_tts.Communicate(text=text, voice=VOICE, rate=RATE, pitch=PITCH, proxy=proxy)
    audio_chunks = bytearray()
    boundaries = []
    async for chunk in comm.stream():
        if chunk["type"] == "audio":
            audio_chunks.extend(chunk["data"])
        elif chunk["type"] in ("WordBoundary", "SentenceBoundary"):
            boundaries.append(
                {
                    "text": chunk["text"],
                    "start": chunk["offset"] / 10_000_000.0,
                    "end": (chunk["offset"] + chunk["duration"]) / 10_000_000.0,
                }
            )
    out_file.write_bytes(audio_chunks)
    dur = _mp3_duration_seconds(bytes(audio_chunks))
    if dur <= 0.1 and boundaries:
        dur = boundaries[-1]["end"] + 0.1
    return dur


async def main():
    video_dir = Path(r"D:\KerneTile\video")
    audio_dir = video_dir / "public" / "audio"
    if audio_dir.exists():
        shutil.rmtree(audio_dir)
    audio_dir.mkdir(parents=True, exist_ok=True)
    proxy = os.environ.get("HTTPS_PROXY") or os.environ.get("HTTP_PROXY")

    fps = 30
    cursor = 0.4
    scene_timeline = []
    subtitles = []
    clips = []

    line_idx = 0
    for s_idx, scene in enumerate(SCENES):
        scene_start = 0.0 if s_idx == 0 else cursor
        for l_idx, text in enumerate(scene["lines"]):
            line_idx += 1
            clip_name = f"line_{line_idx:02d}.mp3"
            clip_path = audio_dir / clip_name
            print(f"[{line_idx:02d}] Synthesizing: {text[:24]}...")
            dur = await synth_line(text, clip_path, proxy)
            line_start = cursor
            line_end = cursor + dur
            clips.append(
                {
                    "index": line_idx,
                    "sceneId": scene["id"],
                    "file": f"audio/{clip_name}",
                    "text": text,
                    "start": round(line_start, 3),
                    "end": round(line_end, 3),
                    "startFrame": int(round(line_start * fps)),
                    "durationFrames": max(1, int(round(dur * fps))),
                    "duration": round(dur, 3),
                }
            )
            subtitles.append(
                {
                    "id": line_idx,
                    "sceneId": scene["id"],
                    "start": round(line_start, 3),
                    "end": round(line_end, 3),
                    "startFrame": int(round(line_start * fps)),
                    "endFrame": int(round(line_end * fps)),
                    "text": text,
                }
            )
            cursor = line_end + (
                PAUSE_BETWEEN_LINES_SEC if l_idx < len(scene["lines"]) - 1 else 0.0
            )
        scene_end = cursor + (PAUSE_BETWEEN_SCENES_SEC if s_idx < len(SCENES) - 1 else 1.0)
        start_f = int(round(scene_start * fps))
        end_f = int(round(scene_end * fps))
        scene_timeline.append(
            {
                "id": scene["id"],
                "title": scene["title"],
                "start": round(scene_start, 3),
                "end": round(scene_end, 3),
                "duration": round(scene_end - scene_start, 3),
                "startFrame": start_f,
                "durationFrames": max(1, end_f - start_f),
            }
        )
        cursor = scene_end

    total_duration = round(cursor, 3)
    total_frames = int(round(total_duration * fps))

    # Write SRT
    srt_lines = []
    for sub in subtitles:
        srt_lines.append(
            f"{sub['id']}\n{_format_srt_time(sub['start'])} --> {_format_srt_time(sub['end'])}\n{sub['text']}\n"
        )
    (video_dir / "public" / "subtitles.srt").write_text("\n".join(srt_lines), encoding="utf-8")

    # Write timeline JSON to src/timeline.json
    timeline_data = {
        "voice": VOICE,
        "rate": RATE,
        "fps": fps,
        "totalDuration": total_duration,
        "totalFrames": total_frames,
        "scenes": scene_timeline,
        "clips": clips,
        "subtitles": subtitles,
    }
    (video_dir / "src" / "timeline.json").write_text(
        json.dumps(timeline_data, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"Done! Total duration: {total_duration:.2f}s ({total_frames} frames @ {fps}fps)")
    for s in scene_timeline:
        print(f"  Scene {s['id']:15s}: {s['start']:6.2f}s -> {s['end']:6.2f}s ({s['duration']:5.2f}s, frames {s['startFrame']}..{s['startFrame']+s['durationFrames']})")


if __name__ == "__main__":
    asyncio.run(main())
