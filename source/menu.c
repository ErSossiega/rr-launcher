/*
    menu.c - graphical main menu

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
#include <gccore.h>

#include "menu.h"
#include "channel.h"
#include "console.h"
#include "gfx.h"
#include "gui.h"
#include "pad.h"
#include "prompt.h"
#include "settings.h"
#include "shutdown.h"
#include "util.h"
#include "update/update.h"
#include "ghosts/leaderboard.h"
#include "ghosts/leaderboard_view.h"
#include "news/news.h"
#include "news/news_view.h"
#include "version.h"

/* The menu fills the black box of the banner, see gui.h. */
#define MENU_X RRC_GUI_BOX_X
#define MENU_Y RRC_GUI_BOX_Y
#define MENU_W RRC_GUI_BOX_W
#define MENU_H RRC_GUI_BOX_H
#define MENU_W_WIDESCREEN RRC_GUI_BOX_W_WIDESCREEN

#define MENU_PAD 10
#define MENU_GAP 12
#define MENU_FOOTER_H 40

/* Tiles per row: the six entries of `tiles' make a 3x2 grid. */
#define MENU_COLS 3

/* Tiles keep the proportions of their artwork. */
#define TILE_ASPECT_W 16
#define TILE_ASPECT_H 9

#define TILE_RADIUS 12
#define TILE_BORDER 3
#define TILE_GLOW 8
#define TILE_LABEL_MARGIN 16

#define COLOR_BACKGROUND RRC_GFX_RGBA(0, 0, 0, 255)
#define COLOR_SLOT_TOP RRC_GFX_RGBA(44, 58, 92, 255)
#define COLOR_SLOT_BOTTOM RRC_GFX_RGBA(14, 18, 32, 255)
#define COLOR_BORDER RRC_GFX_RGBA(225, 225, 225, 255)
#define COLOR_BORDER_SELECTED RRC_GFX_RGBA(120, 220, 255, 255)
#define COLOR_GLOW RRC_GFX_RGBA(70, 180, 255, 220)
#define COLOR_DIM RRC_GFX_RGBA(0, 0, 0, 80)
#define COLOR_LABEL RRC_GFX_RGBA(255, 255, 255, 255)
#define COLOR_LABEL_DISABLED RRC_GFX_RGBA(140, 140, 150, 255)
#define COLOR_SHADOW RRC_GFX_RGBA(0, 0, 0, 170)
#define COLOR_FOOTER RRC_GFX_RGBA(170, 170, 170, 255)
#define COLOR_STATUS RRC_GFX_RGBA(255, 220, 90, 255)
#define COLOR_ERROR RRC_GFX_RGBA(255, 110, 110, 255)

/* 5 seconds */
#define AUTOLAUNCH_TIME 5000000
#define TICKS_PER_SECOND (1000000 / RRC_WPAD_LOOP_TIMEOUT)

enum menu_action
{
    MENU_ACTION_LAUNCH,
    MENU_ACTION_UPDATES,
    MENU_ACTION_INSTALL_CHANNEL,
    MENU_ACTION_REMOVE_CHANNEL,
    MENU_ACTION_SETTINGS,
    MENU_ACTION_GHOSTS,
    MENU_ACTION_NEWS,
    /* Placeholder slot for a future feature; drawn greyed out. */
    MENU_ACTION_NONE
};

struct menu_tile
{
    /* Drawn centered on the tile when there is no artwork. */
    const char *label;

    /*
        Embedded 16:9 PNG artwork (see incbin.S) including its own caption, or NULL to draw an empty slot
        with `label' instead. It is resized to the tile once when the menu opens.
    */
    const void *image;

    enum menu_action action;
};

/* Generated from assets/Images by tools/make_tiles.py */
extern char tile_play[];
extern char tile_updates[];
extern char tile_install_channel[];
extern char tile_settings[];
extern char tile_ghosts[];
extern char tile_news[];
extern char tile_remove_channel[];

static const struct menu_tile menu_tiles[] = {
    {.label = "Launch Game", .image = tile_play, .action = MENU_ACTION_LAUNCH},
    {.label = "Updates", .image = tile_updates, .action = MENU_ACTION_UPDATES},
    {.label = "Install Channel", .image = tile_install_channel, .action = MENU_ACTION_INSTALL_CHANNEL},
    {.label = "Settings", .image = tile_settings, .action = MENU_ACTION_SETTINGS},
    {.label = "Ghosts", .image = tile_ghosts, .action = MENU_ACTION_GHOSTS},
    {.label = "News", .image = tile_news, .action = MENU_ACTION_NEWS},
};

