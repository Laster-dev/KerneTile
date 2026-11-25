import React from "react";
import { AbsoluteFill, interpolate, useCurrentFrame, useVideoConfig } from "remotion";
import timelineData from "./timeline.json";

export const COLORS = {
  bg: "#060A13",
  bgCard: "rgba(15, 23, 42, 0.78)",
  bgCardSolid: "#0F172A",
  border: "rgba(148, 163, 184, 0.18)",
  borderActive: "rgba(56, 189, 248, 0.65)",
  cyan: "#38BDF8",
  blue: "#3B82F6",
  indigo: "#818CF8",
  emerald: "#10B981",
  amber: "#F59E0B",
  rose: "#FB7185",
  textPrimary: "#F8FAFC",
  textSecondary: "#94A3B8",
  textMuted: "#64748B",
  codeBg: "#0B1120",
};

export const FONTS = {
  sans: '"Microsoft YaHei UI", "PingFang SC", "Segoe UI", system-ui, -apple-system, sans-serif',
  mono: '"Cascadia Code", "JetBrains Mono", "Fira Code", Consolas, monospace',
};

export interface TimelineScene {
  id: string;
  title: string;
  start: number;
  end: number;
  duration: number;
  startFrame: number;
  durationFrames: number;
}

export interface TimelineClip {
  index: number;
  sceneId: string;
  file: string;
  text: string;
  start: number;
  end: number;
  startFrame: number;
  durationFrames: number;
  duration: number;
}

export interface TimelineSubtitle {
  id: number;
  sceneId: string;
  start: number;
  end: number;
  startFrame: number;
  endFrame: number;
  text: string;
}

export const TIMELINE = timelineData as {
  voice: string;
  rate: string;
  fps: number;
  totalDuration: number;
  totalFrames: number;
  scenes: TimelineScene[];
  clips: TimelineClip[];
  subtitles: TimelineSubtitle[];
};

