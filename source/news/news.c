/*
    news.c - fetching the VanzaKart news feed

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

#include <stdlib.h>
#include <string.h>

#include "news.h"
#include "../http.h"
#include "../json.h"
#include "../text.h"

/* A converted copy of the string member `key' of `object', or an empty string if it is missing. */
static char *copy_text(const struct rrc_json *object, const char *key)
{
    const char *value = rrc_json_get_string(object, key);
    char *copy = strdup(value != NULL ? value : "");
    if (copy != NULL)
        rrc_text_utf8_to_cp437(copy);
    return copy;
}

struct rrc_result rrc_news_fetch(struct rrc_news *news)
{
    news->articles = NULL;
    news->count = 0;

    char *data;
    size_t len;
    struct rrc_result res = rrc_http_get(RRC_NEWS_URL, "Fetching News", &data, &len);
    if (rrc_result_is_error(res))
        return res;

    struct rrc_json *feed = rrc_json_parse(data, len);
    free(data);
    if (feed == NULL || feed->type != RRC_JSON_ARRAY)
    {
        rrc_json_free(feed);
        return rrc_result_create_error_news("The news feed is malformed.");
    }

    int count = 0;
    for (const struct rrc_json *item = feed->child; item != NULL; item = item->next)
        count++;

    news->articles = calloc(count > 0 ? count : 1, sizeof(*news->articles));
    if (news->articles == NULL)
    {
        rrc_json_free(feed);
        return rrc_result_create_error_news("Out of memory reading the news.");
    }

    for (const struct rrc_json *item = feed->child; item != NULL; item = item->next)
    {
        if (item->type != RRC_JSON_OBJECT)
            continue;

        struct rrc_news_article *a = &news->articles[news->count++];
        a->title = copy_text(item, "Title");
        a->category = copy_text(item, "Category");
        a->version = copy_text(item, "Version");
        a->date_label = copy_text(item, "DateLabel");
        a->summary = copy_text(item, "Summary");
        a->pinned = rrc_json_get_bool(item, "IsPinned", false);

        if (a->title == NULL || a->category == NULL || a->version == NULL || a->date_label == NULL || a->summary == NULL)
        {
            rrc_json_free(feed);
            rrc_news_free(news);
            return rrc_result_create_error_news("Out of memory reading the news.");
        }
    }

    rrc_json_free(feed);
    return rrc_result_success;
}

void rrc_news_free(struct rrc_news *news)
{
    for (int i = 0; i < news->count; i++)
    {
        struct rrc_news_article *a = &news->articles[i];
        free(a->title);
        free(a->category);
        free(a->version);
        free(a->date_label);
        free(a->summary);
    }

    free(news->articles);
    news->articles = NULL;
    news->count = 0;
}
