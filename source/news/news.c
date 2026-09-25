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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <gctypes.h>
#include <curl/curl.h>
#include <wiisocket.h>

#include "news.h"
#include "../console.h"

/* The feed is a few KB; anything much larger is not what we expect. */
#define NEWS_MAX_SIZE (1024 * 1024)

/* Nesting limit when skipping unknown JSON values. */
#define JSON_MAX_DEPTH 32

/*
    Code page 437 equivalents of U+00A0 to U+00FF (generated with Python's cp437 codec).
    Letters the font doesn't have fall back to their unaccented ASCII letter; 0 means dropped.
*/
static const u8 latin1_to_cp437[96] = {
    0x20, 0xAD, 0x9B, 0x9C, 0x00, 0x9D, 0x00, 0x00, /* U+00A0 */
    0x00, 0x00, 0xA6, 0xAE, 0xAA, 0x00, 0x00, 0x00, /* U+00A8 */
    0xF8, 0xF1, 0xFD, 0x00, 0x00, 0xE6, 0x00, 0xFA, /* U+00B0 */
    0x00, 0x00, 0xA7, 0xAF, 0xAC, 0xAB, 0x00, 0xA8, /* U+00B8 */
    0x41, 0x41, 0x41, 0x41, 0x8E, 0x8F, 0x92, 0x80, /* U+00C0 */
    0x45, 0x90, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49, /* U+00C8 */
    0x00, 0xA5, 0x4F, 0x4F, 0x4F, 0x4F, 0x99, 0x00, /* U+00D0 */
    0x00, 0x55, 0x55, 0x55, 0x9A, 0x59, 0x00, 0xE1, /* U+00D8 */
    0x85, 0xA0, 0x83, 0x61, 0x84, 0x86, 0x91, 0x87, /* U+00E0 */
    0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B, /* U+00E8 */
    0x00, 0xA4, 0x95, 0xA2, 0x93, 0x6F, 0x94, 0xF6, /* U+00F0 */
    0x00, 0x97, 0xA3, 0x96, 0x81, 0x79, 0x00, 0x98, /* U+00F8 */
};

/*
    Appends the code page 437 form of `cp' to `*out'. Never writes more bytes than the UTF-8 or
    JSON escape encoding of `cp' takes, so a buffer as long as the source string is enough.
*/
static void append_codepoint(char **out, u32 cp)
{
    if (cp == '\n')
        *(*out)++ = '\n';
    else if (cp == '\t')
        *(*out)++ = ' ';
    else if (cp < 0x20)
        return;
    else if (cp < 0x80)
        *(*out)++ = cp;
    else if (cp >= 0xA0 && cp <= 0xFF)
    {
        u8 c = latin1_to_cp437[cp - 0xA0];
        if (c != 0)
            *(*out)++ = c;
    }
    else
    {
        switch (cp)
        {
        case 0x2018: // curly single quotes
        case 0x2019:
            *(*out)++ = '\'';
            break;
        case 0x201C: // curly double quotes
        case 0x201D:
            *(*out)++ = '"';
            break;
        case 0x2013: // en and em dash
        case 0x2014:
            *(*out)++ = '-';
            break;
        case 0x2022: // bullet
            *(*out)++ = 0x07;
            break;
        case 0x2026: // ellipsis
            memcpy(*out, "...", 3);
            *out += 3;
            break;
        }
    }
}

/* Decodes one UTF-8 sequence at `*p', not reading past `end'. Invalid input yields U+FFFD. */
static u32 decode_utf8(const char **p, const char *end)
{
    unsigned char c = *(*p)++;
    if (c < 0x80)
        return c;

    int extra = c >= 0xF0 ? 3 : (c >= 0xE0 ? 2 : (c >= 0xC0 ? 1 : -1));
    if (extra < 0)
        return 0xFFFD;

    u32 cp = c & (0x3F >> extra);
    for (int i = 0; i < extra; i++)
    {
        if (*p >= end || (**p & 0xC0) != 0x80)
            return 0xFFFD;
        cp = (cp << 6) | (*(*p)++ & 0x3F);
    }

    return cp;
}

/*
    A minimal JSON reader, just enough for the news feed: an array of objects whose values we care
    about are strings and booleans. Everything else is skipped.
*/
struct json
{
    const char *p;
    const char *end;
};