/* Takes the place of the "Install Channel" tile while the channel is installed. */
static const struct menu_tile remove_channel_tile = {.label = "Remove Channel", .image = tile_remove_channel, .action = MENU_ACTION_REMOVE_CHANNEL};

#define TILE_COUNT ((int)(sizeof(menu_tiles) / sizeof(menu_tiles[0])))
#define ROW_COUNT ((TILE_COUNT + MENU_COLS - 1) / MENU_COLS)

struct menu_state
{
    struct rrc_gfx_image canvas;
    /* `menu_tiles', with the channel tile swapped depending on whether the channel is installed. */
    struct menu_tile tiles[TILE_COUNT];
    struct rrc_gfx_image images[TILE_COUNT];
    int tile_w;
    int tile_h;
    int selected;
    char status[64];
    u32 status_color;
    char vk_version[32];
    char channel_version[32];
};

/* Height of the area above the footer that holds the grid. */
#define GRID_AREA_H (MENU_H - 2 * MENU_PAD - MENU_FOOTER_H)

/*
    Makes the tiles as large as the canvas allows while keeping their aspect ratio: as wide as the
    columns allow (4:3), unless the rows then don't fit (16:9, where the canvas is wider).
*/
static void compute_tile_size(struct menu_state *st)
{
    int w = (st->canvas.width - 2 * MENU_PAD - (MENU_COLS - 1) * MENU_GAP) / MENU_COLS;
    int h = w * TILE_ASPECT_H / TILE_ASPECT_W;

    int max_h = (GRID_AREA_H - (ROW_COUNT - 1) * MENU_GAP) / ROW_COUNT;
    if (h > max_h)
    {
        h = max_h;
        w = h * TILE_ASPECT_W / TILE_ASPECT_H;
    }

    st->tile_w = w;
    st->tile_h = h;
}

/* Horizontal space between tiles (and around a full row), spread evenly across the canvas. */
static int tile_gap_x(const struct menu_state *st)
{
    return (st->canvas.width - MENU_COLS * st->tile_w) / (MENU_COLS + 1);
}

/* Left and right edges of a full row of tiles, which the footer text lines up with. */
static void grid_bounds(const struct menu_state *st, int *left, int *right)
{
    int row_w = MENU_COLS * st->tile_w + (MENU_COLS - 1) * tile_gap_x(st);
    *left = (st->canvas.width - row_w) / 2;
    *right = *left + row_w;
}

/* Top left corner of tile `i' in canvas coordinates. A partially filled last row is centered. */
static void tile_position(const struct menu_state *st, int i, int *x, int *y)
{
    int row = i / MENU_COLS;
    int col = i % MENU_COLS;
    int in_row = TILE_COUNT - row * MENU_COLS;
    if (in_row > MENU_COLS)
        in_row = MENU_COLS;

    int gap = tile_gap_x(st);
    int row_w = in_row * st->tile_w + (in_row - 1) * gap;
    int grid_h = ROW_COUNT * st->tile_h + (ROW_COUNT - 1) * MENU_GAP;
    *x = (st->canvas.width - row_w) / 2 + col * (st->tile_w + gap);
    *y = MENU_PAD + (GRID_AREA_H - grid_h) / 2 + row * (st->tile_h + MENU_GAP);
}

/* Labels of empty slots are drawn large unless one of them would not fit, so that they all match. */
static int label_scale(const struct menu_state *st)
{
    for (int i = 0; i < TILE_COUNT; i++)
    {
        if (st->images[i].pixels == NULL && rrc_gfx_text_width(st->tiles[i].label, 2) > st->tile_w - 2 * TILE_LABEL_MARGIN)
            return 1;
    }

    return 2;
}

static void draw_text_shadowed(struct rrc_gfx_image *dst, int x, int y, const char *text, int scale, u32 color)
{
    rrc_gfx_draw_text(dst, x + scale, y + scale, text, scale, COLOR_SHADOW);
    rrc_gfx_draw_text(dst, x, y, text, scale, color);
}

