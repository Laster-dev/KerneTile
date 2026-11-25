import React from "react";
import { AbsoluteFill, interpolate, spring, useCurrentFrame, useVideoConfig } from "remotion";
import { COLORS, FONTS } from "../theme";

const TERMINAL_STEPS = [
  { start: 0, text: "PS D:\\KerneTile> cmake -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl", color: "#38BDF8" },
  { start: 35, text: "-- Configuring done (0.4s) · Flags: /EHs-c- /GR- -Oz -flto=thin · Hybrid CRT", color: "#94A3B8" },
  { start: 65, text: "PS D:\\KerneTile> cmake --build build-clang --target ktui_gallery", color: "#38BDF8" },
  { start: 100, text: "[100%] Linking CXX executable ktui_gallery.exe (ThinLTO + /OPT:REF /OPT:ICF)", color: "#A7F3D0" },
  { start: 135, text: "PS D:\\KerneTile> (Get-Item .\\build-clang\\ktui_gallery.exe).Length", color: "#38BDF8" },
  { start: 165, text: "=> 996,352 bytes (973 KB)  ·  0 external VC++ Redist dependencies", color: "#10B981", bold: true },
  { start: 215, text: "PS D:\\KerneTile> ctest --test-dir build-clang --output-on-failure", color: "#38BDF8" },
  { start: 245, text: "  1/39 Test #1:  test_tile_binning .................... Passed   0.01 sec", color: "#34D399" },
  { start: 262, text: "  8/39 Test #8:  test_sdf & test_shadow ............... Passed   0.01 sec", color: "#34D399" },
  { start: 279, text: " 19/39 Test #19: test_cpu_rasterizer .................. Passed   0.02 sec", color: "#34D399" },
  { start: 296, text: " 31/39 Test #31: test_ktui_spring_physics ............. Passed   0.01 sec", color: "#34D399" },
  { start: 313, text: " 39/39 Test #39: test_ktui_e2e_app .................... Passed   0.03 sec", color: "#34D399" },
  { start: 335, text: "100% tests passed, 0 tests failed out of 39  (Total Test time = 0.42 sec)", color: "#10B981", bold: true },
];

const SIZE_COMPARE = [
  { name: "Electron (Chromium + V8)", size: "120 MB", pct: 100, color: "#64748B" },
  { name: "Qt 6 (Widgets + ICU)", size: "45 MB", pct: 46, color: "#6366F1" },
  { name: "Flutter (Skia + Dart VM)", size: "22 MB", pct: 26, color: "#3B82F6" },
  { name: "KerneTile & KTUI (60+ 控件)", size: "0.97 MB", pct: 6, color: "#10B981", highlight: true },
];