static void skip_whitespace(struct json *j)
{
    while (j->p < j->end && (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r'))
        j->p++;
}

static bool peek(struct json *j, char c)
{
    skip_whitespace(j);
    return j->p < j->end && *j->p == c;
}

static bool consume(struct json *j, char c)
{
    if (!peek(j, c))
        return false;

    j->p++;
    return true;
}

/* Reads 4 hex digits at `j->p' (before `limit'), or returns U+FFFD. */
static u32 parse_hex4(struct json *j, const char *limit)
{
    if (limit - j->p < 4)
        return 0xFFFD;

    u32 v = 0;
    for (int i = 0; i < 4; i++)
    {
        char c = *j->p++;
        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= c - '0';
        else if (c >= 'a' && c <= 'f')
            v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            v |= c - 'A' + 10;
        else
            return 0xFFFD;
    }

    return v;
}

/* Parses a string into a newly allocated, converted copy (see `struct rrc_news_article'). NULL on error. */
static char *parse_string(struct json *j)
{
    if (!consume(j, '"'))
        return NULL;

    // Find the closing quote first to size the buffer. UTF-8 continuation bytes are never '"'.
    const char *close = j->p;
    while (close < j->end && *close != '"')
    {
        if (*close == '\\')
            close++;
        close++;
    }
    if (close >= j->end)
        return NULL;

    char *result = malloc(close - j->p + 1);
    if (result == NULL)
        return NULL;
    char *out = result;

    while (j->p < close)
    {
        u32 cp;
        if (*j->p != '\\')
        {
            cp = decode_utf8(&j->p, close);
        }
        else
        {
            j->p++;
            char escaped = *j->p++;
            switch (escaped)
            {
            case 'n':
                cp = '\n';
                break;
            case 't':
                cp = '\t';
                break;
            case 'b':
            case 'f':
            case 'r':
                cp = 0;
                break;
            case 'u':
                cp = parse_hex4(j, close);
                // combine a surrogate pair
                if (cp >= 0xD800 && cp <= 0xDBFF && close - j->p >= 6 && j->p[0] == '\\' && j->p[1] == 'u')
                {
                    struct json low_j = {j->p + 2, close};
                    u32 low = parse_hex4(&low_j, close);
                    if (low >= 0xDC00 && low <= 0xDFFF)
                    {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        j->p += 6;
                    }
                }
                break;
            default: // \" \\ \/
                cp = escaped;
                break;
            }
        }

        append_codepoint(&out, cp);
    }

    *out = '\0';
    j->p = close + 1;
    return result;
}

static bool skip_value(struct json *j, int depth)
{
    skip_whitespace(j);
    if (j->p >= j->end || depth > JSON_MAX_DEPTH)
        return false;

    char c = *j->p;
    if (c == '"')
    {
        char *s = parse_string(j);
        free(s);
        return s != NULL;
    }

    if (c == '{' || c == '[')
    {
        char close = c == '{' ? '}' : ']';
        j->p++;
        if (consume(j, close))
            return true;

        do
        {
            if (c == '{')
            {
                char *key = parse_string(j);
                free(key);
                if (key == NULL || !consume(j, ':'))
                    return false;
            }

            if (!skip_value(j, depth + 1))
                return false;
        } while (consume(j, ','));

        return consume(j, close);
    }

    // number, true, false or null
    const char *start = j->p;
    while (j->p < j->end && (isalnum((unsigned char)*j->p) || *j->p == '-' || *j->p == '+' || *j->p == '.'))
        j->p++;

    return j->p > start;
}

static bool parse_article(struct json *j, struct rrc_news_article *article)
{
    if (!consume(j, '{'))
        return false;
    if (consume(j, '}'))
        return true;

    do
    {
        char *key = parse_string(j);
        if (key == NULL || !consume(j, ':'))
        {
            free(key);
            return false;
        }

        char **field = NULL;
        if (strcmp(key, "Title") == 0)
            field = &article->title;
        else if (strcmp(key, "Category") == 0)
            field = &article->category;
        else if (strcmp(key, "Version") == 0)
            field = &article->version;
        else if (strcmp(key, "DateLabel") == 0)
            field = &article->date_label;
        else if (strcmp(key, "Summary") == 0)
            field = &article->summary;

        bool ok;
        if (field != NULL && peek(j, '"'))
        {
            free(*field);
            *field = parse_string(j);
            ok = *field != NULL;
        }
        else if (strcmp(key, "IsPinned") == 0 && peek(j, 't'))
        {
            article->pinned = true;
            ok = skip_value(j, 0);
        }
        else
        {
            ok = skip_value(j, 0);
        }

        free(key);
        if (!ok)
            return false;
    } while (consume(j, ','));

    return consume(j, '}');
}

static struct rrc_result parse_feed(const char *data, size_t len, struct rrc_news *news)
{
    struct json j = {data, data + len};

    // UTF-8 byte order mark, as some editors on Windows add
    if (len >= 3 && memcmp(data, "\xEF\xBB\xBF", 3) == 0)
        j.p += 3;

    if (!consume(&j, '['))
        return rrc_result_create_error_news("The news feed is not a list.");

    int capacity = 0;
    while (!peek(&j, ']'))
    {
        if (news->count > 0 && !consume(&j, ','))
            return rrc_result_create_error_news("The news feed is malformed.");
        // tolerate a trailing comma
        if (peek(&j, ']'))
            break;

        if (news->count == capacity)
        {
            int new_capacity = capacity == 0 ? 8 : capacity * 2;
            struct rrc_news_article *grown = realloc(news->articles, new_capacity * sizeof(*grown));
            if (grown == NULL)
                return rrc_result_create_error_news("Out of memory reading the news.");

            memset(grown + capacity, 0, (new_capacity - capacity) * sizeof(*grown));
            news->articles = grown;
            capacity = new_capacity;
        }

        // Count it before parsing so a partially parsed article is freed too.
        if (!parse_article(&j, &news->articles[news->count++]))
            return rrc_result_create_error_news("The news feed is malformed.");
    }

    consume(&j, ']');
    return rrc_result_success;
}

struct download
{
    char *data;
    size_t len;
};

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct download *d = userdata;
    size_t n = size * nmemb;

    // returning less than `n' aborts the transfer
    if (d->len + n > NEWS_MAX_SIZE)
        return 0;

    char *grown = realloc(d->data, d->len + n + 1);
    if (grown == NULL)
        return 0;

    d->data = grown;
    memcpy(d->data + d->len, ptr, n);
    d->len += n;
    d->data[d->len] = '\0';
    return n;
}

static int last_progress;

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    if (dltotal > 0)
    {
        int progress = (int)(dlnow * 100 / dltotal);
        if (progress != last_progress)
        {
            last_progress = progress;
            rrc_con_update("Fetching News", progress);
        }
    }

    return 0;
}