export const Backdrop: React.FC = () => {
  const frame = useCurrentFrame();
  const { durationInFrames } = useVideoConfig();

  const activeSceneIndex = TIMELINE.scenes.findIndex(
    (s) => frame >= s.startFrame && frame < s.startFrame + s.durationFrames
  );
  const currentScene =
    TIMELINE.scenes[activeSceneIndex >= 0 ? activeSceneIndex : TIMELINE.scenes.length - 1];
  const progress = interpolate(frame, [0, durationInFrames], [0, 100], {
    extrapolateRight: "clamp",
  });

  const driftX = (frame * 0.35) % 64;
  const driftY = (frame * 0.2) % 64;

  return (
    <AbsoluteFill style={{ backgroundColor: COLORS.bg, overflow: "hidden", fontFamily: FONTS.sans }}>
      {/* Ambient glows */}
      <div
        style={{
          position: "absolute",
          top: -260,
          left: -180,
          width: 900,
          height: 900,
          borderRadius: "50%",
          background:
            "radial-gradient(circle, rgba(56,189,248,0.14) 0%, rgba(56,189,248,0) 68%)",
        }}
      />
      <div
        style={{
          position: "absolute",
          bottom: -300,
          right: -180,
          width: 960,
          height: 960,
          borderRadius: "50%",
          background:
            "radial-gradient(circle, rgba(129,140,248,0.14) 0%, rgba(129,140,248,0) 68%)",
        }}
      />

      {/* Subtle 16x16 inspired tile grid */}
      <div
        style={{
          position: "absolute",
          inset: -64,
          transform: `translate(${driftX}px, ${driftY}px)`,
          backgroundImage:
            "linear-gradient(to right, rgba(148,163,184,0.055) 1px, transparent 1px), linear-gradient(to bottom, rgba(148,163,184,0.055) 1px, transparent 1px)",
          backgroundSize: "64px 64px",
        }}
      />

      {/* Top HUD Bar */}
      <div
        style={{
          position: "absolute",
          top: 28,
          left: 56,
          right: 56,
          display: "flex",
          alignItems: "center",
          justifyContent: "space-between",
          zIndex: 40,
        }}
      >
        <div style={{ display: "flex", alignItems: "center", gap: 14 }}>
          <div
            style={{
              width: 34,
              height: 34,
              borderRadius: 9,
              background: "linear-gradient(135deg, #38BDF8 0%, #6366F1 100%)",
              display: "grid",
              gridTemplateColumns: "1fr 1fr",
              gap: 3,
              padding: 6,
              boxShadow: "0 0 20px rgba(56,189,248,0.45)",
            }}
          >
            <div style={{ borderRadius: 2, background: "#fff" }} />
            <div style={{ borderRadius: 2, background: "rgba(255,255,255,0.55)" }} />
            <div style={{ borderRadius: 2, background: "rgba(255,255,255,0.55)" }} />
            <div style={{ borderRadius: 2, background: "#fff" }} />
          </div>
          <span
            style={{
              color: COLORS.textPrimary,
              fontWeight: 700,
              fontSize: 22,
              letterSpacing: "0.02em",
            }}
          >
            KerneTile <span style={{ color: COLORS.cyan }}>&amp; KTUI</span>
          </span>
        </div>

        {/* Chapter Pills */}
        <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
          {TIMELINE.scenes.map((scene, idx) => {
            const active = scene.id === currentScene.id;
            const passed = idx < activeSceneIndex;
            return (
              <div
                key={scene.id}
                style={{
                  padding: "6px 14px",
                  borderRadius: 999,
                  fontSize: 15,
                  fontWeight: active ? 700 : 500,
                  color: active
                    ? "#060A13"
                    : passed
                      ? COLORS.cyan
                      : COLORS.textSecondary,
                  background: active
                    ? "linear-gradient(90deg, #38BDF8, #818CF8)"
                    : "rgba(15, 23, 42, 0.72)",
                  border: `1px solid ${active ? "transparent" : passed ? "rgba(56,189,248,0.35)" : COLORS.border}`,
                }}
              >
                0{idx + 1} · {scene.title}
              </div>
            );
          })}
        </div>
      </div>

      {/* Top thin progress bar */}
      <div
        style={{
          position: "absolute",
          top: 0,
          left: 0,
          right: 0,
          height: 4,
          background: "rgba(148,163,184,0.12)",
          zIndex: 50,
        }}
      >
        <div
          style={{
            height: "100%",
            width: `${progress}%`,
            background: "linear-gradient(90deg, #38BDF8, #818CF8, #10B981)",
            boxShadow: "0 0 12px rgba(56,189,248,0.7)",
          }}
        />
      </div>
    </AbsoluteFill>
  );
};

export const SubtitleOverlay: React.FC = () => {
  const frame = useCurrentFrame();
  const active = TIMELINE.subtitles.find(
    (sub) => frame >= sub.startFrame && frame <= sub.endFrame + 4
  );

  if (!active) return null;

  const fadeIn = interpolate(frame, [active.startFrame, active.startFrame + 6], [0, 1], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
  });
  const fadeOut = interpolate(frame, [active.endFrame - 4, active.endFrame + 4], [1, 0], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
  });
  const opacity = Math.min(fadeIn, fadeOut);
  const translateY = interpolate(frame, [active.startFrame, active.startFrame + 6], [8, 0], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
  });

  return (
    <div
      style={{
        position: "absolute",
        bottom: 34,
        left: 96,
        right: 96,
        display: "flex",
        justifyContent: "center",
        pointerEvents: "none",
        zIndex: 60,
        opacity,
        transform: `translateY(${translateY}px)`,
      }}
    >
      <div
        style={{
          maxWidth: 1460,
          padding: "14px 32px",
          borderRadius: 16,
          background: "rgba(6, 10, 19, 0.84)",
          border: "1px solid rgba(56, 189, 248, 0.28)",
          boxShadow: "0 12px 32px rgba(0, 0, 0, 0.55)",
          color: "#F8FAFC",
          fontSize: 30,
          fontWeight: 600,
          lineHeight: 1.45,
          textAlign: "center",
          fontFamily: FONTS.sans,
          letterSpacing: "0.015em",
        }}
      >
        {active.text}
      </div>
    </div>
  );
};
