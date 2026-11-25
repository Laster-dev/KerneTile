import React from "react";
import { AbsoluteFill, Audio, Sequence, staticFile } from "remotion";
import { Backdrop, SubtitleOverlay, TIMELINE } from "./theme";
import { IntroScene } from "./scenes/IntroScene";
import { ArchitectureScene } from "./scenes/ArchitectureScene";
import { CoreCodeScene } from "./scenes/CoreCodeScene";
import { DeclarativeUiScene } from "./scenes/DeclarativeUiScene";
import { TerminalScene } from "./scenes/TerminalScene";
import { OutroScene } from "./scenes/OutroScene";

const SCENE_COMPONENTS: Record<string, React.FC> = {
  intro: IntroScene,
  architecture: ArchitectureScene,
  core_code: CoreCodeScene,
  declarative_ui: DeclarativeUiScene,
  terminal_bench: TerminalScene,
  outro: OutroScene,
};

export const KerneTileIntro: React.FC = () => {
  return (
    <AbsoluteFill>
      {/* Shared animated tile grid backdrop + top chapter HUD */}
      <Backdrop />

      {/* 6 synchronized visual scenes */}
      {TIMELINE.scenes.map((scene) => {
        const Component = SCENE_COMPONENTS[scene.id];
        if (!Component) return null;
        return (
          <Sequence
            key={scene.id}
            from={scene.startFrame}
            durationInFrames={scene.durationFrames}
            name={scene.title}
          >
            <Component />
          </Sequence>
        );
      })}

      {/* 13 Edge-TTS voiceover clips precisely aligned to timeline */}
      {TIMELINE.clips.map((clip) => (
        <Sequence
          key={clip.index}
          from={clip.startFrame}
          durationInFrames={clip.durationFrames + 10}
          name={`Voice #${clip.index}`}
        >
          <Audio src={staticFile(clip.file)} />
        </Sequence>
      ))}

      {/* Synchronized bottom subtitles */}
      <SubtitleOverlay />
    </AbsoluteFill>
  );
};
