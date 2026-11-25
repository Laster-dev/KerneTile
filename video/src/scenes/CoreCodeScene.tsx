import React from "react";
import { AbsoluteFill, interpolate, spring, useCurrentFrame, useVideoConfig } from "remotion";
import { COLORS, FONTS } from "../theme";

interface CodeSnippet {
  file: string;
  tag: string;
  accent: string;
  lines: { text: string; highlight?: boolean }[];
}

const SNIPPETS: CodeSnippet[] = [
  {
    file: "include/kernetile/core/primitives.hpp",
    tag: "80 字节精确二进制镜像",
    accent: COLORS.cyan,
    lines: [
      { text: "struct alignas(16) UiPrimitive {" },
      { text: "    float min_x, min_y, max_x, max_y;      // [00-15] 物理像素包围盒", highlight: true },
      { text: "    float radius_tl, radius_tr, br, bl;    // [16-31] 四角独立圆角" },
      { text: "    float color_r, color_g, color_b, a;    // [32-47] 线性光预乘 RGBA" },
      { text: "    float border_width, shadow_blur, ...;  // [48-63] SDF 描边与阴影", highlight: true },
      { text: "    uint16_t u0, v0, u1, v1, flags, clip;  // [64-79] 图集 UV 与裁剪" },
      { text: "};" },
      { text: "" },
      { text: 'static_assert(sizeof(UiPrimitive) == 80, "80 bytes exact");', highlight: true },
      { text: 'static_assert(alignof(UiPrimitive) == 16, "16B aligned");' },
    ],
  },
  {
    file: "shaders/kerne_sdf.slang",
    tag: "无分支 2D 圆角矩形解析 SDF",
    accent: COLORS.indigo,
    lines: [
      { text: "// Inigo Quilez 2D 精确圆角矩形 SDF (单源码多后端)" },
      { text: "float sdRoundedBox(float2 p, float2 center, float2 half_size, float4 radii) {" },
      { text: "    float2 s = p - center;" },
      { text: "    float r = (s.x >= 0.0) ? ((s.y >= 0.0) ? radii.z : radii.y)", highlight: true },
      { text: "                           : ((s.y >= 0.0) ? radii.w : radii.x);" },
      { text: "    r = min(r, min(half_size.x, half_size.y));" },
      { text: "    float2 q = abs(s) - (half_size - float2(r, r));", highlight: true },
      { text: "    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;", highlight: true },
      { text: "}" },
      { text: "float evaluateCoverage(float d) { return saturate(0.5 - d); }" },
    ],
  },
  {
    file: "shaders/kerne_shadow.slang",
    tag: "单 Pass 四阶 erf 解析高斯阴影",
    accent: COLORS.emerald,
    lines: [
      { text: "// Abramowitz & Stegun 4阶误差函数逼近 (误差仅 0.21 LSB)" },
      { text: "float erfApprox(float x) {" },
      { text: "    float ax = abs(x);" },
      { text: "    float p = 1.0 + 0.278393*ax + 0.230389*ax*ax + 0.078108*ax*ax*ax*ax;", highlight: true },
      { text: "    float res = 1.0 - 1.0 / (p * p * p * p);" },
      { text: "    return (x >= 0.0) ? res : -res;" },
      { text: "}" },
      { text: "// 单遍解析外积积分，零离屏模糊开销" },
      { text: "float fx = gaussianCDF(box_max.x - p.x, sigma) - gaussianCDF(box_min.x - p.x, sigma);", highlight: true },
      { text: "float fy = gaussianCDF(box_max.y - p.y, sigma) - gaussianCDF(box_min.y - p.y, sigma);", highlight: true },
    ],
  },
];

const MEMORY_BLOCKS = [
  { range: "00–15", name: "BBox (min/max)", color: COLORS.cyan },
  { range: "16–31", name: "4× Corner Radii", color: COLORS.blue },
  { range: "32–47", name: "Linear Premul RGBA", color: COLORS.indigo },
  { range: "48–63", name: "SDF Border & Shadow", color: COLORS.emerald },
  { range: "64–79", name: "Atlas UV + ClipIdx", color: COLORS.amber },
];

