/*
    leaderboard_view.c - time trial leaderboard screens

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "leaderboard_view.h"
#include "../gfx.h"
#include "../gui.h"
#include "../pad.h"
#include "../shutdown.h"
#include "../util.h"

#define VIEW_PAD 12
#define ROW_H 18
/* Longest line in characters: the 16:9 canvas fits about 80. */
#define MAX_COLS 96

/* A held Up/Down repeats after REPEAT_DELAY loop ticks (20 ms each), then every REPEAT_RATE ticks. */
#define REPEAT_DELAY 20
#define REPEAT_RATE 3

/* Leaderboard columns, in characters. The player column takes what is left, within these bounds. */
#define COL_RANK_W 3
#define COL_CHARACTER_W 12
#define COL_VEHICLE_W 16
#define COL_TIME_W 9
#define COL_FLAGS_W 2
#define COL_PLAYER_MIN_W 8
#define COL_PLAYER_MAX_W 20

#define COLOR_BACKGROUND RRC_GFX_RGBA(0, 0, 0, 255)
#define COLOR_TITLE RRC_GFX_RGBA(255, 220, 90, 255)
#define COLOR_TEXT RRC_GFX_RGBA(205, 205, 215, 255)
#define COLOR_SELECTED_TEXT RRC_GFX_RGBA(255, 255, 255, 255)
#define COLOR_SELECTED_BG RRC_GFX_RGBA(35, 70, 125, 255)
#define COLOR_DIM RRC_GFX_RGBA(150, 150, 160, 255)
#define COLOR_RULE RRC_GFX_RGBA(80, 80, 95, 255)
#define COLOR_GLITCH RRC_GFX_RGBA(255, 140, 80, 255)
#define COLOR_SHROOMLESS RRC_GFX_RGBA(130, 220, 120, 255)

/* Code page 437 arrows, used as scroll indicators. */
#define GLYPH_UP "\x18"
#define GLYPH_DOWN "\x19"

struct view
{
    struct rrc_gfx_image canvas;
    /* Characters per line. */
    int cols;
};

static void draw_text(struct view *v, int col, int y, const char *s, u32 color)
{
    rrc_gfx_draw_text(&v->canvas, VIEW_PAD + col * RRC_GFX_FONT_W, y, s, 1, color);
}

/* Copies `src' into `dst' (at least `width' + 1 bytes), shortened to `width' characters with "..". */
static void fit(char *dst, const char *src, int width)
{
    int len = strlen(src);
    if (width < 0)
        width = 0;

    if (len <= width)
    {
        memcpy(dst, src, len + 1);
    }
    else if (width < 3)
    {
        memcpy(dst, src, width);
        dst[width] = '\0';
    }
    else
    {
        memcpy(dst, src, width - 2);
        memcpy(dst + width - 2, "..", 3);
    }
}

static int footer_y(const struct view *v)
{
    return v->canvas.height - VIEW_PAD - RRC_GFX_FONT_H;
}

static void draw_rule(struct view *v, int y)
{
    rrc_gfx_fill_rounded_rect(&v->canvas, VIEW_PAD, y, v->canvas.width - 2 * VIEW_PAD, 1, 0, COLOR_RULE, COLOR_RULE);
}

/* Clears the canvas and draws `title' on the left and `right' on the right of the first line. */
static void draw_header(struct view *v, const char *title, const char *right)
{
    char buf[MAX_COLS + 1];
    int right_len = strlen(right);

    rrc_gfx_clear(&v->canvas, COLOR_BACKGROUND);
    fit(buf, title, v->cols - right_len - 2);
    draw_text(v, 0, VIEW_PAD, buf, COLOR_TITLE);
    draw_text(v, v->cols - right_len, VIEW_PAD, right, COLOR_DIM);
}

static void draw_footer(struct view *v, const char *hints, bool can_scroll_up, bool can_scroll_down)
{
    draw_text(v, 0, footer_y(v), hints, COLOR_DIM);
    draw_text(v, v->cols - 2, footer_y(v), GLYPH_UP, can_scroll_up ? COLOR_SELECTED_TEXT : COLOR_RULE);
    draw_text(v, v->cols - 1, footer_y(v), GLYPH_DOWN, can_scroll_down ? COLOR_SELECTED_TEXT : COLOR_RULE);
}

static void present(struct view *v, void *xfb)
{
    rrc_gfx_present(&v->canvas, xfb, rrc_gui_get_video_mode(), RRC_GUI_BOX_X, RRC_GUI_BOX_Y, RRC_GUI_BOX_W);
}

/* Number of list rows that fit between `list_top' and the footer. */
static int visible_rows(const struct view *v, int list_top)
{
    return (footer_y(v) - 6 - list_top) / ROW_H;
}