static char *empty_if_null(char *s)
{
    return s != NULL ? s : strdup("");
}

struct rrc_result rrc_news_fetch(struct rrc_news *news)
{
    news->articles = NULL;
    news->count = 0;

    rrc_con_clear(true);
    rrc_con_update("Prepare Network", 0);
    if (wiisocket_init() < 0)
    {
        return rrc_result_create_error_news("Could not connect to the internet.");
    }

    CURL *curl = curl_easy_init();
    if (curl == NULL)
    {
        return rrc_result_create_error_news("Failed to start the download.");
    }

    struct download d = {NULL, 0};
    last_progress = -1;
    rrc_con_update("Fetching News", 0);

    curl_easy_setopt(curl, CURLOPT_URL, RRC_NEWS_URL);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &d);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    // The console has no CA certificates to check the server against (and its clock may be off).
    // The feed is only displayed, never executed or saved, so an unverified connection is acceptable.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode cres = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (cres != CURLE_OK)
    {
        free(d.data);
        return rrc_result_create_error_curl(cres, "Failed to download the news.");
    }

    rrc_con_update("Reading News", 100);

    if (d.data == NULL)
    {
        // empty response: no news
        return rrc_result_success;
    }

    struct rrc_result res = parse_feed(d.data, d.len, news);
    free(d.data);
    if (rrc_result_is_error(res))
    {
        rrc_news_free(news);
        return res;
    }

    for (int i = 0; i < news->count; i++)
    {
        struct rrc_news_article *a = &news->articles[i];
        a->title = empty_if_null(a->title);
        a->category = empty_if_null(a->category);
        a->version = empty_if_null(a->version);
        a->date_label = empty_if_null(a->date_label);
        a->summary = empty_if_null(a->summary);
    }

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
