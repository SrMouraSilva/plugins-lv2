#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Makes the white background of a picture transparent (a screenshot of a pedalboard, as the MOD pedalboards site gives it), so the manual
can put its own background behind it (the dark, dotted one of the interactive section, see .media.link in tools/manual/manual.css).

    python3 tools/media/white_to_alpha.py IN.jpg OUT.webp [--tolerance 9]

Only the white that is connected to the border of the picture goes: a white pedal inside it stays. Along the edge of what is kept, the
white that the anti-aliasing mixed into a pixel is taken out again (a soft edge, with no white fringe on a dark background).
Needs `pip install pillow`.
"""
import argparse
from collections import deque

from PIL import Image, ImageChops


def main():
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("source")
    parser.add_argument("output")
    parser.add_argument("--tolerance", type=int, default=9, help="how far from pure white still counts as background (0-255)")
    parser.add_argument("--pocket", type=int, default=0, help="also remove a patch of white that touches no border when it has this many pixels (0: never). It also eats the pale parts of a pale pedal: try it only on a dark pedalboard")
    parser.add_argument("--fill", type=float, default=0.5, help="a pocket goes only when it fills less than this share of its box (a pedal face is a block)")
    args = parser.parse_args()

    image = Image.open(args.source).convert("RGB")
    width, height = image.size
    red, green, blue = image.split()
    whiteness = ImageChops.darker(ImageChops.darker(red, green), blue).load()   # the darkest channel: 255 only for pure white
    limit = 255 - args.tolerance

    # The background: near-white pixels that touch the border, and the pockets of near-white that cables or pedals enclose (a few pixels
    # of white inside a pedal stay). A component is a set of connected near-white pixels
    background = bytearray(width * height)
    seen = bytearray(width * height)
    for start in range(width * height):
        if seen[start] or whiteness[start % width, start // width] < limit:
            continue
        component, touches, queue = [], False, deque([start])
        left, top, right, bottom = width, height, 0, 0
        seen[start] = 1
        while queue:
            index = queue.popleft()
            component.append(index)
            x, y = index % width, index // width
            touches = touches or x in (0, width - 1) or y in (0, height - 1)
            left, top, right, bottom = min(left, x), min(top, y), max(right, x), max(bottom, y)
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < width and 0 <= ny < height:
                    neighbour = ny * width + nx
                    if not seen[neighbour] and whiteness[nx, ny] >= limit:
                        seen[neighbour] = 1
                        queue.append(neighbour)
        # A pocket between cables is a thin sliver that fills little of its box; the white face of a pedal is a block that fills most of it
        fill = len(component) / ((right - left + 1) * (bottom - top + 1))
        if touches or (args.pocket and len(component) >= args.pocket and fill < args.fill):
            for index in component:
                background[index] = 1

    pixels = image.load()
    out = Image.new("RGBA", image.size)
    put = out.load()
    for y in range(height):
        for x in range(width):
            if background[y * width + x]:
                put[x, y] = (0, 0, 0, 0)
                continue
            r, g, b = pixels[x, y]
            near = any(
                0 <= x + dx < width and 0 <= y + dy < height and background[(y + dy) * width + x + dx]
                for dx in (-2, -1, 0, 1, 2) for dy in (-2, -1, 0, 1, 2)
            )
            if not near:
                put[x, y] = (r, g, b, 255)
                continue
            # An edge pixel: some of its white is the background showing through. alpha from how far it is from white,
            # then the white taken out of the colour: c = (c' - 255 * (1 - alpha)) / alpha
            alpha = min(1.0, (255 - whiteness[x, y]) / 110.0)
            alpha = max(alpha, 0.04)
            channel = lambda c: max(0, min(255, round((c - 255 * (1 - alpha)) / alpha)))
            put[x, y] = (channel(r), channel(g), channel(b), round(alpha * 255))
    out.save(args.output, "WEBP", quality=90, method=6)
    print(f"{args.output}: {width}x{height}")


if __name__ == "__main__":
    main()