static void draw_tile(struct menu_state *st, int i)
{
    const struct menu_tile *tile = &st->tiles[i];
    bool selected = st->selected == i;
    int w = st->tile_w, h = st->tile_h;
    int x, y;
    tile_position(st, i, &x, &y);

    if (selected)
        rrc_gfx_glow_rounded_rect(&st->canvas, x, y, w, h, TILE_RADIUS, TILE_GLOW, COLOR_GLOW);

    if (st->images[i].pixels != NULL)
    {
        rrc_gfx_draw_image_rounded(&st->canvas, &st->images[i], x, y, w, h, TILE_RADIUS);
    }
    else
    {
        rrc_gfx_fill_rounded_rect(&st->canvas, x, y, w, h, TILE_RADIUS, COLOR_SLOT_TOP, COLOR_SLOT_BOTTOM);

        int scale = label_scale(st);
        int tx = x + (w - rrc_gfx_text_width(tile->label, scale)) / 2;
        int ty = y + (h - RRC_GFX_FONT_H * scale) / 2;
        u32 color = tile->action == MENU_ACTION_NONE ? COLOR_LABEL_DISABLED : COLOR_LABEL;
        draw_text_shadowed(&st->canvas, tx, ty, tile->label, scale, color);
    }

    if (!selected)
        rrc_gfx_fill_rounded_rect(&st->canvas, x, y, w, h, TILE_RADIUS, COLOR_DIM, COLOR_DIM);

    rrc_gfx_stroke_rounded_rect(&st->canvas, x, y, w, h, TILE_RADIUS, TILE_BORDER, selected ? COLOR_BORDER_SELECTED : COLOR_BORDER);
}

static void draw_menu(struct menu_state *st, void *xfb)
{
    rrc_gfx_clear(&st->canvas, COLOR_BACKGROUND);

    for (int i = 0; i < TILE_COUNT; i++)
    {
        draw_tile(st, i);
    }

    int footer_y = MENU_H - MENU_PAD - MENU_FOOTER_H;
    int status_y = footer_y + 4;
    int hints_y = footer_y + 4 + RRC_GFX_FONT_H + 4;
    int left, right;
    grid_bounds(st, &left, &right);

    rrc_gfx_draw_text(&st->canvas, left, status_y, st->status, 1, st->status_color);
    rrc_gfx_draw_text(&st->canvas, right - rrc_gfx_text_width(st->channel_version, 1), status_y, st->channel_version, 1, COLOR_FOOTER);
    rrc_gfx_draw_text(&st->canvas, left, hints_y, "A: Select   HOME: Exit", 1, COLOR_FOOTER);
    rrc_gfx_draw_text(&st->canvas, right - rrc_gfx_text_width(st->vk_version, 1), hints_y, st->vk_version, 1, COLOR_FOOTER);

    rrc_gfx_present(&st->canvas, xfb, rrc_gui_get_video_mode(), MENU_X, MENU_Y, MENU_W);
}

/* Blanks the menu area so console output (progress, settings) starts on a clean slate. */
static void clear_menu(struct menu_state *st, void *xfb)
{
    rrc_gfx_clear(&st->canvas, COLOR_BACKGROUND);
    rrc_gfx_present(&st->canvas, xfb, rrc_gui_get_video_mode(), MENU_X, MENU_Y, MENU_W);
}

static void set_status(struct menu_state *st, const char *status)
{
    snprintf(st->status, sizeof(st->status), "%s", status);
    st->status_color = COLOR_STATUS;
}

static void set_error_status(struct menu_state *st, const char *status)
{
    set_status(st, status);
    st->status_color = COLOR_ERROR;
}

static void load_versions(struct menu_state *st)
{
    struct rrc_version version;
    struct rrc_result res = rrc_update_get_current_version(&version);
    if (rrc_result_is_error(res))
    {
        snprintf(st->vk_version, sizeof(st->vk_version), "VK ?");
        rrc_result_free(res);
    }
    else
    {
#if defined(RRC_BETA) && RRC_BETA >= 1
        snprintf(st->vk_version, sizeof(st->vk_version), "VK %i.%i.%i BETA", version.major, version.minor, version.patch);
#else
        snprintf(st->vk_version, sizeof(st->vk_version), "VK %i.%i.%i", version.major, version.minor, version.patch);
#endif
    }

    struct rrc_version internal_version = RRC_INTERNAL_VERSION;
    snprintf(st->channel_version, sizeof(st->channel_version), "Channel %i.%i.%i", internal_version.major, internal_version.minor, internal_version.patch);
}

