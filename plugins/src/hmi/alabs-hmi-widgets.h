/*
  aLabs HMI widgets extension
  Copyright 2026 Andrea Del Signore

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/**
   @file alabs-hmi-widgets.h
   C header for plugin widgets drawn on the display of a device (HMI), as used on the MOD Dwarf by aLabs images.
*/

#ifndef ALABS_HMI_WIDGETS_H_INCLUDED
#define ALABS_HMI_WIDGETS_H_INCLUDED

#if defined(__has_include) && __has_include(<lv2/core/lv2.h>)
#include <lv2/core/lv2.h>
#else
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#endif

#define ALABS_HMI_URI                 "https://github.com/sejerpz/alabs-hmi-widgets"
#define ALABS_HMI_PREFIX              ALABS_HMI_URI "#"
#define ALABS_HMI__WidgetDraw         ALABS_HMI_PREFIX "WidgetDraw"
#define ALABS_HMI__WidgetNotification ALABS_HMI_PREFIX "WidgetNotification"
#define ALABS_HMI__Widget             ALABS_HMI_PREFIX "Widget"
#define ALABS_HMI__widget             ALABS_HMI_PREFIX "widget"
#define ALABS_HMI__minWidth           ALABS_HMI_PREFIX "minWidth"
#define ALABS_HMI__minHeight          ALABS_HMI_PREFIX "minHeight"
#define ALABS_HMI__naturalWidth       ALABS_HMI_PREFIX "naturalWidth"
#define ALABS_HMI__naturalHeight      ALABS_HMI_PREFIX "naturalHeight"
#define ALABS_HMI__suggestedPorts     ALABS_HMI_PREFIX "suggestedPorts"
#define ALABS_HMI__pixelFormat        ALABS_HMI_PREFIX "pixelFormat"
#define ALABS_HMI__Mono1              ALABS_HMI_PREFIX "Mono1"
#define ALABS_HMI__Gray4              ALABS_HMI_PREFIX "Gray4"
#define ALABS_HMI__RGB565             ALABS_HMI_PREFIX "RGB565"
#define ALABS_HMI__RGB888             ALABS_HMI_PREFIX "RGB888"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Opaque handle for a plugin widget shown on the display, given from host to plugins.
 *
 *  A widget is declared by the plugin in its TTL and is not tied to any parameter:
 *
 *    <plugin> ahw:widget [
 *        a ahw:Widget ;
 *        lv2:symbol "meter" ;
 *        lv2:name "Meter" ;
 *        ahw:minWidth 32 ;
 *        ahw:minHeight 10 ;
 *        ahw:naturalWidth 40 ;
 *        ahw:naturalHeight 37 ;
 *    ] .
 *
 *  The areas of the display are up to the device. ahw:minWidth and ahw:minHeight are the smallest area, in pixels,
 *  the widget can draw in: the host only offers it in areas at least that big, in any area when missing.
 *  ahw:naturalWidth and ahw:naturalHeight are the size the widget looks best at, the minimum when missing: a hint
 *  the host uses to order and preselect areas, never to exclude one. The plugin draws in any area at least as big
 *  as its minimum and learns its size in ALABS_HMI_WidgetNotification::widget_shown.
 *
 *  ahw:pixelFormat lists the pixel formats the widget can draw in, ahw:Mono1 alone when missing:
 *
 *        ahw:pixelFormat ahw:Mono1 , ahw:RGB565 ;
 *
 *  A widget is only offered on a device whose display uses one of them, and is told which one in
 *  ALABS_HMI_WidgetNotification::widget_shown.
 *
 *  ahw:suggestedPorts is an ordered list of the symbols of the ports the widget would like bound to the controls
 *  of the device, a suggestion the host offers to the user when the widget is placed:
 *
 *        ahw:suggestedPorts ( "stop_all" "level" ) ;
 *
 *  The user decides, for each area of the display, whether it shows the default content of the device (e.g. the
 *  knob addressing) or one of the widgets of the pedalboard, and, separately, what the controls are bound to.
 *  The hardware controls keep working as usual either way, only what is drawn changes.
 */
typedef void* ALABS_HMI_Widget;

/**
 *  The pixel format of a widget area, the layout of the bitmaps given to ALABS_HMI_WidgetDraw::draw_bitmap.
 *  In every format a bitmap holds its rows from top to bottom, and all bits zero is the background.
 */
typedef enum {
   /**
    *  1 bit per pixel, a set bit is a lit pixel. A row is (width + 7) / 8 bytes, the most significant bit
    *  of its first byte being the leftmost pixel. The format of the MOD Dwarf display.
    */
    ALABS_HMI_PixelFormat_Mono1 = 0,

   /**
    *  4 bits per pixel, 0 is the background and 15 the brightest. A row is (width + 1) / 2 bytes, the high
    *  nibble of its first byte being the leftmost pixel.
    */
    ALABS_HMI_PixelFormat_Gray4 = 1,

   /**
    *  16 bits per pixel, 5 red, 6 green, 5 blue, most significant bits first, each pixel in big endian.
    *  A row is width * 2 bytes.
    */
    ALABS_HMI_PixelFormat_RGB565 = 2,

   /**
    *  24 bits per pixel, a byte each for red, green and blue in that order. A row is width * 3 bytes.
    */
    ALABS_HMI_PixelFormat_RGB888 = 3
} ALABS_HMI_PixelFormat;

/**
 *  The area a widget draws in, given to ALABS_HMI_WidgetNotification::widget_shown.
 *  Only valid during that call: the plugin keeps what it needs.
 */
