/*
    news_view.c - news reader screen

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
#include <strings.h>
#include <unistd.h>

#include "news_view.h"
#include "../gfx.h"
#include "../gui.h"
#include "../pad.h"
#include "../shutdown.h"
#include "../util.h"

#define VIEW_PAD 12
#define LINE_H 18
#define BADGE_H 20
#define TITLE_MAX_LINES 2
/* Lines scrolled per D-Pad press. */
#define SCROLL_STEP 3

/* Longest line in characters: the 16:9 canvas fits about 80. */
#define LINE_MAX_CHARS 96

#define COLOR_BACKGROUND RRC_GFX_RGBA(0, 0, 0, 255)
#define COLOR_TITLE RRC_GFX_RGBA(255, 255, 255, 255)
#define COLOR_BODY RRC_GFX_RGBA(205, 205, 215, 255)
#define COLOR_HEADING RRC_GFX_RGBA(255, 220, 90, 255)
#define COLOR_SUBHEADING RRC_GFX_RGBA(120, 220, 255, 255)
#define COLOR_META RRC_GFX_RGBA(150, 150, 160, 255)
#define COLOR_RULE RRC_GFX_RGBA(80, 80, 95, 255)
#define COLOR_BADGE_TEXT RRC_GFX_RGBA(255, 255, 255, 255)

/* Code page 437 arrows, used as scroll indicators. */
#define GLYPH_UP "\x18"
#define GLYPH_DOWN "\x19"
#define GLYPH_BULLET "\x07"

struct view_line
{
    char text[LINE_MAX_CHARS + 1];
    u32 color;
    /* In characters. */
    int indent;
    /* A horizontal separator instead of text. */
    bool rule;
};

struct line_list
{
    struct view_line *lines;
    int count;
    int capacity;
};

static struct view_line *add_line(struct line_list *list)
{
    if (list->count == list->capacity)
    {
        int capacity = list->capacity == 0 ? 32 : list->capacity * 2;
        struct view_line *grown = realloc(list->lines, capacity * sizeof(*grown));
        if (grown == NULL)
            return NULL;

        list->lines = grown;
        list->capacity = capacity;
    }

    struct view_line *line = &list->lines[list->count++];
    memset(line, 0, sizeof(*line));
    return line;
}

/*
    Adds `text' word-wrapped to `cols' characters. The first line starts at `first_indent' characters,
    the following ones at `indent'.
*/
static void add_wrapped(struct line_list *list, const char *text, int cols, int first_indent, int indent, u32 color)
{
    const char *p = text;
    bool first = true;

    do
    {
        int line_indent = first ? first_indent : indent;
        int avail = cols - line_indent;
        if (avail > LINE_MAX_CHARS)
            avail = LINE_MAX_CHARS;
        if (avail < 8)
            avail = 8;

        if (!first)
        {
            while (*p == ' ')
                p++;
            if (*p == '\0')
                break;
        }

        int take = strlen(p);
        if (take > avail)
        {
            // break at the last space that fits, or mid-word if there is none
            take = avail;
            for (int i = avail; i > 0; i--)
            {
                if (p[i] == ' ')
                {
                    take = i;
                    break;
                }
            }
        }

        struct view_line *line = add_line(list);
        if (line == NULL)
            return;

        memcpy(line->text, p, take);
        line->text[take] = '\0';
        line->color = color;
        line->indent = line_indent;

        p += take;
        first = false;
    } while (*p != '\0');
}

/* Removes Markdown emphasis and code markers, which the font can't render. */
static void strip_inline_markup(char *s)
{
    char *out = s;
    for (char *p = s; *p != '\0'; p++)
    {
        if (*p != '*' && *p != '`')
            *out++ = *p;
    }
    *out = '\0';
}

/* "---", "***" or "___" on its own line. */
static bool is_rule(const char *s)
{
    if (s[0] != '-' && s[0] != '*' && s[0] != '_')
        return false;

    int n = 0;
    for (; s[n] != '\0'; n++)
    {
        if (s[n] != s[0])
            return false;
    }

    return n >= 3;
}

