import React from "react";
import { AbsoluteFill, interpolate, spring, useCurrentFrame, useVideoConfig } from "remotion";
import { COLORS, FONTS } from "../theme";

const PLATFORMS = [
  { name: "Windows", tech: "Direct3D 12 / WARP", color: COLORS.cyan },
  { name: "Linux", tech: "Vulkan · X11 / Wayland", color: COLORS.indigo },
  { name: "Android", tech: "NDK · Vulkan / CPU", color: COLORS.emerald },
  { name: "WebAssembly", tech: "WebGPU / Canvas", color: COLORS.amber },
  { name: "Embedded / RTOS", tech: "CPU Tile → Framebuffer", color: COLORS.rose },
];

export const OutroScene: React.FC = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();

  const titleSpring = spring({ frame, fps, config: { damping: 14, stiffness: 110 } });

  return (
    <AbsoluteFill
      style={{
        padding: "110px 80px 135px",
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        justifyContent: "center",
        fontFamily: FONTS.sans,
      }}
    >
      <div
        style={{
          transform: `scale(${interpolate(titleSpring, [0, 1], [0.9, 1])})`,
          opacity: titleSpring,
          textAlign: "center",
        }}
      >
        <div
          style={{
            fontSize: 76,
            fontWeight: 800,
            background: "linear-gradient(135deg, #FFFFFF 0%, #38BDF8 50%, #10B981 100%)",
            WebkitBackgroundClip: "text",
            WebkitTextFillColor: "transparent",
          }}
        >
          KerneTile &amp; KTUI
        </div>
        <div style={{ fontSize: 28, color: COLORS.textSecondary, marginTop: 14 }}>
          1 MB 以内的现代跨平台 C++20 图形引擎与声明式界面框架
        </div>
      </div>

      {/* Platform Badges */}
      <div
        style={{
          display: "grid",
          gridTemplateColumns: "repeat(5, 1fr)",
          gap: 18,
          width: "100%",
          maxWidth: 1620,
          marginTop: 48,
        }}
      >
        {PLATFORMS.map((p, idx) => {
          const pop = spring({
            frame: frame - (12 + idx * 6),
            fps,
            config: { damping: 14, stiffness: 130 },
          });
          return (
            <div
              key={p.name}
              style={{
                transform: `translateY(${interpolate(pop, [0, 1], [28, 0])}px)`,
                opacity: pop,
                background: COLORS.bgCard,
                border: `1px solid ${COLORS.border}`,
                borderBottom: `3px solid ${p.color}`,
                borderRadius: 16,
                padding: "22px 18px",
                textAlign: "center",
              }}
            >
              <div style={{ fontSize: 22, fontWeight: 700, color: COLORS.textPrimary }}>{p.name}</div>
              <div style={{ fontSize: 15, color: p.color, fontFamily: FONTS.mono, marginTop: 8 }}>
                {p.tech}
              </div>
            </div>
          );
        })}
      </div>

      {/* Links Card */}
      <div
        style={{
          marginTop: 46,
          display: "flex",
          gap: 24,
          opacity: interpolate(frame, [35, 55], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp" }),
        }}
      >
        <div
          style={{
            padding: "16px 30px",
            borderRadius: 14,
            background: "rgba(56,189,248,0.14)",
            border: "1px solid rgba(56,189,248,0.45)",
            color: COLORS.textPrimary,
            fontFamily: FONTS.mono,
            fontSize: 21,
          }}
        >
          🌐 Live Demo: laster-dev.github.io/KerneTile
        </div>
        <div
          style={{
            padding: "16px 30px",
            borderRadius: 14,
            background: "rgba(16,185,129,0.14)",
            border: "1px solid rgba(16,185,129,0.45)",
            color: COLORS.textPrimary,
            fontFamily: FONTS.mono,
            fontSize: 21,
          }}
        >
          ⭐ GitHub: Laster-dev/KerneTile
        </div>
      </div>
    </AbsoluteFill>
  );
};
