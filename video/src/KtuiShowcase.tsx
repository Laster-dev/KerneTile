import React from "react";
import {
  AbsoluteFill,
  Audio,
  OffthreadVideo,
  Sequence,
  interpolate,
  spring,
  staticFile,
  useCurrentFrame,
  useVideoConfig,
} from "remotion";
import showcaseData from "./showcase_timeline.json";
import clipDurationsData from "./clip_durations.json";
import { COLORS, FONTS } from "./theme";

interface ShowcaseScene {
  id: string;
  title: string;
  badge: string;
  clip: string;
  specs: string[];
  startFrame: number;
  durationFrames: number;
  duration: number;
}

interface ShowcaseClip {
  index: number;
  sceneId: string;
  file: string;
  text: string;
  startFrame: number;
  durationFrames: number;
}

interface ShowcaseSubtitle {
  id: number;
  sceneId: string;
  startFrame: number;
  endFrame: number;
  text: string;
}

export const SHOWCASE_TIMELINE = showcaseData as {
  voice: string;
  fps: number;
  totalDuration: number;
  totalFrames: number;
  scenes: ShowcaseScene[];
  clips: ShowcaseClip[];
  subtitles: ShowcaseSubtitle[];
};

const CLIP_DURATIONS: Record<string, number> = clipDurationsData;