typedef struct {
   /**
    *  The size of this struct. Fields are only ever appended: check it before reading any added later.
    */
    size_t size;

   /**
    *  Size of the area in pixels, never smaller than the ahw:minWidth and ahw:minHeight of the widget; the plugin
    *  draws in 0..width-1, 0..height-1, with the origin at the top-left corner of the area. Where the area sits on
    *  the display is up to the device.
    */
    uint16_t width, height;

   /**
    *  The pixel format of the area, always one the widget declares with ahw:pixelFormat.
    */
    ALABS_HMI_PixelFormat pixelFormat;
} ALABS_HMI_WidgetArea;

/**
 * Interface to be implemented by plugins that declare widgets,
 * returned on extension_data() when URI is ALABS_HMI__WidgetNotification.
 *
 * Threading: the host calls widget_shown() and widget_hidden() from a non real-time thread, one call at a time
 * for the same plugin instance, but possibly while run() is executing in the audio thread. The plugin
 * synchronizes whatever it shares between them and run(), e.g. publishing the widget handle atomically after
 * the rest of the state it goes with. Neither callback may be called concurrently with instantiate(),
 * activate(), deactivate() or cleanup().
 */
typedef struct {
   /**
    * The size of this struct, as known by the plugin.
    * Used by the host to know which callbacks are present, for future revisions of this API.
    */
    size_t size;

   /**
    * Notifies the plugin that the widget declared with @a symbol is now visible on an area of the display,
    * e.g. because the user moved to the page holding it, or placed it on the current page.
    * The @a widget variable should be saved by the plugin and used as argument to ALABS_HMI_WidgetDraw::draw_bitmap
    * until widget_hidden() is called for it; every showing gets a new handle.
    *
    * @a area describes where the plugin draws: its size and its pixel format. It is only valid during this call.
    *
    * The plugin MUST redraw the whole area after this call: nothing drawn during a previous showing is guaranteed
    * to be there. Until it does, the area shows nothing, a device placeholder, or a stale copy of earlier content
    * kept by the device.
    *
    * The same widget may be visible in more than one area at once, each with its own @a widget handle.
    */
    void (*widget_shown)(LV2_Handle handle,
                         const char* symbol,
                         ALABS_HMI_Widget widget,
                         const ALABS_HMI_WidgetArea* area);

   /**
    * Notifies the plugin that a widget is no longer visible, e.g. because the user moved to another page.
    * The plugin MUST NOT start any call to ALABS_HMI_WidgetDraw::draw_bitmap with this @a widget after this
    * call returns. A call already running in another thread when widget_hidden() is called is harmless: the host
    * drops it.
    */
    void (*widget_hidden)(LV2_Handle handle, ALABS_HMI_Widget widget);

} ALABS_HMI_WidgetNotification;

/**
 * Compatibility size for ALABS_HMI_WidgetNotification.
 */
#define ALABS_HMI_WIDGETNOTIFICATION_SIZE_BASE (sizeof(size_t) + sizeof(void*) * 2)

/**
 *  Opaque handle for ALABS_HMI__WidgetDraw feature.
 */
typedef void* ALABS_HMI_WidgetDraw_Handle;

/**
 * Compatibility size for ALABS_HMI_WidgetDraw.
 */
#define ALABS_HMI_WIDGETDRAW_SIZE_BASE (sizeof(size_t) + sizeof(void*) * 2)

/**
 * On instantiation, host supplies ALABS_HMI__WidgetDraw feature where it can show widgets.
 * LV2_Feature::data must be pointer to ALABS_HMI_WidgetDraw.
 */
typedef struct {
    /**
     * The size of this struct, as known by the host.
     * Functions are only ever appended: check it before calling any added later.
     */
    size_t size;

    /**
     *  Opaque host data.
     */
    ALABS_HMI_WidgetDraw_Handle handle;

    /**
     * Draw a bitmap into the area of a visible widget, in the rectangle @a x, @a y, @a width, @a height
     * given in area coordinates. Any rectangle inside the area is fine, so the plugin can update just the part
     * that changed. Pixels outside the area are clipped.
     * The bitmap replaces the pixels of the rectangle; the rest of the area is left untouched.
     *
     * @a bits holds @a height rows from top to bottom, in the pixel format of the area (ALABS_HMI_PixelFormat),
     * which also sets how many bytes a row takes. Every row starts at the beginning of its first byte, whatever
     * @a x is: in formats with less than a byte per pixel, when @a x does not fall on a byte the device does the
     * shifting, not the plugin.
     *
     * Passing NULL as @a bits fills the rectangle (clipped to the area) with the background, all bits zero,
     * without sending any pixel data. Clearing the whole area is NULL with 0, 0 and the size of the area.
     *
     * Updates, clears included, are applied in the order of the calls. The host may merge pending updates
     * before they reach the display, but never in a way that changes the final result.
     *
     * Threading: it may be called from any thread, run() included. The host makes it safe to call from the audio
     * thread: it never waits for the display, it only queues the update, and calls from different threads are
     * serialized by the host.
     *
     * @note This function must only be used with a @a widget received in
     *       ALABS_HMI_WidgetNotification::widget_shown and not yet hidden.
     */
    void (*draw_bitmap)(ALABS_HMI_WidgetDraw_Handle handle,
                        ALABS_HMI_Widget widget,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        const uint8_t* bits);

} ALABS_HMI_WidgetDraw;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ALABS_HMI_WIDGETS_H_INCLUDED */
