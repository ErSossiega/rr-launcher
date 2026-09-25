/*
    gfx.h - minimal software renderer for the graphical menu

    Copyright (C) 2025  Retro Rewind Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef RRC_GFX_H
#define RRC_GFX_H

#include <gccore.h>

/*
    The renderer draws into a plain RGBA8 image in main memory and then converts it to the
    YCbYCr format of the external framebuffer in one go (see `rrc_gfx_present').

    This deliberately avoids GX: the console used for progress output and prompts writes straight
    into the XFB, and drawing in software lets both coexist without any render-to-texture juggling.

    Colours are packed as 0xRRGGBBAA, which is also the in-memory layout PNGU decodes to.
*/

#define RRC_GFX_RGBA(r, g, b, a) ((((u32)(r)) << 24) | (((u32)(g)) << 16) | (((u32)(b)) << 8) | ((u32)(a)))

/* Width and height in pixels of one glyph of the built-in font at scale 1. */
#define RRC_GFX_FONT_W 8
#define RRC_GFX_FONT_H 16

struct rrc_gfx_image
{
    u32 *pixels;
    int width;
    int height;
};

/*
    Allocates an image of the given size. Pixel contents are undefined.

    Returns 0 on success, -1 on error.
*/
int rrc_gfx_image_alloc(struct rrc_gfx_image *img, int width, int height);

/*
    Decodes a PNG from memory into a newly allocated image.

    Returns 0 on success, -1 on error.
*/
int rrc_gfx_image_from_png(struct rrc_gfx_image *img, const void *png);

void rrc_gfx_image_free(struct rrc_gfx_image *img);

/*
    Allocates `dst' and fills it with `src' resampled to `width' x `height' (box filter, suited to
    downscaling).

    Returns 0 on success, -1 on error, in which case `dst' holds no pixels.
*/
int rrc_gfx_image_resize(struct rrc_gfx_image *dst, const struct rrc_gfx_image *src, int width, int height);

/* Fills the whole image with `color', ignoring its alpha. */
void rrc_gfx_clear(struct rrc_gfx_image *dst, u32 color);

/*
    Fills an anti-aliased rounded rectangle with a vertical gradient from `top' to `bottom'.
    Pass the same colour twice for a solid fill.
*/
void rrc_gfx_fill_rounded_rect(struct rrc_gfx_image *dst, int x, int y, int w, int h, int radius, u32 top, u32 bottom);

/* Draws the outline of an anti-aliased rounded rectangle, `thickness' pixels wide, inside its bounds. */
void rrc_gfx_stroke_rounded_rect(struct rrc_gfx_image *dst, int x, int y, int w, int h, int radius, int thickness, u32 color);

/* Draws a soft glow of `size' pixels around the outside of a rounded rectangle. */
void rrc_gfx_glow_rounded_rect(struct rrc_gfx_image *dst, int x, int y, int w, int h, int radius, int size, u32 color);

/*
    Draws `src' stretched to `w' x `h' (nearest neighbour), clipped to a rounded rectangle.
    `src' alpha is respected.
*/
void rrc_gfx_draw_image_rounded(struct rrc_gfx_image *dst, const struct rrc_gfx_image *src, int x, int y, int w, int h, int radius);

/* Draws a single line of text using the console font, magnified by the integer `scale'. */
void rrc_gfx_draw_text(struct rrc_gfx_image *dst, int x, int y, const char *text, int scale, u32 color);

int rrc_gfx_text_width(const char *text, int scale);

/*
    Converts `src' to YCbYCr and writes it to the framebuffer `xfb' with its top left corner at (`x', `y'),
    scaled horizontally to `width' pixels (pass the width of `src' for no scaling).

    Scaling lets 16:9 content be drawn at its real proportions and squeezed into the framebuffer, which
    the TV stretches back out.

    `x' and `width' must be even, since each framebuffer word holds two pixels.
*/
void rrc_gfx_present(const struct rrc_gfx_image *src, void *xfb, GXRModeObj *rmode, int x, int y, int width);

#endif
