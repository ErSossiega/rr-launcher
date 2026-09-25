/*
    gfx.c - minimal software renderer for the graphical menu

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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "pngu/pngu.h"

/* The 8x16 bitmap font libogc's console uses; one byte per row, most significant bit leftmost. */
extern u8 console_font_8x16[];

#define CH_R(c) (((c) >> 24) & 0xFF)
#define CH_G(c) (((c) >> 16) & 0xFF)
#define CH_B(c) (((c) >> 8) & 0xFF)
#define CH_A(c) ((c) & 0xFF)

static inline u32 div255(u32 v)
{
    return (v + 1 + (v >> 8)) >> 8;
}

static inline int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Blends `color' over `dst' with an opacity of `alpha' (0-255). The canvas is always opaque. */
static inline u32 blend(u32 dst, u32 color, u32 alpha)
{
    if (alpha == 0)
        return dst;
    if (alpha >= 255)
        return color | 0xFF;

    u32 inv = 255 - alpha;
    return RRC_GFX_RGBA(
        div255(CH_R(color) * alpha + CH_R(dst) * inv),
        div255(CH_G(color) * alpha + CH_G(dst) * inv),
        div255(CH_B(color) * alpha + CH_B(dst) * inv),
        0xFF);
}

static inline void put_pixel(struct rrc_gfx_image *dst, int x, int y, u32 color, u32 alpha)
{
    if (x < 0 || y < 0 || x >= dst->width || y >= dst->height)
        return;

    u32 *p = &dst->pixels[y * dst->width + x];
    *p = blend(*p, color, alpha);
}

/*
    Signed distance from the centre of pixel (px, py) to the edge of a rounded rectangle:
    negative inside, positive outside.
*/
static float rounded_rect_distance(int px, int py, int x, int y, int w, int h, int radius)
{
    float hw = w * 0.5f;
    float hh = h * 0.5f;
    float qx = fabsf(px + 0.5f - (x + hw)) - (hw - radius);
    float qy = fabsf(py + 0.5f - (y + hh)) - (hh - radius);

    float ox = qx > 0 ? qx : 0;
    float oy = qy > 0 ? qy : 0;
    float outside = (ox == 0 || oy == 0) ? ox + oy : sqrtf(ox * ox + oy * oy);
    float inside = qx > qy ? qx : qy;
    if (inside > 0)
        inside = 0;

    return outside + inside - radius;
}

/* Fraction (0-1) of pixel (px, py) covered by a rounded rectangle, for anti-aliasing. */
static float rounded_rect_coverage(int px, int py, int x, int y, int w, int h, int radius)
{
    if (w <= 0 || h <= 0)
        return 0;

    float c = 0.5f - rounded_rect_distance(px, py, x, y, w, h, radius);
    return c < 0 ? 0 : (c > 1 ? 1 : c);
}

int rrc_gfx_image_alloc(struct rrc_gfx_image *img, int width, int height)
{
    img->pixels = malloc(width * height * sizeof(u32));
    if (img->pixels == NULL)
    {
        img->width = img->height = 0;
        return -1;
    }

    img->width = width;
    img->height = height;
    return 0;
}

int rrc_gfx_image_from_png(struct rrc_gfx_image *img, const void *png)
{
    PNGUPROP prop;
    int ret = -1;

    IMGCTX ctx = PNGU_SelectImageFromBuffer(png);
    if (!ctx)
        return -1;

    if (PNGU_GetImageProperties(ctx, &prop) != PNGU_OK)
        goto out;

    if (rrc_gfx_image_alloc(img, prop.imgWidth, prop.imgHeight) != 0)
        goto out;

    if (PNGU_DecodeToRGBA8(ctx, prop.imgWidth, prop.imgHeight, img->pixels, 0, 0xFF) != PNGU_OK)
    {
        rrc_gfx_image_free(img);
        goto out;
    }

    ret = 0;

out:
    PNGU_ReleaseImageContext(ctx);
    return ret;
}

void rrc_gfx_image_free(struct rrc_gfx_image *img)
{
    free(img->pixels);
    img->pixels = NULL;
    img->width = img->height = 0;
}

