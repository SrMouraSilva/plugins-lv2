/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A 1 bit canvas for the display widgets (the `ahw:Mono1` format of hmi/alabs-hmi-widgets.h): rows from the top, a row
 * is (width + 7) / 8 bytes, the most significant bit of a byte is the leftmost pixel, a set bit is a lit pixel.
 *
 * Header only and `static`, so each plugin gets its own copy and nothing is shared between the plugins at run time. No
 * allocation: the canvas is a struct on the stack, as big as the biggest display of a MOD device (128 x 64).
 */

#ifndef HMI_WIDGET_CANVAS_H
#define HMI_WIDGET_CANVAS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "widget-font.h"

#define CANVAS_MAX_WIDTH  128
#define CANVAS_MAX_HEIGHT 64

/** The text styles: the 5 x 7 font (6 px a character), the 3 x 5 one (4 px a character) */
typedef enum {
    FONT_STYLE_BIG = 0,
    FONT_STYLE_SMALL
} FontStyle;

typedef struct {
    uint16_t width;
    uint16_t height;
    /** Bytes in a row: what draw_bitmap expects for this width */
    uint16_t stride;
    uint8_t  bits[((CANVAS_MAX_WIDTH + 7) / 8) * CANVAS_MAX_HEIGHT];
} Canvas;

/** An empty canvas of this size (clamped to the biggest display) */
static inline void canvas_init(Canvas* canvas, unsigned int width, unsigned int height) {
    canvas->width = (uint16_t) (width > CANVAS_MAX_WIDTH ? CANVAS_MAX_WIDTH : width);
    canvas->height = (uint16_t) (height > CANVAS_MAX_HEIGHT ? CANVAS_MAX_HEIGHT : height);
    canvas->stride = (uint16_t) ((canvas->width + 7) / 8);
    memset(canvas->bits, 0, (size_t) canvas->stride * canvas->height);
}

static inline void canvas_pixel(Canvas* canvas, int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= canvas->width || y >= canvas->height) {
        return;
    }

    uint8_t* byte = &canvas->bits[y * canvas->stride + x / 8];
    const uint8_t mask = (uint8_t) (0x80u >> (x % 8));

    if (on) {
        *byte |= mask;
    } else {
        *byte &= (uint8_t) ~mask;
    }
}

static inline bool canvas_get(const Canvas* canvas, int x, int y) {
    if (x < 0 || y < 0 || x >= canvas->width || y >= canvas->height) {
        return false;
    }

    return (canvas->bits[y * canvas->stride + x / 8] & (0x80u >> (x % 8))) != 0;
}

/** A filled rectangle, lit (on) or dark */
static inline void canvas_fill(Canvas* canvas, int x, int y, int width, int height, bool on) {
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            canvas_pixel(canvas, x + i, y + j, on);
        }
    }
}

/** Half of the pixels lit, in a chessboard: the "half way" of a three level step, and a grey for a dim area */
static inline void canvas_checker(Canvas* canvas, int x, int y, int width, int height) {
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            canvas_pixel(canvas, x + i, y + j, ((x + i + y + j) & 1) == 0);
        }
    }
}

/** The outline of a rectangle, one pixel thick */
static inline void canvas_frame(Canvas* canvas, int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    canvas_fill(canvas, x, y, width, 1, true);
    canvas_fill(canvas, x, y + height - 1, width, 1, true);
    canvas_fill(canvas, x, y, 1, height, true);
    canvas_fill(canvas, x + width - 1, y, 1, height, true);
}

/** Flip every pixel of a rectangle: a label on a lit bar, a selected row */
static inline void canvas_invert(Canvas* canvas, int x, int y, int width, int height) {
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            canvas_pixel(canvas, x + i, y + j, !canvas_get(canvas, x + i, y + j));
        }
    }
}

/** A line of dots, one lit every `period` pixels */
static inline void canvas_dotted_hline(Canvas* canvas, int x, int y, int width, int period) {
    for (int i = 0; i < width; i += period) {
        canvas_pixel(canvas, x + i, y, true);
    }
}

// -------------------------------------------------------------------------------------------------------------------
// Text

static inline int canvas_char_width(FontStyle style) {
    return style == FONT_STYLE_BIG ? 6 : 4;
}

/** The height of a line of text of this style, without the descender row of the big font */
static inline int canvas_font_height(FontStyle style) {
    return style == FONT_STYLE_BIG ? 7 : 5;
}