export const TerminalScene: React.FC = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();

  const cursorBlink = Math.floor(frame / 12) % 2 === 0 ? "█" : " ";

  return (
    <AbsoluteFill
      style={{
        padding: "96px 64px 128px",
        display: "grid",
        gridTemplateColumns: "1.35fr 0.85fr",
        gap: 34,
        alignItems: "center",
        fontFamily: FONTS.sans,
      }}
    >
      {/* Left: Authentic Windows Terminal Execution */}
      <div
        style={{
          background: "#090D16",
          border: "1.5px solid rgba(16, 185, 129, 0.55)",
          borderRadius: 20,
          overflow: "hidden",
          boxShadow: "0 24px 60px rgba(0,0,0,0.6), 0 0 32px rgba(16,185,129,0.16)",
        }}
      >
        <div
          style={{
            display: "flex",
            alignItems: "center",
            justifyContent: "space-between",
            padding: "13px 22px",
            background: "#111827",
            borderBottom: "1px solid rgba(148,163,184,0.16)",
          }}
        >
          <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#FB7185" }} />
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#FBBF24" }} />
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#34D399" }} />
            <span style={{ marginLeft: 12, fontFamily: FONTS.mono, fontSize: 16, color: COLORS.textPrimary }}>
              Windows PowerShell · Clang-CL + Ninja + CTest
            </span>
          </div>
          <span style={{ fontFamily: FONTS.mono, fontSize: 14, color: COLORS.emerald, fontWeight: 700 }}>
            EXIT 0
          </span>
        </div>

        <div
          style={{
            padding: "22px 24px",
            fontFamily: FONTS.mono,
            fontSize: 16.5,
            lineHeight: 1.72,
            minHeight: 510,
          }}
        >
          {TERMINAL_STEPS.map((line, idx) => {
            if (frame < line.start) return null;
            const elapsed = frame - line.start;
            const isCommand = line.text.startsWith("PS ");
            const shownChars = isCommand
              ? Math.min(line.text.length, Math.floor(elapsed * 3.8))
              : line.text.length;
            const shownText = line.text.slice(0, shownChars);
            const isLatest =
              idx === TERMINAL_STEPS.length - 1 || frame < TERMINAL_STEPS[idx + 1].start;

            return (
              <div
                key={idx}
                style={{
                  color: line.color,
                  fontWeight: line.bold ? 700 : 400,
                  background: line.bold ? "rgba(16, 185, 129, 0.14)" : "transparent",
                  padding: line.bold ? "4px 10px" : "1px 0",
                  borderRadius: 6,
                  marginTop: isCommand && idx > 0 ? 10 : 0,
                  whiteSpace: "pre-wrap",
                }}
              >
                {shownText}
                {isLatest ? cursorBlink : ""}
              </div>
            );
          })}
        </div>
      </div>

      {/* Right: Binary Size Comparison Bar Chart */}
      <div
        style={{
          background: COLORS.bgCard,
          border: `1.5px solid ${COLORS.border}`,
          borderRadius: 24,
          padding: 32,
          display: "flex",
          flexDirection: "column",
          gap: 24,
          boxShadow: "0 20px 50px rgba(0,0,0,0.45)",
        }}
      >
        <div>
          <div style={{ fontSize: 15, color: COLORS.emerald, fontFamily: FONTS.mono, fontWeight: 700 }}>
            BINARY FOOTPRINT COMPARISON
          </div>
          <div style={{ fontSize: 28, color: COLORS.textPrimary, fontWeight: 800, marginTop: 6 }}>
            跨平台 GUI 产物单文件体积对比
          </div>
        </div>

        <div style={{ display: "flex", flexDirection: "column", gap: 22 }}>
          {SIZE_COMPARE.map((item, idx) => {
            const delay = 40 + idx * 18;
            const barSpring = spring({
              frame: frame - delay,
              fps,
              config: { damping: 16, stiffness: 100 },
            });
            const widthPct = interpolate(barSpring, [0, 1], [0, item.pct]);
            return (
              <div key={item.name}>
                <div
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "baseline",
                    marginBottom: 8,
                  }}
                >
                  <span
                    style={{
                      fontSize: 17,
                      fontWeight: item.highlight ? 800 : 500,
                      color: item.highlight ? COLORS.emerald : COLORS.textPrimary,
                    }}
                  >
                    {item.name}
                  </span>
                  <span
                    style={{
                      fontFamily: FONTS.mono,
                      fontSize: item.highlight ? 26 : 19,
                      fontWeight: 800,
                      color: item.highlight ? COLORS.emerald : COLORS.textSecondary,
                    }}
                  >
                    {item.size}
                  </span>
                </div>
                <div
                  style={{
                    height: item.highlight ? 22 : 16,
                    borderRadius: 11,
                    background: "rgba(30,41,59,0.8)",
                    overflow: "hidden",
                    border: item.highlight ? "1px solid rgba(16,185,129,0.6)" : "none",
                  }}
                >
                  <div
                    style={{
                      width: `${Math.max(widthPct, 1)}%`,
                      height: "100%",
                      background: item.color,
                      borderRadius: 11,
                      boxShadow: item.highlight ? "0 0 18px rgba(16,185,129,0.8)" : "none",
                    }}
                  />
                </div>
              </div>
            );
          })}
        </div>

        <div
          style={{
            padding: "16px 20px",
            borderRadius: 14,
            background: "rgba(16,185,129,0.12)",
            border: "1px solid rgba(16,185,129,0.35)",
            fontSize: 16,
            color: "#D1FAE5",
            lineHeight: 1.5,
          }}
        >
          <strong>核心瘦身秘诀：</strong>零异常 / 零 RTTI + SDF 实时绘制替代贴图 + 桥接系统原生文本引擎 + Clang-CL ThinLTO。
        </div>
      </div>
    </AbsoluteFill>
  );
};