/*
    Returns -1 for Up and +1 for Down, both on a press and repeatedly while the direction is held,
    otherwise 0. Must be called once per loop tick.
*/
static int vertical_input(int *held_ticks, struct pad_state pressed, struct pad_state held)
{
    if (rrc_pad_up_pressed(pressed) || rrc_pad_down_pressed(pressed))
    {
        *held_ticks = 0;
        return rrc_pad_up_pressed(pressed) ? -1 : 1;
    }

    int dir = rrc_pad_up_pressed(held) ? -1 : (rrc_pad_down_pressed(held) ? 1 : 0);
    if (dir == 0)
    {
        *held_ticks = 0;
        return 0;
    }

    (*held_ticks)++;
    if (*held_ticks >= REPEAT_DELAY && (*held_ticks - REPEAT_DELAY) % REPEAT_RATE == 0)
        return dir;

    return 0;
}

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void show_leaderboard(struct view *v, void *xfb, const struct rrc_tt_track *track, const struct rrc_tt_leaderboard *lb)
{
    int player_w = clamp(v->cols - (COL_RANK_W + COL_CHARACTER_W + COL_VEHICLE_W + COL_TIME_W + COL_FLAGS_W + 5),
                         COL_PLAYER_MIN_W, COL_PLAYER_MAX_W);
    int col_player = COL_RANK_W + 1;
    int col_character = col_player + player_w + 1;
    int col_vehicle = col_character + COL_CHARACTER_W + 1;
    int col_time = col_vehicle + COL_VEHICLE_W + 1;
    int col_flags = col_time + COL_TIME_W + 1;

    int subtitle_y = VIEW_PAD + ROW_H;
    int columns_y = subtitle_y + ROW_H + 4;
    int list_top = columns_y + ROW_H + 6;
    int visible = visible_rows(v, list_top);
    int max_top = lb->count > visible ? lb->count - visible : 0;

    int top = 0;
    int held_ticks = 0;
    bool redraw = true;

    while (1)
    {
        rrc_shutdown_check();

        if (redraw)
        {
            char header_right[16];
            snprintf(header_right, sizeof(header_right), "%dcc", RRC_TT_CC);
            draw_header(v, track->name, header_right);

            char subtitle[48];
            if (lb->count == 0)
                subtitle[0] = '\0';
            else if (lb->total > lb->count)
                snprintf(subtitle, sizeof(subtitle), "Top %d of %d times", lb->count, lb->total);
            else
                snprintf(subtitle, sizeof(subtitle), lb->count == 1 ? "%d time" : "%d times", lb->count);
            draw_text(v, 0, subtitle_y, subtitle, COLOR_DIM);

            if (lb->count == 0)
            {
                draw_text(v, 0, list_top, "No times on this track yet.", COLOR_TEXT);
                draw_text(v, 0, list_top + ROW_H, "Times are added by the moderators after review.", COLOR_DIM);
            }
            else
            {
                draw_text(v, 0, columns_y, "  #", COLOR_DIM);
                draw_text(v, col_player, columns_y, "Player", COLOR_DIM);
                draw_text(v, col_character, columns_y, "Character", COLOR_DIM);
                draw_text(v, col_vehicle, columns_y, "Vehicle", COLOR_DIM);
                draw_text(v, col_time, columns_y, "Time", COLOR_DIM);
                draw_rule(v, list_top - 5);

                for (int i = 0; i < visible && top + i < lb->count; i++)
                {
                    const struct rrc_tt_entry *e = &lb->entries[top + i];
                    int y = list_top + i * ROW_H;
                    char buf[MAX_COLS + 1];

                    snprintf(buf, sizeof(buf), "%3d", e->rank);
                    draw_text(v, 0, y, buf, COLOR_DIM);

                    fit(buf, e->player, player_w);
                    draw_text(v, col_player, y, buf, COLOR_SELECTED_TEXT);

                    fit(buf, rrc_tt_character_name(e->character), COL_CHARACTER_W);
                    draw_text(v, col_character, y, buf, COLOR_TEXT);

                    fit(buf, rrc_tt_vehicle_name(e->vehicle), COL_VEHICLE_W);
                    draw_text(v, col_vehicle, y, buf, COLOR_TEXT);

                    fit(buf, e->time, COL_TIME_W);
                    draw_text(v, col_time, y, buf, COLOR_SELECTED_TEXT);

                    if (e->glitch)
                        draw_text(v, col_flags, y, "G", COLOR_GLITCH);
                    if (e->shroomless)
                        draw_text(v, col_flags + 1, y, "S", COLOR_SHROOMLESS);
                }
            }

            draw_footer(v, "Up/Down: Scroll   B: Back   G: Glitch  S: Shroomless", top > 0, top < max_top);
            present(v, xfb);
            redraw = false;
        }

        struct pad_state pressed = rrc_pad_buttons();
        struct pad_state held = rrc_pad_held();
        int move = vertical_input(&held_ticks, pressed, held);

        if (rrc_pad_b_pressed(pressed) || rrc_pad_home_pressed(pressed))
            return;

        if (rrc_pad_left_pressed(pressed))
            move = -visible;
        else if (rrc_pad_right_pressed(pressed))
            move = visible;

        int new_top = clamp(top + move, 0, max_top);
        if (new_top != top)
        {
            top = new_top;
            redraw = true;
        }

        usleep(RRC_WPAD_LOOP_TIMEOUT);
    }
}

