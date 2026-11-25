import React from "react";
import { AbsoluteFill, interpolate, spring, useCurrentFrame, useVideoConfig } from "remotion";
import { COLORS, FONTS } from "../theme";

const METRICS = [
  { label: "Gallery 单文件体积", value: "973 KB", sub: "无外部 VC++ Redist 依赖", color: COLORS.emerald },
  { label: "现代 C++20 内核", value: "0 异常 / 0 RTTI", sub: "无 .eh_frame 与 typeinfo 膨胀", color: COLORS.cyan },
  { label: "Fluent 2 控件库", value: "60+ 控件", sub: "声明式 DSL · Spring 动效", color: COLORS.indigo },
  { label: "跨平台与双后端", value: "GPU + CPU", sub: "Win / Linux / Android / Web", color: COLORS.amber },
];

export const IntroScene: React.FC = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();

  const badgeScale = spring({ frame, fps, config: { damping: 14, stiffness: 120 } });
  const titleY = interpolate(
    spring({ frame: frame - 8, fps, config: { damping: 16, stiffness: 110 } }),
    [0, 1],
    [40, 0]
  );
  const titleOpacity = interpolate(frame, [6, 22], [0, 1], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
  });

  return (
    <AbsoluteFill
      style={{
        padding: "110px 80px 130px",
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        justifyContent: "center",
        fontFamily: FONTS.sans,
      }}
    >
      {/* Animated 4x4 Tile Emblem */}
      <div
        style={{
          transform: `scale(${badgeScale})`,
          display: "flex",
          alignItems: "center",
          gap: 14,
          padding: "10px 24px",
          borderRadius: 999,
          background: "rgba(56, 189, 248, 0.12)",
          border: "1px solid rgba(56, 189, 248, 0.4)",
          color: COLORS.cyan,
          fontSize: 20,
          fontWeight: 600,
          marginBottom: 28,
        }}
      >
        <span>⚡ Compute-Shader-Driven 16×16 Tile 2D Engine</span>
      </div>

      {/* Main Title */}
      <div
        style={{
          opacity: titleOpacity,
          transform: `translateY(${titleY}px)`,
          textAlign: "center",
          marginBottom: 22,
        }}
      >
        <div
          style={{
            fontSize: 96,
            fontWeight: 800,
            letterSpacing: "-0.02em",
            lineHeight: 1.08,
            background: "linear-gradient(135deg, #FFFFFF 0%, #38BDF8 55%, #818CF8 100%)",
            WebkitBackgroundClip: "text",
            WebkitTextFillColor: "transparent",
          }}
        >
          KerneTile &amp; KTUI
        </div>
        <div
          style={{
            marginTop: 18,
            fontSize: 32,
            color: COLORS.textSecondary,
            fontWeight: 500,
          }}
        >
          不到 1 MB 的现代 C++20 计算着色器 2D 渲染引擎与 Fluent 2 声明式界面框架
        </div>
      </div>

      {/* 4 Core Metric Cards */}
      <div
        style={{
          display: "grid",
          gridTemplateColumns: "repeat(4, 1fr)",
          gap: 24,
          width: "100%",
          maxWidth: 1640,
          marginTop: 48,
        }}
      >
        {METRICS.map((item, idx) => {
          const delay = 26 + idx * 10;
          const cardSpring = spring({
            frame: frame - delay,
            fps,
            config: { damping: 14, stiffness: 115 },
          });
          const y = interpolate(cardSpring, [0, 1], [36, 0]);
          const opacity = interpolate(frame, [delay, delay + 14], [0, 1], {
            extrapolateLeft: "clamp",
            extrapolateRight: "clamp",
          });

          return (
            <div
              key={item.label}
              style={{
                opacity,
                transform: `translateY(${y}px)`,
                background: COLORS.bgCard,
                border: `1px solid ${COLORS.border}`,
                borderTop: `3px solid ${item.color}`,
                borderRadius: 20,
                padding: "28px 26px",
                boxShadow: "0 20px 45px rgba(0, 0, 0, 0.45)",
              }}
            >
              <div style={{ fontSize: 18, color: COLORS.textSecondary, marginBottom: 10 }}>
                {item.label}
              </div>
              <div
                style={{
                  fontSize: 40,
                  fontWeight: 800,
                  color: item.color,
                  fontFamily: FONTS.mono,
                  marginBottom: 10,
                }}
              >
                {item.value}
              </div>
              <div style={{ fontSize: 17, color: COLORS.textMuted }}>{item.sub}</div>
            </div>
          );
        })}
      </div>
    </AbsoluteFill>
  );
};