const SceneFrame: React.FC<{ scene: ShowcaseScene; sceneIndex: number }> = ({
  scene,
  sceneIndex,
}) => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();

  const enter = spring({ frame, fps, config: { damping: 16, stiffness: 115 } });
  const videoScale = interpolate(enter, [0, 1], [0.965, 1]);
  const videoOpacity = interpolate(frame, [0, 10], [0, 1], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
  });

  const clipSec = CLIP_DURATIONS[scene.id] ?? 7.5;
  const playbackRate = Math.min(0.85, Math.max(0.3, (clipSec - 0.15) / Math.max(1, scene.duration)));

  return (
    <AbsoluteFill
      style={{
        padding: "86px 48px 118px",
        display: "grid",
        gridTemplateColumns: "1320px 1fr",
        gap: 32,
        alignItems: "center",
        fontFamily: FONTS.sans,
      }}
    >
      {/* Left: Live Captured Native ktui_gallery.exe Window */}
      <div
        style={{
          opacity: videoOpacity,
          transform: `scale(${videoScale})`,
          borderRadius: 20,
          overflow: "hidden",
          border: "1.5px solid rgba(56, 189, 248, 0.48)",
          boxShadow:
            "0 28px 70px rgba(0, 0, 0, 0.72), 0 0 42px rgba(56, 189, 248, 0.16)",
          background: "#0B1120",
          width: 1320,
          height: 836,
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
          position: "relative",
        }}
      >
        <OffthreadVideo
          src={staticFile(scene.clip)}
          muted
          playbackRate={playbackRate}
          style={{
            width: "100%",
            height: "100%",
            objectFit: "contain",
          }}
        />
        {/* Live capture indicator pill */}
        <div
          style={{
            position: "absolute",
            top: 14,
            right: 16,
            padding: "5px 12px",
            borderRadius: 999,
            background: "rgba(6, 10, 19, 0.78)",
            border: "1px solid rgba(16, 185, 129, 0.55)",
            color: COLORS.emerald,
            fontSize: 13,
            fontFamily: FONTS.mono,
            fontWeight: 700,
            display: "flex",
            alignItems: "center",
            gap: 8,
          }}
        >
          <span
            style={{
              width: 8,
              height: 8,
              borderRadius: "50%",
              background: COLORS.emerald,
              boxShadow: "0 0 8px #10B981",
            }}
          />
          LIVE NATIVE · D3D12 / C++20
        </div>
      </div>

      {/* Right: Fluent 2 Design Spec & Chapter Navigator */}
      <div
        style={{
          height: 836,
          display: "flex",
          flexDirection: "column",
          justifyContent: "space-between",
          gap: 20,
        }}
      >
        {/* Top Spec Card */}
        <div
          style={{
            background: COLORS.bgCard,
            border: `1.5px solid ${COLORS.borderActive}`,
            borderRadius: 20,
            padding: "26px 24px",
            boxShadow: "0 18px 40px rgba(0,0,0,0.45)",
          }}
        >
          <div
            style={{
              display: "inline-block",
              padding: "5px 14px",
              borderRadius: 999,
              background: "rgba(56, 189, 248, 0.16)",
              border: "1px solid rgba(56, 189, 248, 0.4)",
              color: COLORS.cyan,
              fontSize: 14,
              fontFamily: FONTS.mono,
              fontWeight: 700,
              marginBottom: 14,
            }}
          >
            {scene.badge}
          </div>
          <div
            style={{
              fontSize: 32,
              fontWeight: 800,
              color: COLORS.textPrimary,
              lineHeight: 1.2,
              marginBottom: 20,
            }}
          >
            0{sceneIndex + 1} · {scene.title}
          </div>

          <div style={{ display: "flex", flexDirection: "column", gap: 13 }}>
            {scene.specs.map((spec, idx) => {
              const sPop = spring({
                frame: frame - (10 + idx * 8),
                fps,
                config: { damping: 16, stiffness: 120 },
              });
              return (
                <div
                  key={spec}
                  style={{
                    opacity: sPop,
                    transform: `translateX(${interpolate(sPop, [0, 1], [18, 0])}px)`,
                    display: "flex",
                    alignItems: "flex-start",
                    gap: 10,
                    padding: "11px 14px",
                    borderRadius: 12,
                    background: "rgba(30, 41, 59, 0.6)",
                    border: "1px solid rgba(148, 163, 184, 0.14)",
                    fontSize: 17,
                    color: "#E2E8F0",
                    lineHeight: 1.4,
                  }}
                >
                  <span style={{ color: COLORS.emerald, fontWeight: 800 }}>✓</span>
                  <span>{spec}</span>
                </div>
              );
            })}
          </div>
        </div>

        {/* Bottom Chapter Index */}
        <div
          style={{
            flex: 1,
            background: COLORS.bgCard,
            border: `1px solid ${COLORS.border}`,
            borderRadius: 20,
            padding: "20px 22px",
            display: "flex",
            flexDirection: "column",
            justifyContent: "space-between",
          }}
        >
          <div
            style={{
              fontSize: 13,
              color: COLORS.textMuted,
              fontFamily: FONTS.mono,
              fontWeight: 700,
              letterSpacing: "0.08em",
              marginBottom: 8,
            }}
          >
            FLUENT 2 CONTROLS SHOWCASE
          </div>
          <div style={{ display: "flex", flexDirection: "column", gap: 7 }}>
            {SHOWCASE_TIMELINE.scenes.map((s, idx) => {
              const isCurrent = idx === sceneIndex;
              const isDone = idx < sceneIndex;
              return (
                <div
                  key={s.id}
                  style={{
                    display: "flex",
                    alignItems: "center",
                    justifyContent: "space-between",
                    padding: "8px 12px",
                    borderRadius: 10,
                    background: isCurrent
                      ? "linear-gradient(90deg, rgba(56,189,248,0.22), rgba(129,140,248,0.16))"
                      : "transparent",
                    border: isCurrent
                      ? "1px solid rgba(56,189,248,0.5)"
                      : "1px solid transparent",
                  }}
                >
                  <span
                    style={{
                      fontSize: 16,
                      fontWeight: isCurrent ? 700 : 500,
                      color: isCurrent
                        ? "#FFFFFF"
                        : isDone
                          ? COLORS.emerald
                          : COLORS.textSecondary,
                    }}
                  >
                    {isDone ? "✓ " : isCurrent ? "▶ " : `0${idx + 1} `}
                    {s.title}
                  </span>
                  <span
                    style={{
                      fontSize: 13,
                      fontFamily: FONTS.mono,
                      color: isCurrent ? COLORS.cyan : COLORS.textMuted,
                    }}
                  >
                    {s.badge.split("·")[0].trim()}
                  </span>
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </AbsoluteFill>
  );
};

export const KtuiShowcase: React.FC = () => {
  const frame = useCurrentFrame();
  const { durationInFrames } = useVideoConfig();
  const progress = interpolate(frame, [0, durationInFrames], [0, 100], {
    extrapolateRight: "clamp",
  });

  const activeSub = SHOWCASE_TIMELINE.subtitles.find(
    (s) => frame >= s.startFrame && frame <= s.endFrame + 4
  );

  return (
    <AbsoluteFill style={{ backgroundColor: COLORS.bg, fontFamily: FONTS.sans }}>
      {/* Ambient background */}
      <div
        style={{
          position: "absolute",
          inset: 0,
          background:
            "radial-gradient(circle at 20% 15%, rgba(56,189,248,0.12), transparent 50%), radial-gradient(circle at 85% 85%, rgba(129,140,248,0.12), transparent 50%)",
        }}
      />

      {/* Top Progress Bar */}
      <div
        style={{
          position: "absolute",
          top: 0,
          left: 0,
          right: 0,
          height: 4,
          background: "rgba(148,163,184,0.14)",
          zIndex: 50,
        }}
      >
        <div
          style={{
            height: "100%",
            width: `${progress}%`,
            background: "linear-gradient(90deg, #38BDF8, #818CF8, #10B981)",
          }}
        />
      </div>

      {/* Top Header Bar */}
      <div
        style={{
          position: "absolute",
          top: 22,
          left: 48,
          right: 48,
          display: "flex",
          alignItems: "center",
          justifyContent: "space-between",
          zIndex: 40,
        }}
      >
        <div style={{ display: "flex", alignItems: "center", gap: 14 }}>
          <div
            style={{
              width: 30,
              height: 30,
              borderRadius: 8,
              background: "linear-gradient(135deg, #38BDF8, #6366F1)",
              display: "grid",
              gridTemplateColumns: "1fr 1fr",
              gap: 3,
              padding: 5,
            }}
          >
            <div style={{ borderRadius: 2, background: "#fff" }} />
            <div style={{ borderRadius: 2, background: "rgba(255,255,255,0.55)" }} />
            <div style={{ borderRadius: 2, background: "rgba(255,255,255,0.55)" }} />
            <div style={{ borderRadius: 2, background: "#fff" }} />
          </div>
          <span style={{ fontSize: 22, fontWeight: 800, color: COLORS.textPrimary }}>
            KTUI Gallery <span style={{ color: COLORS.cyan }}>实机演示</span>
          </span>
          <span
            style={{
              fontSize: 15,
              color: COLORS.textSecondary,
              paddingLeft: 12,
              borderLeft: "1px solid rgba(148,163,184,0.28)",
            }}
          >
            Fluent 2 / WinUI 3 设计规范 · 单文件 973 KB 原生运行实录
          </span>
        </div>

        <div
          style={{
            fontFamily: FONTS.mono,
            fontSize: 15,
            color: COLORS.cyan,
            padding: "6px 14px",
            borderRadius: 999,
            background: "rgba(15, 23, 42, 0.8)",
            border: "1px solid rgba(56, 189, 248, 0.35)",
          }}
        >
          ktui_gallery.exe · 60+ Fluent 2 Controls
        </div>
      </div>

      {/* 8 Live Capture Scenes */}
      {SHOWCASE_TIMELINE.scenes.map((scene, idx) => (
        <Sequence
          key={scene.id}
          from={scene.startFrame}
          durationInFrames={scene.durationFrames}
          name={scene.title}
        >
          <SceneFrame scene={scene} sceneIndex={idx} />
        </Sequence>
      ))}

      {/* Edge-TTS Voiceover Clips */}
      {SHOWCASE_TIMELINE.clips.map((clip) => (
        <Sequence
          key={clip.index}
          from={clip.startFrame}
          durationInFrames={clip.durationFrames + 10}
          name={`Showcase Voice #${clip.index}`}
        >
          <Audio src={staticFile(clip.file)} />
        </Sequence>
      ))}

      {/* Synchronized Subtitles */}
      {activeSub && (
        <div
          style={{
            position: "absolute",
            bottom: 24,
            left: 80,
            right: 80,
            display: "flex",
            justifyContent: "center",
            zIndex: 60,
          }}
        >
          <div
            style={{
              maxWidth: 1520,
              padding: "12px 30px",
              borderRadius: 14,
              background: "rgba(6, 10, 19, 0.88)",
              border: "1px solid rgba(56, 189, 248, 0.32)",
              boxShadow: "0 10px 28px rgba(0,0,0,0.55)",
              color: "#F8FAFC",
              fontSize: 28,
              fontWeight: 600,
              lineHeight: 1.4,
              textAlign: "center",
            }}
          >
            {activeSub.text}
          </div>
        </div>
      )}
    </AbsoluteFill>
  );
};