export const CoreCodeScene: React.FC = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();

  // Synced to the 3 lines in core_code (~24s = 724 frames)
  const partIdx = frame < 230 ? 0 : frame < 475 ? 1 : 2;
  const snippet = SNIPPETS[partIdx];
  const localFrame = partIdx === 0 ? frame : partIdx === 1 ? frame - 230 : frame - 475;

  const panelSpring = spring({ frame: localFrame, fps, config: { damping: 16, stiffness: 120 } });

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
      {/* Left: VS Code Style Syntax Highlighted Editor */}
      <div
        style={{
          background: COLORS.codeBg,
          border: `1.5px solid ${snippet.accent}`,
          borderRadius: 20,
          overflow: "hidden",
          boxShadow: `0 20px 60px rgba(0,0,0,0.55), 0 0 32px ${snippet.accent}25`,
        }}
      >
        {/* Window Titlebar + Tabs */}
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
          <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#FB7185" }} />
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#FBBF24" }} />
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#34D399" }} />
            <span
              style={{
                marginLeft: 14,
                fontFamily: FONTS.mono,
                fontSize: 17,
                color: COLORS.textPrimary,
                fontWeight: 600,
              }}
            >
              {snippet.file}
            </span>
          </div>
          <div
            style={{
              padding: "4px 12px",
              borderRadius: 999,
              background: `${snippet.accent}22`,
              color: snippet.accent,
              fontSize: 14,
              fontWeight: 700,
            }}
          >
            {snippet.tag}
          </div>
        </div>

        {/* Code Lines */}
        <div style={{ padding: "24px 20px", fontFamily: FONTS.mono, fontSize: 18.5, lineHeight: 1.75 }}>
          {snippet.lines.map((line, i) => {
            const lineOpacity = interpolate(localFrame, [i * 2.5, i * 2.5 + 10], [0, 1], {
              extrapolateLeft: "clamp",
              extrapolateRight: "clamp",
            });
            const isComment = line.text.trim().startsWith("//");
            return (
              <div
                key={`${partIdx}-${i}`}
                style={{
                  opacity: lineOpacity,
                  display: "flex",
                  gap: 18,
                  padding: "3px 14px",
                  borderRadius: 8,
                  background: line.highlight ? `${snippet.accent}1C` : "transparent",
                  borderLeft: line.highlight ? `3px solid ${snippet.accent}` : "3px solid transparent",
                  color: isComment
                    ? "#64748B"
                    : line.highlight
                      ? "#FFFFFF"
                      : "#CBD5E1",
                  whiteSpace: "pre",
                }}
              >
                <span style={{ color: "#475569", width: 26, textAlign: "right", userSelect: "none" }}>
                  {i + 1}
                </span>
                <span>{line.text}</span>
              </div>
            );
          })}
        </div>

        {/* Tab indicator footer */}
        <div
          style={{
            display: "flex",
            gap: 10,
            padding: "14px 22px",
            background: "#0D1424",
            borderTop: "1px solid rgba(148,163,184,0.12)",
          }}
        >
          {SNIPPETS.map((s, idx) => (
            <div
              key={s.file}
              style={{
                flex: 1,
                padding: "8px 12px",
                borderRadius: 10,
                fontSize: 14,
                fontWeight: idx === partIdx ? 700 : 500,
                textAlign: "center",
                color: idx === partIdx ? "#060A13" : COLORS.textSecondary,
                background: idx === partIdx ? s.accent : "rgba(30,41,59,0.5)",
              }}
            >
              {idx + 1}. {s.tag}
            </div>
          ))}
        </div>
      </div>

      {/* Right: Synchronized Visual Explainer */}
      <div
        style={{
          background: COLORS.bgCard,
          border: `1.5px solid ${COLORS.border}`,
          borderRadius: 24,
          padding: 32,
          height: 580,
          display: "flex",
          flexDirection: "column",
          justifyContent: "space-between",
          transform: `scale(${interpolate(panelSpring, [0, 1], [0.96, 1])})`,
          boxShadow: "0 20px 50px rgba(0,0,0,0.45)",
        }}
      >
        {partIdx === 0 && (
          <>
            <div>
              <div style={{ fontSize: 15, color: COLORS.cyan, fontFamily: FONTS.mono, fontWeight: 700 }}>
                MEMORY LAYOUT · 80 BYTES
              </div>
              <div style={{ fontSize: 28, color: COLORS.textPrimary, fontWeight: 800, marginTop: 6 }}>
                CPU / GPU 二进制精确镜像
              </div>
            </div>
            <div style={{ display: "flex", flexDirection: "column", gap: 14 }}>
              {MEMORY_BLOCKS.map((blk, i) => {
                const w = interpolate(localFrame, [8 + i * 7, 26 + i * 7], [0, 100], {
                  extrapolateLeft: "clamp",
                  extrapolateRight: "clamp",
                });
                return (
                  <div key={blk.range}>
                    <div style={{ display: "flex", justifyContent: "space-between", fontSize: 16, marginBottom: 6 }}>
                      <span style={{ color: blk.color, fontFamily: FONTS.mono, fontWeight: 700 }}>
                        [{blk.range} B]
                      </span>
                      <span style={{ color: COLORS.textPrimary, fontWeight: 600 }}>{blk.name}</span>
                    </div>
                    <div style={{ height: 14, borderRadius: 7, background: "rgba(30,41,59,0.8)", overflow: "hidden" }}>
                      <div style={{ width: `${w}%`, height: "100%", background: blk.color, borderRadius: 7 }} />
                    </div>
                  </div>
                );
              })}
            </div>
            <div style={{ fontSize: 16, color: COLORS.textSecondary }}>
              16 字节对齐直接映射至 GPU StructuredBuffer，零序列化开销。
            </div>
          </>
        )}

        {partIdx === 1 && (
          <>
            <div>
              <div style={{ fontSize: 15, color: COLORS.indigo, fontFamily: FONTS.mono, fontWeight: 700 }}>
                ANALYTICAL 2D SDF
              </div>
              <div style={{ fontSize: 28, color: COLORS.textPrimary, fontWeight: 800, marginTop: 6 }}>
                无三角网格 · 任意缩放无锯齿
              </div>
            </div>
            <div
              style={{
                flex: 1,
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
                position: "relative",
              }}
            >
              {[1.45, 1.3, 1.15, 1.0].map((scale, idx) => (
                <div
                  key={scale}
                  style={{
                    position: "absolute",
                    width: 240 * scale,
                    height: 150 * scale,
                    borderRadius: 34 * scale,
                    border:
                      idx === 3
                        ? "3px solid #38BDF8"
                        : `1.5px dashed rgba(129, 140, 248, ${0.22 + idx * 0.18})`,
                    background:
                      idx === 3
                        ? "linear-gradient(135deg, rgba(56,189,248,0.28), rgba(129,140,248,0.28))"
                        : "transparent",
                    transform: `scale(${0.92 + 0.08 * Math.sin(localFrame * 0.06)})`,
                  }}
                />
              ))}
              <div
                style={{
                  position: "relative",
                  fontFamily: FONTS.mono,
                  fontSize: 22,
                  fontWeight: 700,
                  color: "#FFFFFF",
                }}
              >
                d(p) = 0.00 px
              </div>
            </div>
            <div style={{ fontSize: 16, color: COLORS.textSecondary }}>
              400% DPI 缩放依然逐像素解析求值，彻底消除多边形折线锯齿。
            </div>
          </>
        )}

        {partIdx === 2 && (
          <>
            <div>
              <div style={{ fontSize: 15, color: COLORS.emerald, fontFamily: FONTS.mono, fontWeight: 700 }}>
                SINGLE-PASS GAUSSIAN SHADOW
              </div>
              <div style={{ fontSize: 28, color: COLORS.textPrimary, fontWeight: 800, marginTop: 6 }}>
                单遍解析高斯软阴影
              </div>
            </div>
            <div
              style={{
                flex: 1,
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
              }}
            >
              {(() => {
                const blur = 26 + 18 * Math.sin(localFrame * 0.07);
                return (
                  <div
                    style={{
                      width: 260,
                      height: 150,
                      borderRadius: 26,
                      background: "linear-gradient(135deg, #1E293B, #0F172A)",
                      border: "1.5px solid rgba(56,189,248,0.5)",
                      boxShadow: `0 18px ${blur}px rgba(16, 185, 129, 0.55)`,
                      display: "flex",
                      flexDirection: "column",
                      alignItems: "center",
                      justifyContent: "center",
                      gap: 8,
                    }}
                  >
                    <div style={{ fontFamily: FONTS.mono, fontSize: 22, fontWeight: 700, color: COLORS.emerald }}>
                      σ = {(blur / 2).toFixed(1)} px
                    </div>
                    <div style={{ fontSize: 15, color: COLORS.textSecondary }}>
                      误差 ≈ 0.21 LSB · 0 离屏缓冲
                    </div>
                  </div>
                );
              })()}
            </div>
            <div style={{ fontSize: 16, color: COLORS.textSecondary }}>
              无需多遍高斯模糊 RenderTarget，常数时间 O(1) 渲染平滑圆角阴影。
            </div>
          </>
        )}
      </div>
    </AbsoluteFill>
  );
};