static void move_selection(struct menu_state *st, struct pad_state pad)
{
    int i = st->selected;

    if (rrc_pad_right_pressed(pad))
    {
        i = (i + 1) % TILE_COUNT;
    }
    else if (rrc_pad_left_pressed(pad))
    {
        i = (i + TILE_COUNT - 1) % TILE_COUNT;
    }
    else if (rrc_pad_down_pressed(pad))
    {
        i += MENU_COLS;
        // wrap around to the top of the same column
        if (i >= TILE_COUNT)
            i %= MENU_COLS;
    }
    else if (rrc_pad_up_pressed(pad))
    {
        i -= MENU_COLS;
        if (i < 0)
        {
            // wrap around to the bottom of the same column, which may not exist in a partial last row
            i += ROW_COUNT * MENU_COLS;
            if (i >= TILE_COUNT)
                i -= MENU_COLS;
        }
    }

    st->selected = i;
}

/* Returns true if the channel should exit to apply installed updates. */
static bool run_updates(struct menu_state *st, void *xfb)
{
    clear_menu(st, xfb);

    int update_count;
    bool updated;
    struct rrc_result update_res = rrc_update_do_updates(xfb, &update_count, &updated);

    if (rrc_result_is_error(update_res))
    {
        rrc_result_error_check_error_normal(update_res, xfb);
        set_error_status(st, "Updating failed.");
    }
    else if (updated)
    {
        char status_message[64];
        snprintf(status_message, sizeof(status_message), "%d updates were installed.", update_count);
        char *lines[] = {status_message, "", "The channel will now exit to apply the updates."};
        rrc_prompt_1_option(xfb, lines, 3, "OK");
        return true;
    }
    else if (update_count == 0)
    {
        set_status(st, "No updates available.");
    }

    return false;
}

/* Time trial leaderboards. Downloading the ghosts themselves (see ghosts/ghosts.h) comes later. */
static void run_ghosts(struct menu_state *st, void *xfb)
{
    clear_menu(st, xfb);

    struct rrc_tt_tracks tracks;
    struct rrc_result res = rrc_tt_fetch_tracks(&tracks);
    if (rrc_result_is_error(res))
    {
        rrc_result_error_check_error_normal(res, xfb);
        set_error_status(st, "Could not load the leaderboards.");
        return;
    }

    if (tracks.count == 0)
        set_status(st, "No leaderboards yet.");
    else
        rrc_leaderboard_view_display(xfb, &tracks);

    rrc_tt_free_tracks(&tracks);
}

static void run_news(struct menu_state *st, void *xfb)
{
    clear_menu(st, xfb);

    struct rrc_news news;
    struct rrc_result res = rrc_news_fetch(&news);
    if (rrc_result_is_error(res))
    {
        rrc_result_error_check_error_normal(res, xfb);
        set_error_status(st, "Could not load the news.");
        return;
    }

    if (news.count == 0)
        set_status(st, "No news right now.");
    else
        rrc_news_view_display(xfb, &news);

    rrc_news_free(&news);
}

/* Loads the artwork of tile `i', resized to the tile. Without artwork the tile is drawn as an empty slot. */
static void load_tile_image(struct menu_state *st, int i)
{
    rrc_gfx_image_free(&st->images[i]);

    if (st->tiles[i].image == NULL)
        return;

    // A tile whose artwork fails to load just falls back to an empty slot.
    struct rrc_gfx_image decoded = {NULL, 0, 0};
    if (rrc_gfx_image_from_png(&decoded, st->tiles[i].image) != 0 ||
        rrc_gfx_image_resize(&st->images[i], &decoded, st->tile_w, st->tile_h) != 0)
    {
        rrc_dbg_printf("failed to load artwork for menu tile '%s'\n", st->tiles[i].label);
    }
    rrc_gfx_image_free(&decoded);
}

/* Shows "Install Channel" or "Remove Channel" depending on whether the channel is installed. */
static void refresh_channel_tile(struct menu_state *st)
{
    for (int i = 0; i < TILE_COUNT; i++)
    {
        if (menu_tiles[i].action != MENU_ACTION_INSTALL_CHANNEL)
            continue;

        const struct menu_tile *tile = rrc_channel_is_installed() ? &remove_channel_tile : &menu_tiles[i];
        if (st->tiles[i].action != tile->action)
        {
            st->tiles[i] = *tile;
            load_tile_image(st, i);
        }
    }
}