/* Turns the Markdown summary into display lines: headings, bullet lists, separators and paragraphs. */
static void layout_summary(struct line_list *list, const char *summary, int cols)
{
    list->count = 0;

    char *copy = strdup(summary);
    if (copy == NULL)
        return;

    bool prev_blank = true;
    for (char *line = copy; line != NULL;)
    {
        char *next = strchr(line, '\n');
        if (next != NULL)
            *next++ = '\0';

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\r'))
            line[--len] = '\0';

        char *s = line;
        while (*s == ' ')
            s++;

        if (*s == '\0')
        {
            // collapse runs of blank lines
            if (!prev_blank)
                add_line(list);
            prev_blank = true;
        }
        else if (is_rule(s))
        {
            struct view_line *rule = add_line(list);
            if (rule != NULL)
                rule->rule = true;
            prev_blank = false;
        }
        else if (s[0] == '#')
        {
            int level = 0;
            while (s[level] == '#')
                level++;
            s += level;
            while (*s == ' ')
                s++;

            strip_inline_markup(s);
            add_wrapped(list, s, cols, 0, 0, level == 1 ? COLOR_HEADING : COLOR_SUBHEADING);
            prev_blank = false;
        }
        else if ((s[0] == '-' || s[0] == '*' || s[0] == '+') && s[1] == ' ')
        {
            s += 2;
            while (*s == ' ')
                s++;
            strip_inline_markup(s);

            char bullet[strlen(s) + 3];
            snprintf(bullet, sizeof(bullet), GLYPH_BULLET " %s", s);
            add_wrapped(list, bullet, cols, 1, 3, COLOR_BODY);
            prev_blank = false;
        }
        else
        {
            strip_inline_markup(s);
            add_wrapped(list, s, cols, 0, 0, COLOR_BODY);
            prev_blank = false;
        }

        line = next;
    }

    while (list->count > 0 && list->lines[list->count - 1].text[0] == '\0' && !list->lines[list->count - 1].rule)
        list->count--;

    free(copy);
}

static u32 category_color(const char *category)
{
    if (strcasecmp(category, "UPDATE") == 0)
        return RRC_GFX_RGBA(50, 160, 80, 255);
    if (strcasecmp(category, "SHOWCASE") == 0)
        return RRC_GFX_RGBA(60, 120, 215, 255);
    if (strcasecmp(category, "COMMUNITY") == 0)
        return RRC_GFX_RGBA(220, 130, 30, 255);
    return RRC_GFX_RGBA(100, 100, 115, 255);
}

struct view
{
    struct rrc_gfx_image canvas;
    const struct rrc_news *news;
    int index;
    int scroll;
    int cols;
    struct line_list title;
    struct line_list body;
    int body_top;
    int visible_lines;
};

static int max_scroll(const struct view *v)
{
    int m = v->body.count - v->visible_lines;
    return m > 0 ? m : 0;
}

static void layout(struct view *v)
{
    const struct rrc_news_article *a = &v->news->articles[v->index];

    v->title.count = 0;
    add_wrapped(&v->title, a->title, v->cols, 0, 0, COLOR_TITLE);
    if (v->title.count > TITLE_MAX_LINES)
        v->title.count = TITLE_MAX_LINES;

    layout_summary(&v->body, a->summary, v->cols);

    int title_top = VIEW_PAD + BADGE_H + 6;
    v->body_top = title_top + v->title.count * LINE_H + 10;

    int footer_top = v->canvas.height - VIEW_PAD - RRC_GFX_FONT_H - 6;
    v->visible_lines = (footer_top - v->body_top) / LINE_H;
    v->scroll = 0;
}

