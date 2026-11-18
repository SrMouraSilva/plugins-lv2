/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The row of steps of Kill Switch, drawn on a widget canvas: one bar for each step, as high as its level
 * says ('2' the whole height, '1' half of it, '0' only the baseline), and a caret under the step that plays.
 */

#ifndef HMI_WIDGET_STEPS_H
#define HMI_WIDGET_STEPS_H

#include "widget-canvas.h"

/** What a row of steps takes under its bars: a gap of one pixel and the caret */
#define STEPS_CARET_HEIGHT 3

/** The first column of step `index` of `count` in a row `width` pixels wide that starts at `x` */
static inline int widget_step_x(int x, int width, int count, int index) {
    return x + (index * width) / count;
}

/**
 * Draw `count` steps of `steps` ('0'..'2') in the box x, y, width, bar_height (the bars; the caret takes STEPS_CARET_HEIGHT
 * more rows below).
 *
 * @param playhead the step that plays, -1 for none; with `running` it is a dotted line under all of them instead of one
 *                 caret (a pattern too fast to follow with the eye)
 * @param dim      draw the bars as a grey (the effect is bypassed)
 */
static inline void widget_draw_steps(Canvas* canvas, int x, int y, int width, int bar_height, const char* steps, int count,
                                     int playhead, bool running, bool dim) {
    if (count <= 0 || width <= 0 || bar_height <= 0) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        const int left = widget_step_x(x, width, count, i);
        const int cell = widget_step_x(x, width, count, i + 1) - left;
        const int bar = cell >= 3 ? cell - 1 : 1;
        const int level = steps[i] - '0';

        // The cut keeps a line on the baseline, so the row reads as a row even when all but one step is cut
        int height = 1;
        if (level >= 2) {
            height = bar_height;
        } else if (level == 1) {
            height = (bar_height + 1) / 2;
        }

        if (dim && height > 1) {
            canvas_checker(canvas, left, y + bar_height - height, bar, height);
        } else {
            canvas_fill(canvas, left, y + bar_height - height, bar, height, true);
        }

        if (!dim && i == playhead && !running) {
            const int caret = y + bar_height + 1;
            canvas_fill(canvas, left, caret, bar, STEPS_CARET_HEIGHT - 1, true);
        }
    }

    if (running && !dim) {
        canvas_dotted_hline(canvas, x, y + bar_height + 2, width, 2);
    }
}

#endif