void rrc_gfx_clear(struct rrc_gfx_image *dst, u32 color)
{
    int n = dst->width * dst->height;
    for (int i = 0; i < n; i++)
    {
        dst->pixels[i] = color | 0xFF;
    }
}

void rrc_gfx_fill_rounded_rect(struct rrc_gfx_image *dst, int x, int y, int w, int h, int radius, u32 top, u32 bottom)
{
    for (int py = y; py < y + h; py++)
    {
        int t = h > 1 ? ((py - y) * 255) / (h - 1) : 0;
        u32 color = RRC_GFX_RGBA(
            div255(CH_R(top) * (255 - t) + CH_R(bottom) * t),
            div255(CH_G(top) * (255 - t) + CH_G(bottom) * t),
            div255(CH_B(top) * (255 - t) + CH_B(bottom) * t),
            div255(CH_A(top) * (255 - t) + CH_A(bottom) * t));

        for (int px = x; px < x + w; px++)
        {
            float c = rounded_rect_coverage(px, py, x, y, w, h, radius);
            put_pixel(dst, px, py, color, (u32)(CH_A(color) * c));
        }
    }
}

void rrc_gfx_stroke_rounded_rect(struct rrc_gfx_image *dst, int x, int y, int w, int h, int radius, int thickness, u32 color)
{
    int inner_radius = radius > thickness ? radius - thickness : 0;

    for (int py = y; py < y + h; py++)
    {
        for (int px = x; px < x + w; px++)
        {
            float c = rounded_rect_coverage(px, py, x, y, w, h, radius) -
                      rounded_rect_coverage(px, py, x + thickness, y + thickness, w - 2 * thickness, h - 2 * thickness, inner_radius);
            if (c > 0)
                put_pixel(dst, px, py, color, (u32)(CH_A(color) * c));
        }
    }
}

void rrc_gfx_glow_rounded_rect(struct rrc_gfx_image *dst, int x, int y, int w, int h, int radius, int size, u32 color)
{
    for (int py = y - size; py < y + h + size; py++)
    {
        for (int px = x - size; px < x + w + size; px++)
        {
            float d = rounded_rect_distance(px, py, x, y, w, h, radius);
            if (d <= 0 || d >= size)
                continue;

            float f = 1.0f - d / size;
            put_pixel(dst, px, py, color, (u32)(CH_A(color) * f * f));
        }
    }
}

void rrc_gfx_draw_image_rounded(struct rrc_gfx_image *dst, const struct rrc_gfx_image *src, int x, int y, int w, int h, int radius)
{
    if (src->pixels == NULL || w <= 0 || h <= 0)
        return;

    for (int py = y; py < y + h; py++)
    {
        const u32 *src_row = &src->pixels[((py - y) * src->height / h) * src->width];

        for (int px = x; px < x + w; px++)
        {
            float c = rounded_rect_coverage(px, py, x, y, w, h, radius);
            if (c <= 0)
                continue;

            u32 s = src_row[(px - x) * src->width / w];
            put_pixel(dst, px, py, s, (u32)(CH_A(s) * c));
        }
    }
}

void rrc_gfx_draw_text(struct rrc_gfx_image *dst, int x, int y, const char *text, int scale, u32 color)
{
    for (; *text; text++, x += RRC_GFX_FONT_W * scale)
    {
        const u8 *glyph = &console_font_8x16[((unsigned char)*text) * RRC_GFX_FONT_H];

        for (int row = 0; row < RRC_GFX_FONT_H; row++)
        {
            u8 bits = glyph[row];
            for (int col = 0; col < RRC_GFX_FONT_W; col++)
            {
                if (!(bits & (0x80 >> col)))
                    continue;

                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        put_pixel(dst, x + col * scale + sx, y + row * scale + sy, color, CH_A(color));
            }
        }
    }
}

int rrc_gfx_text_width(const char *text, int scale)
{
    return strlen(text) * RRC_GFX_FONT_W * scale;
}

