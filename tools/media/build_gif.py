# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Builds a GIF from the frames recorded by record.mjs: python3 build_gif.py FRAMES_DIR OUT.gif

FRAMES_DIR holds NNNN.png and durations.json (how long each frame stays, in ms). Identical frames in a row are
merged, the colours are reduced to one palette for the whole GIF and the loop is endless. Needs Pillow.
"""
import json
import os
import sys

from PIL import Image, ImageChops

MIN_FRAME_MS = 40       # browsers show anything shorter than 20 ms as 100 ms
COLORS = 192
SAMPLE_FRAMES = 24


def main(directory: str, out: str) -> None:
    names = sorted(n for n in os.listdir(directory) if n.endswith(".png"))
    durations = json.load(open(os.path.join(directory, "durations.json")))

    frames, times = [], []
    for name, duration in zip(names, durations):
        image = Image.open(os.path.join(directory, name)).convert("RGB")
        if frames and ImageChops.difference(frames[-1], image).getbbox() is None:
            times[-1] += duration
        else:
            frames.append(image)
            times.append(duration)

    # One palette for every frame: no flicker between them, and a smaller file. Built from frames spread over the whole
    # GIF, so a colour that only shows up in one scene (the preset colours) is not lost
    picked = frames[:: max(1, len(frames) // SAMPLE_FRAMES)][:SAMPLE_FRAMES]
    sample = Image.new("RGB", (frames[0].width, frames[0].height * len(picked)))
    for i, frame in enumerate(picked):
        sample.paste(frame, (0, i * frames[0].height))
    palette = sample.quantize(colors=COLORS, method=Image.Quantize.MAXCOVERAGE, dither=Image.Dither.NONE)
    frames = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]

    frames[0].save(out, save_all=True, append_images=frames[1:], duration=[max(MIN_FRAME_MS, t) for t in times],
                   loop=0, optimize=True, disposal=1)
    print(f"{out}: {len(frames)} frames, {sum(times) / 1000:.1f} s, {os.path.getsize(out) // 1024} KB")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
