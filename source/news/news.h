/*
    news.h - fetching the VanzaKart news feed

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

#ifndef RRC_NEWS_H
#define RRC_NEWS_H

#include <stdbool.h>
#include "../result.h"

/* The same feed the PC launcher shows (see `news_url' in Launcher/endpoints.json on the server). */
#define RRC_NEWS_URL "https://vanzakart.net:8443/Launcher/news.json"

/*
    One entry of the feed. All strings are converted from UTF-8 to the character set of the console
    font (code page 437): accented letters are kept where the font has them, typographic punctuation
    is replaced by its ASCII equivalent and anything else (e.g. emoji) is dropped.
*/
struct rrc_news_article
{
    char *title;
    char *category;
    char *version;
    char *date_label;
    /* Markdown text. */
    char *summary;
    bool pinned;
};

struct rrc_news
{
    struct rrc_news_article *articles;
    int count;
};

/*
    Downloads and parses the news feed, showing progress on the console.

    On success `news' must be freed with `rrc_news_free'; on error it is left empty.
*/
struct rrc_result rrc_news_fetch(struct rrc_news *news);

void rrc_news_free(struct rrc_news *news);

#endif