/*
    Full-range BT.601, the same conversion PNGU uses for the banner, so menu colours match it exactly.
    Chroma is shared between the two pixels of a framebuffer word, so it is averaged.
*/
static inline u32 rgb_pair_to_ycbycr(u32 p1, u32 p2)
{
    int r1 = CH_R(p1), g1 = CH_G(p1), b1 = CH_B(p1);
    int r2 = CH_R(p2), g2 = CH_G(p2), b2 = CH_B(p2);

    int y1 = (77 * r1 + 150 * g1 + 29 * b1) >> 8;
    int y2 = (77 * r2 + 150 * g2 + 29 * b2) >> 8;
    int cb = ((-43 * (r1 + r2) - 85 * (g1 + g2) + 128 * (b1 + b2)) >> 9) + 128;
    int cr = ((128 * (r1 + r2) - 107 * (g1 + g2) - 21 * (b1 + b2)) >> 9) + 128;

    return ((u32)clamp_int(y1, 0, 255) << 24) |
           ((u32)clamp_int(cb, 0, 255) << 16) |
           ((u32)clamp_int(y2, 0, 255) << 8) |
           (u32)clamp_int(cr, 0, 255);
}

/*
    Resamples a line of `src_n' pixels to `dst_n' pixels by averaging the source pixels each destination
    pixel covers (box filter), so thin lines like text strokes survive downscaling.
    The strides (in pixels) allow resampling columns as well as rows.
*/
static void scale_line(const u32 *src, int src_n, int src_stride, u32 *dst, int dst_n, int dst_stride)
{
    for (int i = 0; i < dst_n; i++)
    {
        // span of source pixels covered by destination pixel `i', in 1/256ths of a pixel
        int start = (i * src_n * 256) / dst_n;
        int end = ((i + 1) * src_n * 256) / dst_n;
        u32 r = 0, g = 0, b = 0, a = 0;

        for (int p = start >> 8; p * 256 < end && p < src_n; p++)
        {
            int lo = p * 256 > start ? p * 256 : start;
            int hi = (p + 1) * 256 < end ? (p + 1) * 256 : end;
            u32 weight = hi - lo;
            u32 c = src[p * src_stride];

            r += CH_R(c) * weight;
            g += CH_G(c) * weight;
            b += CH_B(c) * weight;
            a += CH_A(c) * weight;
        }

        u32 total = end - start;
        dst[i * dst_stride] = RRC_GFX_RGBA(r / total, g / total, b / total, a / total);
    }
}

int rrc_gfx_image_resize(struct rrc_gfx_image *dst, const struct rrc_gfx_image *src, int width, int height)
{
    // Scale rows first into an intermediate image, then its columns into `dst'.
    struct rrc_gfx_image tmp;
    if (rrc_gfx_image_alloc(&tmp, width, src->height) != 0)
    {
        dst->pixels = NULL;
        dst->width = dst->height = 0;
        return -1;
    }

    if (rrc_gfx_image_alloc(dst, width, height) != 0)
    {
        rrc_gfx_image_free(&tmp);
        return -1;
    }

    for (int y = 0; y < src->height; y++)
    {
        scale_line(&src->pixels[y * src->width], src->width, 1, &tmp.pixels[y * width], width, 1);
    }

    for (int x = 0; x < width; x++)
    {
        scale_line(&tmp.pixels[x], src->height, width, &dst->pixels[x], height, width);
    }

    rrc_gfx_image_free(&tmp);
    return 0;
}

void rrc_gfx_present(const struct rrc_gfx_image *src, void *xfb, GXRModeObj *rmode, int x, int y, int width)
{
    u32 *fb = xfb;
    int words_per_row = rmode->fbWidth / 2;
    u32 scaled[width];

    for (int row = 0; row < src->height; row++)
    {
        int fy = y + row;
        if (fy < 0 || fy >= rmode->xfbHeight)
            continue;

        const u32 *line = &src->pixels[row * src->width];
        if (width != src->width)
        {
            scale_line(line, src->width, 1, scaled, width, 1);
            line = scaled;
        }

        u32 *out = &fb[fy * words_per_row];

        for (int col = 0; col + 1 < width; col += 2)
        {
            int fx = x + col;
            if (fx < 0 || fx + 1 >= rmode->fbWidth)
                continue;

            out[fx / 2] = rgb_pair_to_ycbycr(line[col], line[col + 1]);
        }
    }
}
