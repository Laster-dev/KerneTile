import React from "react";
import { Composition } from "remotion";
import { KerneTileIntro } from "./KerneTileIntro";
import { KtuiShowcase, SHOWCASE_TIMELINE } from "./KtuiShowcase";
import { TIMELINE } from "./theme";

export const RemotionRoot: React.FC = () => {
  return (
    <>
      <Composition
        id="KerneTileIntro"
        component={KerneTileIntro}
        durationInFrames={TIMELINE.totalFrames}
        fps={TIMELINE.fps}
        width={1920}
        height={1080}
      />
      <Composition
        id="KtuiShowcase"
        component={KtuiShowcase}
        durationInFrames={SHOWCASE_TIMELINE.totalFrames}
        fps={SHOWCASE_TIMELINE.fps}
        width={1920}
        height={1080}
      />
    </>
  );
};