static void run_channel_action(struct menu_state *st, void *xfb, bool uninstall)
{
    char *lines[] = {
        uninstall ? "Remove the VanzaKart Launcher channel from the Wii Menu?"
               : "Install the VanzaKart Launcher channel to the Wii Menu?",
        "",
        uninstall ? "Your VanzaKart files on the SD card will not be touched."
               : "You can then start VanzaKart without the Homebrew Channel."};

    if (rrc_prompt_yes_no(xfb, lines, 3) != RRC_PROMPT_RESULT_YES)
        return;

    clear_menu(st, xfb);

    struct rrc_result res = uninstall ? rrc_channel_remove(xfb) : rrc_channel_install(xfb);
    if (rrc_result_is_error(res))
    {
        rrc_result_error_check_error_normal(res, xfb);
        set_error_status(st, uninstall ? "Removing the channel failed." : "Installing the channel failed.");
    }
    else
    {
        set_status(st, uninstall ? "Channel removed." : "Channel installed.");
    }

    refresh_channel_tile(st);
}

enum rrc_menu_result rrc_menu_display(void *xfb, struct rrc_settingsfile *stored_settings, bool allow_autolaunch)
{
    enum rrc_menu_result result;
    struct menu_state st;
    memset(&st, 0, sizeof(st));
    st.status_color = COLOR_STATUS;

    if (rrc_gfx_image_alloc(&st.canvas, rrc_gui_is_widescreen() ? MENU_W_WIDESCREEN : MENU_W, MENU_H) != 0)
    {
        RRC_FATAL("failed to allocate the menu canvas");
    }

    compute_tile_size(&st);

    for (int i = 0; i < TILE_COUNT; i++)
    {
        st.tiles[i] = menu_tiles[i];
        load_tile_image(&st, i);
    }
    refresh_channel_tile(&st);

    load_versions(&st);

    int autolaunch_ticks = allow_autolaunch ? AUTOLAUNCH_TIME / RRC_WPAD_LOOP_TIMEOUT : -1;
    int shown_seconds = -1;
    bool redraw = true;

    while (1)
    {
        rrc_shutdown_check();

        if (autolaunch_ticks == 0)
        {
            result = RRC_MENU_LAUNCH;
            goto out;
        }
        else if (autolaunch_ticks > 0)
        {
            int seconds = (autolaunch_ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
            if (seconds != shown_seconds)
            {
                char countdown[32];
                snprintf(countdown, sizeof(countdown), "Auto-launching in %d...", seconds);
                set_status(&st, countdown);
                shown_seconds = seconds;
                redraw = true;
            }
            autolaunch_ticks--;
        }

        if (redraw)
        {
            draw_menu(&st, xfb);
            redraw = false;
        }

        struct pad_state pad = rrc_pad_buttons();
        if (pad.wpad == 0 && pad.gc == 0)
        {
            usleep(RRC_WPAD_LOOP_TIMEOUT);
            continue;
        }

        // Any input means the user wants to stay in the menu.
        redraw = true;
        if (autolaunch_ticks > 0)
        {
            autolaunch_ticks = -1;
            set_status(&st, "");
        }

        if (rrc_pad_home_pressed(pad))
        {
            result = RRC_MENU_EXIT;
            goto out;
        }
        else if (rrc_pad_a_pressed(pad))
        {
            set_status(&st, "");

            switch (st.tiles[st.selected].action)
            {
            case MENU_ACTION_LAUNCH:
                result = RRC_MENU_LAUNCH;
                goto out;

            case MENU_ACTION_UPDATES:
                if (run_updates(&st, xfb))
                {
                    result = RRC_MENU_EXIT;
                    goto out;
                }
                break;

            case MENU_ACTION_SETTINGS:
            {
                clear_menu(&st, xfb);

                struct rrc_result r;
                enum rrc_settings_result settings_res = rrc_settings_display(xfb, stored_settings, &r);
                rrc_result_error_check_error_fatal(r);

                result = settings_res == RRC_SETTINGS_LAUNCH ? RRC_MENU_LAUNCH : RRC_MENU_EXIT;
                goto out;
            }

            case MENU_ACTION_GHOSTS:
                run_ghosts(&st, xfb);
                break;

            case MENU_ACTION_NEWS:
                run_news(&st, xfb);
                break;

            case MENU_ACTION_INSTALL_CHANNEL:
                run_channel_action(&st, xfb, false);
                break;

            case MENU_ACTION_REMOVE_CHANNEL:
                run_channel_action(&st, xfb, true);
                break;

            case MENU_ACTION_NONE:
                set_status(&st, "Coming soon!");
                break;
            }
        }
        else
        {
            move_selection(&st, pad);
        }

        usleep(RRC_WPAD_LOOP_TIMEOUT);
    }

out:
    clear_menu(&st, xfb);

    for (int i = 0; i < TILE_COUNT; i++)
    {
        rrc_gfx_image_free(&st.images[i]);
    }
    rrc_gfx_image_free(&st.canvas);

    return result;
}
