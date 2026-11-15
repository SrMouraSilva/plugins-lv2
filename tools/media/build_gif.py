# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Builds an animated WebP and a GIF from the frames recorded by record.mjs: python3 build_gif.py FRAMES_DIR OUT.gif

FRAMES_DIR holds NNNN.png and durations.json (how long each frame stays, in ms). Identical frames in a row are
merged. Two files are written next to each other, the same animation twice:

  OUT.webp   the one to show: full colour, a fraction of the size of the GIF, every browser has played it for years
  OUT.gif    the fallback, for what cannot play WebP (an old browser, a viewer, an e-mail client): the colours are
             reduced to one palette for the whole animation

Both loop endlessly. Needs Pillow built with WebP (the wheels are). Use them together (see docs/making-animations.md):
<picture><source srcset="OUT.webp" type="image/webp"><img src="OUT.gif"></picture>
"""
import json
import os
import sys

from PIL import Image, ImageChops

MIN_FRAME_MS = 40       # browsers show anything shorter than 20 ms as 100 ms
COLORS = 192            # GIF
SAMPLE_FRAMES = 24      # GIF: frames the palette is built from
# Animated WebP is lossy here: lossless is 4 to 5 times larger than the GIF. Tried on the frames of two scenarios
# (GIF: 374 and 880 KB): quality 80 gives 466 and 725 KB; with minimize_size (the encoder picks, frame by frame, the
# smaller of a keyframe and a difference) 245 and 543 KB, the same quality, a few seconds slower. At 4x zoom it is
# indistinguishable from the frames (worst PSNR 34 dB, only some noise around text)
WEBP_QUALITY = 80


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

    durations_ms = [max(MIN_FRAME_MS, t) for t in times]

    webp = os.path.splitext(out)[0] + ".webp"
    frames[0].save(webp, save_all=True, append_images=frames[1:], duration=durations_ms, loop=0,
                   quality=WEBP_QUALITY, method=6, minimize_size=True)
    print(f"{webp}: {len(frames)} frames, {sum(times) / 1000:.1f} s, {os.path.getsize(webp) // 1024} KB")

    # One palette for every frame: no flicker between them, and a smaller file. Built from frames spread over the whole
    # GIF, so a colour that only shows up in one scene (the preset colours) is not lost
    picked = frames[:: max(1, len(frames) // SAMPLE_FRAMES)][:SAMPLE_FRAMES]
    sample = Image.new("RGB", (frames[0].width, frames[0].height * len(picked)))
    for i, frame in enumerate(picked):
        sample.paste(frame, (0, i * frames[0].height))
    palette = sample.quantize(colors=COLORS, method=Image.Quantize.MAXCOVERAGE, dither=Image.Dither.NONE)
    frames = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]

    frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations_ms,
                   loop=0, optimize=True, disposal=1)
    print(f"{out}: {len(frames)} frames, {sum(times) / 1000:.1f} s, {os.path.getsize(out) // 1024} KB")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