static void draw_tracks(struct view *v, const struct rrc_tt_tracks *tracks, int selected, int top, int list_top, int visible)
{
    char title[48], position[16];
    snprintf(title, sizeof(title), "Time Trial Leaderboards - %dcc", RRC_TT_CC);
    snprintf(position, sizeof(position), "%d/%d", selected + 1, tracks->count);
    draw_header(v, title, position);
    draw_rule(v, list_top - 5);

    for (int i = 0; i < visible && top + i < tracks->count; i++)
    {
        const struct rrc_tt_track *t = &tracks->tracks[top + i];
        bool is_selected = top + i == selected;
        int y = list_top + i * ROW_H;
        char buf[MAX_COLS + 1];

        if (is_selected)
        {
            rrc_gfx_fill_rounded_rect(&v->canvas, VIEW_PAD - 4, y - 1, v->canvas.width - 2 * VIEW_PAD + 8, ROW_H, 4,
                                      COLOR_SELECTED_BG, COLOR_SELECTED_BG);
        }

        fit(buf, t->name, v->cols - 7);
        draw_text(v, 0, y, buf, is_selected ? COLOR_SELECTED_TEXT : COLOR_TEXT);
        if (t->retro)
            draw_text(v, v->cols - 5, y, "Retro", COLOR_DIM);
    }

    draw_footer(v, "A: Leaderboard   Left/Right: Page   B: Back", top > 0, top + visible < tracks->count);
}

void rrc_leaderboard_view_display(void *xfb, const struct rrc_tt_tracks *tracks)
{
    if (tracks->count == 0)
        return;

    struct view v;
    memset(&v, 0, sizeof(v));
    if (rrc_gfx_image_alloc(&v.canvas, rrc_gui_is_widescreen() ? RRC_GUI_BOX_W_WIDESCREEN : RRC_GUI_BOX_W, RRC_GUI_BOX_H) != 0)
        return;

    v.cols = (v.canvas.width - 2 * VIEW_PAD) / RRC_GFX_FONT_W;
    if (v.cols > MAX_COLS)
        v.cols = MAX_COLS;

    int list_top = VIEW_PAD + ROW_H + 8;
    int visible = visible_rows(&v, list_top);
    int selected = 0;
    int top = 0;
    int held_ticks = 0;
    bool redraw = true;

    while (1)
    {
        rrc_shutdown_check();

        if (redraw)
        {
            draw_tracks(&v, tracks, selected, top, list_top, visible);
            present(&v, xfb);
            redraw = false;
        }

        struct pad_state pressed = rrc_pad_buttons();
        struct pad_state held = rrc_pad_held();
        int move = vertical_input(&held_ticks, pressed, held);

        if (rrc_pad_b_pressed(pressed) || rrc_pad_home_pressed(pressed))
            break;

        if (rrc_pad_a_pressed(pressed))
        {
            // blank the box: the download shows its progress on the console
            rrc_gfx_clear(&v.canvas, COLOR_BACKGROUND);
            present(&v, xfb);

            const struct rrc_tt_track *track = &tracks->tracks[selected];
            struct rrc_tt_leaderboard lb;
            struct rrc_result res = rrc_tt_fetch_leaderboard(track->id, &lb);
            if (rrc_result_is_error(res))
            {
                rrc_result_error_check_error_normal(res, xfb);
            }
            else
            {
                show_leaderboard(&v, xfb, track, &lb);
                rrc_tt_free_leaderboard(&lb);
            }

            redraw = true;
        }

        if (rrc_pad_left_pressed(pressed))
            move = -visible;
        else if (rrc_pad_right_pressed(pressed))
            move = visible;

        int new_selected = clamp(selected + move, 0, tracks->count - 1);
        if (new_selected != selected)
        {
            selected = new_selected;
            if (selected < top)
                top = selected;
            else if (selected >= top + visible)
                top = selected - visible + 1;
            redraw = true;
        }

        usleep(RRC_WPAD_LOOP_TIMEOUT);
    }

    rrc_gfx_image_free(&v.canvas);
}