static void draw(struct view *v, void *xfb)
{
    const struct rrc_news_article *a = &v->news->articles[v->index];
    struct rrc_gfx_image *c = &v->canvas;
    int right = c->width - VIEW_PAD;

    rrc_gfx_clear(c, COLOR_BACKGROUND);

    // header: category badge, version and date, position in the feed
    int x = VIEW_PAD;
    if (a->category[0] != '\0')
    {
        int badge_w = rrc_gfx_text_width(a->category, 1) + 12;
        rrc_gfx_fill_rounded_rect(c, x, VIEW_PAD, badge_w, BADGE_H, 6, category_color(a->category), category_color(a->category));
        rrc_gfx_draw_text(c, x + 6, VIEW_PAD + 2, a->category, 1, COLOR_BADGE_TEXT);
        x += badge_w + 8;
    }

    char meta[128];
    if (a->version[0] != '\0' && a->date_label[0] != '\0')
        snprintf(meta, sizeof(meta), "%s - %s", a->version, a->date_label);
    else
        snprintf(meta, sizeof(meta), "%s%s", a->version, a->date_label);
    rrc_gfx_draw_text(c, x, VIEW_PAD + 2, meta, 1, COLOR_META);

    char position[16];
    snprintf(position, sizeof(position), "%d/%d", v->index + 1, v->news->count);
    rrc_gfx_draw_text(c, right - rrc_gfx_text_width(position, 1), VIEW_PAD + 2, position, 1, COLOR_META);

    int title_top = VIEW_PAD + BADGE_H + 6;
    for (int i = 0; i < v->title.count; i++)
        rrc_gfx_draw_text(c, VIEW_PAD, title_top + i * LINE_H, v->title.lines[i].text, 1, COLOR_TITLE);

    rrc_gfx_fill_rounded_rect(c, VIEW_PAD, v->body_top - 6, c->width - 2 * VIEW_PAD, 1, 0, COLOR_RULE, COLOR_RULE);

    // body
    for (int i = 0; i < v->visible_lines && v->scroll + i < v->body.count; i++)
    {
        const struct view_line *line = &v->body.lines[v->scroll + i];
        int y = v->body_top + i * LINE_H;

        if (line->rule)
            rrc_gfx_fill_rounded_rect(c, VIEW_PAD, y + LINE_H / 2 - 1, c->width - 2 * VIEW_PAD, 1, 0, COLOR_RULE, COLOR_RULE);
        else
            rrc_gfx_draw_text(c, VIEW_PAD + line->indent * RRC_GFX_FONT_W, y, line->text, 1, line->color);
    }

    // footer: controls and scroll indicators
    int footer_y = c->height - VIEW_PAD - RRC_GFX_FONT_H;
    rrc_gfx_draw_text(c, VIEW_PAD, footer_y, "Left/Right: News   Up/Down: Scroll   B: Back", 1, COLOR_META);

    u32 up_color = v->scroll > 0 ? COLOR_TITLE : COLOR_RULE;
    u32 down_color = v->scroll < max_scroll(v) ? COLOR_TITLE : COLOR_RULE;
    rrc_gfx_draw_text(c, right - 2 * RRC_GFX_FONT_W, footer_y, GLYPH_UP, 1, up_color);
    rrc_gfx_draw_text(c, right - RRC_GFX_FONT_W, footer_y, GLYPH_DOWN, 1, down_color);

    rrc_gfx_present(c, xfb, rrc_gui_get_video_mode(), RRC_GUI_BOX_X, RRC_GUI_BOX_Y, RRC_GUI_BOX_W);
}

void rrc_news_view_display(void *xfb, const struct rrc_news *news)
{
    if (news->count == 0)
        return;

    struct view v;
    memset(&v, 0, sizeof(v));
    v.news = news;

    if (rrc_gfx_image_alloc(&v.canvas, rrc_gui_is_widescreen() ? RRC_GUI_BOX_W_WIDESCREEN : RRC_GUI_BOX_W, RRC_GUI_BOX_H) != 0)
    {
        return;
    }

    v.cols = (v.canvas.width - 2 * VIEW_PAD) / RRC_GFX_FONT_W;
    if (v.cols > LINE_MAX_CHARS)
        v.cols = LINE_MAX_CHARS;

    layout(&v);
    bool redraw = true;

    while (1)
    {
        rrc_shutdown_check();

        if (redraw)
        {
            draw(&v, xfb);
            redraw = false;
        }

        struct pad_state pad = rrc_pad_buttons();

        if (rrc_pad_b_pressed(pad) || rrc_pad_home_pressed(pad))
        {
            break;
        }
        else if (rrc_pad_right_pressed(pad) || rrc_pad_left_pressed(pad))
        {
            int step = rrc_pad_right_pressed(pad) ? 1 : news->count - 1;
            v.index = (v.index + step) % news->count;
            layout(&v);
            redraw = true;
        }
        else if (rrc_pad_down_pressed(pad) || rrc_pad_up_pressed(pad))
        {
            int scroll = v.scroll + (rrc_pad_down_pressed(pad) ? SCROLL_STEP : -SCROLL_STEP);
            if (scroll > max_scroll(&v))
                scroll = max_scroll(&v);
            if (scroll < 0)
                scroll = 0;

            redraw = scroll != v.scroll;
            v.scroll = scroll;
        }

        usleep(RRC_WPAD_LOOP_TIMEOUT);
    }

    free(v.title.lines);
    free(v.body.lines);
    rrc_gfx_image_free(&v.canvas);
}
