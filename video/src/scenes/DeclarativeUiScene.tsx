import React from "react";
import { AbsoluteFill, interpolate, spring, useCurrentFrame, useVideoConfig } from "remotion";
import { COLORS, FONTS } from "../theme";

const KTUI_CODE = [
  { text: "// KTUI C++20 指定初始化器 + Property<T> 响应式绑定" },
  { text: "Property<int> count{0};", highlight: true },
  { text: "" },
  { text: "window->set_content(VStack {" },
  { text: "    .align_items = AlignItems::Stretch," },
  { text: "    .spacing = 12.0f," },
  { text: "    .content = {" },
  { text: '        Text { .text = "Hello KerneTile", .font_size = 24.0f },' },
  { text: "        Button {", highlight: true },
  { text: '            .text = "点击计数 +1", .variant = ButtonVariant::Accent,' },
  { text: "            .on_click = [&]() { count.set(count.get() + 1); }", highlight: true },
  { text: "        }" },
  { text: "    }" },
  { text: "});" },
];

const CONTROLS = [
  "DockPanel",
  "VirtualList (10万行)",
  "HexViewer",
  "SyntaxFilterBar",
  "NavigationView",
  "InspectorGrid",
  "ColorPicker",
  "Spring Physics",
  "DirectWrite / Pango",
  "Mica / Acrylic",
];

