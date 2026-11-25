import React from "react";
import { AbsoluteFill, interpolate, spring, useCurrentFrame, useVideoConfig } from "remotion";
import { COLORS, FONTS } from "../theme";

export const ArchitectureScene: React.FC = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();

  // Focus phases synced to the 3 lines in architecture scene (~26s = 781 frames)
  // 0..220: Layer 1 KTUI -> UiPrimitive[]
  // 220..480: Layer 2 KerneTile 16x16 Tile Binning & Dirty Tracking
  // 480..781: Layer 3 GPU HAL + CPU Rasterizer + Layer 4 PAL
  const activeStage = frame < 210 ? 0 : frame < 460 ? 1 : 2;

  const pulseOffset = (frame * 5) % 100;

  const layerStyle = (stageMatch: boolean, delay: number) => {
    const enter = spring({ frame: frame - delay, fps, config: { damping: 16, stiffness: 120 } });
    return {
      opacity: interpolate(enter, [0, 1], [0, 1]),
      transform: `translateY(${interpolate(enter, [0, 1], [24, 0])}px) scale(${stageMatch ? 1.015 : 0.995})`,
      border: `1.5px solid ${stageMatch ? COLORS.cyan : COLORS.border}`,
      boxShadow: stageMatch
        ? "0 0 32px rgba(56, 189, 248, 0.24), 0 16px 36px rgba(0,0,0,0.45)"
        : "0 12px 28px rgba(0,0,0,0.35)",
      background: stageMatch ? "rgba(15, 29, 54, 0.92)" : COLORS.bgCard,
      borderRadius: 18,
      padding: "20px 28px",
      transition: "border-color 0.2s",
    } as React.CSSProperties;
  };

  // Interactive 8x6 tile grid demo showing 99%+ culling (only 3 dirty tiles active)
  const dirtyTileSet = new Set([18, 19, 26, 27]);
  const tileSweep = Math.floor(frame / 12) % 4;
  const movingDirty = new Set([10 + tileSweep, 18 + tileSweep, 26 + tileSweep]);

  return (
    <AbsoluteFill
      style={{
        padding: "96px 64px 128px",
        display: "grid",
        gridTemplateColumns: "1.45fr 0.85fr",
        gap: 36,
        alignItems: "center",
        fontFamily: FONTS.sans,
      }}
    >
      {/* Left: 4-Layer Architecture Diagram */}
      <div style={{ display: "flex", flexDirection: "column", gap: 14 }}>
        <div style={{ marginBottom: 4 }}>
          <div style={{ fontSize: 17, color: COLORS.cyan, fontWeight: 700, letterSpacing: "0.08em" }}>
            ARCHITECTURE OVERVIEW
          </div>
          <div style={{ fontSize: 38, color: COLORS.textPrimary, fontWeight: 800 }}>
            四层解耦架构：从声明式 UI 到 GPU/CPU 双轨渲染
          </div>
        </div>

        {/* Layer 1: KTUI */}
        <div style={layerStyle(activeStage === 0, 0)}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
            <div style={{ fontSize: 24, fontWeight: 700, color: COLORS.textPrimary }}>
              ① KTUI 声明式 UI 层 <span style={{ color: COLORS.cyan, fontSize: 16, fontFamily: FONTS.mono }}>KTUI/include/ktui/</span>
            </div>
            <span style={{ fontSize: 14, color: COLORS.cyan, fontWeight: 600 }}>C++20 DSL</span>
          </div>
          <div style={{ fontSize: 17, color: COLORS.textSecondary, marginTop: 8 }}>
            指定初始化器 DSL · Property&lt;T&gt; 细粒度绑定 · Flex/Grid/DockPanel · Spring 动效 · 60+ 控件
          </div>
        </div>

        {/* Animated Data Flow Connector 1 -> 2 */}
        <div style={{ display: "flex", alignItems: "center", justifyContent: "center", gap: 16, height: 26 }}>
          <div style={{ width: 220, height: 2, background: "linear-gradient(90deg, transparent, #38BDF8, transparent)" }} />
          <div
            style={{
              padding: "3px 16px",
              borderRadius: 999,
              background: "rgba(56,189,248,0.16)",
              border: "1px solid rgba(56,189,248,0.45)",
              color: COLORS.cyan,
              fontSize: 15,
              fontFamily: FONTS.mono,
              fontWeight: 600,
            }}
          >
            ▼ 扁平化 UiPrimitive[] (80B) + ClipRect[] (32B)
          </div>
          <div style={{ width: 220, height: 2, background: "linear-gradient(90deg, transparent, #38BDF8, transparent)" }} />
        </div>

        {/* Layer 2: KerneTile Render Core */}
        <div style={layerStyle(activeStage === 1, 14)}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
            <div style={{ fontSize: 24, fontWeight: 700, color: COLORS.textPrimary }}>
              ② KerneTile 渲染内核 <span style={{ color: COLORS.indigo, fontSize: 16, fontFamily: FONTS.mono }}>include/kernetile/, src/</span>
            </div>
            <span style={{ fontSize: 14, color: COLORS.emerald, fontWeight: 700 }}>99%+ 脏瓦片剔除</span>
          </div>
          <div style={{ fontSize: 17, color: COLORS.textSecondary, marginTop: 8 }}>
            16×16 Tile Binning · 帧间签名脏块剔除 · 无锁环形提交 (vring) · 解析 SDF &amp; 高斯阴影 · Shelf 动态字形图集
          </div>
        </div>

        {/* Dual Track Connector */}
        <div style={{ display: "grid", gridTemplateColumns: "1.25fr 1fr", gap: 18 }}>
          {/* Layer 3A: GPU HAL */}
          <div style={layerStyle(activeStage === 2, 28)}>
            <div style={{ fontSize: 21, fontWeight: 700, color: COLORS.cyan }}>
              ③A 图形硬件抽象层 GPU HAL
            </div>
            <div style={{ fontSize: 15, color: COLORS.textSecondary, marginTop: 8, lineHeight: 1.5 }}>
              仅 6 个纯虚 Compute 接口 · Slang 单源码多后端<br />
              <span style={{ color: COLORS.textPrimary, fontFamily: FONTS.mono }}>
                D3D12 (DXIL) · Vulkan (SPIR-V) · WebGPU (WGSL)
              </span>
            </div>
          </div>

          {/* Layer 3B: CPU Rasterizer */}
          <div style={layerStyle(activeStage === 2, 34)}>
            <div style={{ fontSize: 21, fontWeight: 700, color: COLORS.emerald }}>
              ③B 纯 CPU 瓦片光栅化
            </div>
            <div style={{ fontSize: 15, color: COLORS.textSecondary, marginTop: 8, lineHeight: 1.5 }}>
              数学语义与 GPU 100% 同源 · 零独立显卡依赖<br />
              <span style={{ color: COLORS.textPrimary, fontFamily: FONTS.mono }}>
                嵌入式 / 虚拟机 / Framebuffer 直出 RGBA8
              </span>
            </div>
          </div>
        </div>

        {/* Layer 4: PAL */}
        <div style={layerStyle(activeStage === 2, 42)}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
            <div style={{ fontSize: 22, fontWeight: 700, color: COLORS.textPrimary }}>
              ④ 平台抽象层 PAL <span style={{ color: COLORS.amber, fontSize: 15, fontFamily: FONTS.mono }}>IPlatformBackend / IPlatformWindow</span>
            </div>
            <span style={{ fontSize: 14, color: COLORS.amber, fontWeight: 600 }}>W3C Pointer + IME</span>
          </div>
          <div style={{ fontSize: 16, color: COLORS.textSecondary, marginTop: 6 }}>
            Win32 · Linux (X11/Wayland) · Android (NDK) · Web (Emscripten) · Headless 无头测试
          </div>
        </div>
      </div>

      {/* Right: Live 16x16 Tile Binning & Dirty Tracking Visualizer */}
      <div
        style={{
          background: COLORS.bgCard,
          border: `1.5px solid ${activeStage === 1 ? COLORS.emerald : COLORS.border}`,
          borderRadius: 24,
          padding: 30,
          boxShadow: "0 20px 50px rgba(0,0,0,0.5)",
          display: "flex",
          flexDirection: "column",
          gap: 18,
        }}
      >
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
          <div>
            <div style={{ fontSize: 14, color: COLORS.emerald, fontWeight: 700, fontFamily: FONTS.mono }}>
              TILE BINNING &amp; DIRTY CULLING
            </div>
            <div style={{ fontSize: 24, color: COLORS.textPrimary, fontWeight: 700, marginTop: 4 }}>
              16×16 瓦片脏块剔除演示
            </div>
          </div>
          <div
            style={{
              padding: "6px 12px",
              borderRadius: 10,
              background: "rgba(16,185,129,0.16)",
              color: COLORS.emerald,
              fontFamily: FONTS.mono,
              fontSize: 15,
              fontWeight: 700,
            }}
          >
            256 threads/tile
          </div>
        </div>

        {/* 8x6 Tile Grid */}
        <div
          style={{
            display: "grid",
            gridTemplateColumns: "repeat(8, 1fr)",
            gap: 8,
            padding: 16,
            borderRadius: 16,
            background: "#070D19",
            border: "1px solid rgba(148,163,184,0.14)",
          }}
        >
          {Array.from({ length: 48 }).map((_, idx) => {
            const isDirty = activeStage >= 1 ? movingDirty.has(idx) : dirtyTileSet.has(idx);
            const glow = isDirty ? 0.85 + 0.15 * Math.sin(frame * 0.25) : 0;
            return (
              <div
                key={idx}
                style={{
                  aspectRatio: "1",
                  borderRadius: 8,
                  background: isDirty
                    ? `rgba(16, 185, 129, ${0.35 * glow})`
                    : "rgba(30, 41, 59, 0.55)",
                  border: isDirty
                    ? "1.5px solid #10B981"
                    : "1px solid rgba(148,163,184,0.1)",
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "center",
                  fontSize: 12,
                  fontFamily: FONTS.mono,
                  color: isDirty ? "#A7F3D0" : "rgba(100,116,139,0.45)",
                  boxShadow: isDirty ? "0 0 14px rgba(16,185,129,0.45)" : "none",
                }}
              >
                {isDirty ? "DIRTY" : "SKIP"}
              </div>
            );
          })}
        </div>

        {/* Pipeline stats */}
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 14 }}>
          <div
            style={{
              padding: 16,
              borderRadius: 14,
              background: "rgba(16,185,129,0.1)",
              border: "1px solid rgba(16,185,129,0.28)",
            }}
          >
            <div style={{ fontSize: 14, color: COLORS.textSecondary }}>局部交互跳过率</div>
            <div style={{ fontSize: 32, fontWeight: 800, color: COLORS.emerald, fontFamily: FONTS.mono }}>
              99.2%
            </div>
          </div>
          <div
            style={{
              padding: 16,
              borderRadius: 14,
              background: "rgba(56,189,248,0.1)",
              border: "1px solid rgba(56,189,248,0.28)",
            }}
          >
            <div style={{ fontSize: 14, color: COLORS.textSecondary }}>GPU HAL 纯虚接口</div>
            <div style={{ fontSize: 32, fontWeight: 800, color: COLORS.cyan, fontFamily: FONTS.mono }}>
              仅 6 个
            </div>
          </div>
        </div>

        {/* Flowing pulse bar */}
        <div style={{ fontSize: 14, color: COLORS.textMuted, fontFamily: FONTS.mono }}>
          Compute PSO → Offscreen RGBA8 UAV → Fullscreen Blit ({pulseOffset}%)
        </div>
      </div>
    </AbsoluteFill>
  );
};