/** The width in pixels of `count` characters of the text, without the empty column after the last one */
static inline int canvas_text_width(FontStyle style, int count) {
    return count <= 0 ? 0 : count * canvas_char_width(style) - 1;
}

static inline void canvas_glyph_big(Canvas* canvas, int x, int y, char character, bool on) {
    if (character < FONT_BIG_FIRST || character > FONT_BIG_LAST) {
        character = '?';
    }

    const uint8_t* columns = &FONT_BIG[(character - FONT_BIG_FIRST) * 5];

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (columns[i] & (1u << j)) {
                canvas_pixel(canvas, x + i, y + j, on);
            }
        }
    }
}

static inline void canvas_glyph_small(Canvas* canvas, int x, int y, char character, bool on) {
    if (character >= 'a' && character <= 'z') {
        character = (char) (character - 'a' + 'A');
    }

    for (int g = 0; g < FONT_SMALL_COUNT; ++g) {
        if (FONT_SMALL[g].character != character) {
            continue;
        }

        const uint16_t rows = FONT_SMALL[g].rows;

        for (int j = 0; j < 5; ++j) {
            for (int i = 0; i < 3; ++i) {
                if (rows & (1u << (14 - (j * 3 + i)))) {
                    canvas_pixel(canvas, x + i, y + j, on);
                }
            }
        }
        return;
    }
}

/**
 * Write at most `max_chars` characters of `text` with the top left corner at (x, y).
 * @return the x after the last character drawn (where the next text can start)
 */
static inline int canvas_text_n(Canvas* canvas, int x, int y, FontStyle style, const char* text, int max_chars, bool on) {
    for (int i = 0; text[i] != '\0' && i < max_chars; ++i) {
        if (style == FONT_STYLE_BIG) {
            canvas_glyph_big(canvas, x, y, text[i], on);
        } else {
            canvas_glyph_small(canvas, x, y, text[i], on);
        }
        x += canvas_char_width(style);
    }

    return x;
}

static inline int canvas_text(Canvas* canvas, int x, int y, FontStyle style, const char* text, bool on) {
    return canvas_text_n(canvas, x, y, style, text, 1 << 20, on);
}

/** How many characters of the text fit in `max_width` pixels */
static inline int canvas_chars_that_fit(FontStyle style, int max_width) {
    return max_width < canvas_text_width(style, 1) ? 0 : (max_width + 1) / canvas_char_width(style);
}

/**
 * The text in the biggest style that shows all of it in `max_width` pixels (the big font when `allow_big`, the small
 * one otherwise); cut at the end when not even the small one does. The line is `canvas_font_height` high.
 * @return the width drawn
 */
static inline int canvas_text_fit(Canvas* canvas, int x, int y, int max_width, bool allow_big, const char* text, bool on) {
    const int length = (int) strlen(text);
    FontStyle style = FONT_STYLE_SMALL;

    if (allow_big && canvas_text_width(FONT_STYLE_BIG, length) <= max_width) {
        style = FONT_STYLE_BIG;
    }

    int count = canvas_chars_that_fit(style, max_width);
    if (count > length) {
        count = length;
    }

    canvas_text_n(canvas, x, y, style, text, count, on);
    return canvas_text_width(style, count);
}

/** A text ending at the right edge `right` (exclusive) */
static inline void canvas_text_right(Canvas* canvas, int right, int y, FontStyle style, const char* text, bool on) {
    canvas_text(canvas, right - canvas_text_width(style, (int) strlen(text)), y, style, text, on);
}

/** A text centred between `left` and `right` */
static inline void canvas_text_centred(Canvas* canvas, int left, int right, int y, FontStyle style, const char* text, bool on) {
    canvas_text(canvas, left + (right - left - canvas_text_width(style, (int) strlen(text))) / 2, y, style, text, on);
}

/** A digit of the 5 x 7 font at twice the size (10 x 14): the number of a preset on a tall area */
static inline void canvas_big_digit(Canvas* canvas, int x, int y, int digit, bool on) {
    const uint8_t* columns = &FONT_BIG[('0' + digit % 10 - FONT_BIG_FIRST) * 5];

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 7; ++j) {
            if (columns[i] & (1u << j)) {
                canvas_fill(canvas, x + i * 2, y + j * 2, 2, 2, on);
            }
        }
    }
}

#endif