export const DeclarativeUiScene: React.FC = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();

  // Simulate 3 spring clicks at frames 90, 195, 300
  const clickFrames = [90, 195, 300];
  const count = clickFrames.filter((cf) => frame >= cf).length;

  let btnScale = 1;
  for (const cf of clickFrames) {
    if (frame >= cf && frame < cf + 20) {
      const s = spring({ frame: frame - cf, fps, config: { damping: 10, stiffness: 260 } });
      btnScale = interpolate(s, [0, 0.35, 1], [0.92, 1.04, 1]);
    }
  }

  return (
    <AbsoluteFill
      style={{
        padding: "96px 64px 128px",
        display: "grid",
        gridTemplateColumns: "1.2fr 1fr",
        gap: 36,
        alignItems: "center",
        fontFamily: FONTS.sans,
      }}
    >
      {/* Left: C++20 Declarative DSL Code */}
      <div
        style={{
          background: COLORS.codeBg,
          border: `1.5px solid ${COLORS.indigo}`,
          borderRadius: 20,
          overflow: "hidden",
          boxShadow: "0 20px 55px rgba(0,0,0,0.5)",
        }}
      >
        <div
          style={{
            display: "flex",
            alignItems: "center",
            justifyContent: "space-between",
            padding: "14px 22px",
            background: "#0F172A",
            borderBottom: "1px solid rgba(148,163,184,0.16)",
          }}
        >
          <span style={{ fontFamily: FONTS.mono, fontSize: 17, color: COLORS.textPrimary, fontWeight: 600 }}>
            examples/counter_demo.cpp
          </span>
          <span
            style={{
              padding: "4px 12px",
              borderRadius: 999,
              background: "rgba(129,140,248,0.2)",
              color: COLORS.indigo,
              fontSize: 14,
              fontWeight: 700,
            }}
          >
            C++20 Declarative DSL
          </span>
        </div>

        <div style={{ padding: "22px 20px", fontFamily: FONTS.mono, fontSize: 18, lineHeight: 1.72 }}>
          {KTUI_CODE.map((line, i) => {
            const opacity = interpolate(frame, [i * 2.5, i * 2.5 + 10], [0, 1], {
              extrapolateLeft: "clamp",
              extrapolateRight: "clamp",
            });
            return (
              <div
                key={i}
                style={{
                  opacity,
                  display: "flex",
                  gap: 16,
                  padding: "2px 12px",
                  borderRadius: 8,
                  background: line.highlight ? "rgba(129,140,248,0.16)" : "transparent",
                  borderLeft: line.highlight ? `3px solid ${COLORS.indigo}` : "3px solid transparent",
                  color: line.text.trim().startsWith("//") ? "#64748B" : "#E2E8F0",
                  whiteSpace: "pre",
                }}
              >
                <span style={{ color: "#475569", width: 24, textAlign: "right" }}>{i + 1}</span>
                <span>{line.text}</span>
              </div>
            );
          })}
        </div>
      </div>

      {/* Right: Live Fluent 2 Preview + 60+ Controls Cloud */}
      <div style={{ display: "flex", flexDirection: "column", gap: 24 }}>
        {/* Simulated Fluent 2 Mica Window */}
        <div
          style={{
            background: "linear-gradient(145deg, rgba(30,41,59,0.92), rgba(15,23,42,0.96))",
            border: "1.5px solid rgba(56,189,248,0.4)",
            borderRadius: 22,
            overflow: "hidden",
            boxShadow: "0 24px 60px rgba(0,0,0,0.55)",
          }}
        >
          <div
            style={{
              padding: "12px 20px",
              background: "rgba(15,23,42,0.85)",
              borderBottom: "1px solid rgba(148,163,184,0.14)",
              display: "flex",
              justifyContent: "space-between",
              alignItems: "center",
              fontSize: 15,
              color: COLORS.textSecondary,
            }}
          >
            <span>KTUI Fluent 2 Live Preview (Mica)</span>
            <span style={{ color: COLORS.emerald, fontFamily: FONTS.mono }}>60 FPS · Spring</span>
          </div>

          <div
            style={{
              padding: "36px 34px",
              display: "flex",
              flexDirection: "column",
              alignItems: "center",
              gap: 20,
            }}
          >
            <div style={{ fontSize: 34, fontWeight: 800, color: COLORS.textPrimary }}>
              Hello KerneTile
            </div>
            <div style={{ fontSize: 20, color: COLORS.textSecondary }}>
              当前响应式计数：
              <span
                style={{
                  color: COLORS.cyan,
                  fontFamily: FONTS.mono,
                  fontSize: 34,
                  fontWeight: 800,
                  marginLeft: 10,
                }}
              >
                {count}
              </span>
            </div>
            <div
              style={{
                transform: `scale(${btnScale})`,
                padding: "14px 36px",
                borderRadius: 12,
                background: "linear-gradient(135deg, #0078D4, #38BDF8)",
                color: "#FFFFFF",
                fontSize: 21,
                fontWeight: 700,
                boxShadow: "0 10px 26px rgba(0, 120, 212, 0.45)",
              }}
            >
              点击计数 +1
            </div>
          </div>
        </div>

        {/* 60+ Controls Pill Cloud */}
        <div
          style={{
            background: COLORS.bgCard,
            border: `1px solid ${COLORS.border}`,
            borderRadius: 20,
            padding: "22px 24px",
          }}
        >
          <div style={{ fontSize: 16, color: COLORS.cyan, fontWeight: 700, marginBottom: 14 }}>
            内置 60+ 原生桌面 / 工程控件 · 桥接系统原生文本引擎（零 ICU / HarfBuzz 体积）
          </div>
          <div style={{ display: "flex", flexWrap: "wrap", gap: 10 }}>
            {CONTROLS.map((ctrl, idx) => {
              const pop = spring({
                frame: frame - (40 + idx * 5),
                fps,
                config: { damping: 14, stiffness: 140 },
              });
              return (
                <div
                  key={ctrl}
                  style={{
                    transform: `scale(${pop})`,
                    padding: "7px 14px",
                    borderRadius: 10,
                    background: "rgba(56,189,248,0.12)",
                    border: "1px solid rgba(56,189,248,0.28)",
                    color: COLORS.textPrimary,
                    fontSize: 15,
                    fontFamily: FONTS.mono,
                  }}
                >
                  {ctrl}
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </AbsoluteFill>
  );
};
