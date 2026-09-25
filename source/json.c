/*
    json.c - minimal JSON parser for server responses

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
#include <gctypes.h>

#include "json.h"

/* Nesting limit, so malicious input can't exhaust the stack. */
#define JSON_MAX_DEPTH 64

struct parser
{
    const char *p;
    const char *end;
    int depth;
};

static void skip_whitespace(struct parser *ps)
{
    while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r'))
        ps->p++;
}

static bool peek(struct parser *ps, char c)
{
    skip_whitespace(ps);
    return ps->p < ps->end && *ps->p == c;
}

static bool consume(struct parser *ps, char c)
{
    if (!peek(ps, c))
        return false;

    ps->p++;
    return true;
}

static bool consume_word(struct parser *ps, const char *word)
{
    size_t len = strlen(word);
    if ((size_t)(ps->end - ps->p) < len || memcmp(ps->p, word, len) != 0)
        return false;

    ps->p += len;
    return true;
}

/* Reads 4 hex digits, or returns -1. */
static int parse_hex4(const char *p, const char *end)
{
    if (end - p < 4)
        return -1;

    int v = 0;
    for (int i = 0; i < 4; i++)
    {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= c - '0';
        else if (c >= 'a' && c <= 'f')
            v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            v |= c - 'A' + 10;
        else
            return -1;
    }

    return v;
}

static void encode_utf8(char **out, u32 cp)
{
    char *o = *out;
    if (cp < 0x80)
    {
        *o++ = cp;
    }
    else if (cp < 0x800)
    {
        *o++ = 0xC0 | (cp >> 6);
        *o++ = 0x80 | (cp & 0x3F);
    }
    else if (cp < 0x10000)
    {
        *o++ = 0xE0 | (cp >> 12);
        *o++ = 0x80 | ((cp >> 6) & 0x3F);
        *o++ = 0x80 | (cp & 0x3F);
    }
    else
    {
        *o++ = 0xF0 | (cp >> 18);
        *o++ = 0x80 | ((cp >> 12) & 0x3F);
        *o++ = 0x80 | ((cp >> 6) & 0x3F);
        *o++ = 0x80 | (cp & 0x3F);
    }
    *out = o;
}

/* Parses a string (at its opening quote) into a newly allocated UTF-8 copy. NULL on error. */
static char *parse_string(struct parser *ps)
{
    if (!consume(ps, '"'))
        return NULL;

    // Find the closing quote first to size the buffer; unescaping never makes the text longer.
    const char *close = ps->p;
    while (close < ps->end && *close != '"')
    {
        if (*close == '\\')
            close++;
        close++;
    }
    if (close >= ps->end)
        return NULL;

    char *result = malloc(close - ps->p + 1);
    if (result == NULL)
        return NULL;
    char *out = result;

    while (ps->p < close)
    {
        if (*ps->p != '\\')
        {
            *out++ = *ps->p++;
            continue;
        }

        ps->p++;
        char escaped = *ps->p++;
        switch (escaped)
        {
        case 'b':
            *out++ = '\b';
            break;
        case 'f':
            *out++ = '\f';
            break;
        case 'n':
            *out++ = '\n';
            break;
        case 'r':
            *out++ = '\r';
            break;
        case 't':
            *out++ = '\t';
            break;
        case 'u':
        {
            int cp = parse_hex4(ps->p, close);
            if (cp < 0)
            {
                free(result);
                return NULL;
            }
            ps->p += 4;

            if (cp >= 0xD800 && cp <= 0xDBFF && close - ps->p >= 6 && ps->p[0] == '\\' && ps->p[1] == 'u')
            {
                int low = parse_hex4(ps->p + 2, close);
                if (low >= 0xDC00 && low <= 0xDFFF)
                {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    ps->p += 6;
                }
            }

            // a lone surrogate is not a character
            if (cp >= 0xD800 && cp <= 0xDFFF)
                cp = 0xFFFD;

            encode_utf8(&out, cp);
            break;
        }
        default: // \" \\ \/
            *out++ = escaped;
            break;
        }
    }

    *out = '\0';
    ps->p = close + 1;
    return result;
}

static bool parse_number(struct parser *ps, double *number)
{
    // copy it out: the input is not necessarily NUL-terminated
    char buf[64];
    size_t n = 0;
    while (ps->p < ps->end && n < sizeof(buf) - 1 &&
           ((*ps->p >= '0' && *ps->p <= '9') || *ps->p == '-' || *ps->p == '+' || *ps->p == '.' || *ps->p == 'e' || *ps->p == 'E'))
    {
        buf[n++] = *ps->p++;
    }
    buf[n] = '\0';

    char *endptr;
    *number = strtod(buf, &endptr);
    return n > 0 && *endptr == '\0';
}

static struct rrc_json *parse_value(struct parser *ps);

/* Parses the elements (or members) of an array (or object) after its opening bracket. */
static bool parse_children(struct parser *ps, struct rrc_json *parent, bool object)
{
    char close = object ? '}' : ']';
    if (consume(ps, close))
        return true;

    struct rrc_json **tail = &parent->child;
    do
    {
        char *key = NULL;
        if (object)
        {
            key = parse_string(ps);
            if (key == NULL || !consume(ps, ':'))
            {
                free(key);
                return false;
            }
        }

        struct rrc_json *child = parse_value(ps);
        if (child == NULL)
        {
            free(key);
            return false;
        }

        child->key = key;
        *tail = child;
        tail = &child->next;
    } while (consume(ps, ','));

    return consume(ps, close);
}

static struct rrc_json *parse_value(struct parser *ps)
{
    skip_whitespace(ps);
    if (ps->p >= ps->end || ps->depth > JSON_MAX_DEPTH)
        return NULL;

    struct rrc_json *value = calloc(1, sizeof(*value));
    if (value == NULL)
        return NULL;

    bool ok;
    char c = *ps->p;
    if (c == '{' || c == '[')
    {
        value->type = c == '{' ? RRC_JSON_OBJECT : RRC_JSON_ARRAY;
        ps->p++;
        ps->depth++;
        ok = parse_children(ps, value, c == '{');
        ps->depth--;
    }
    else if (c == '"')
    {
        value->type = RRC_JSON_STRING;
        value->string = parse_string(ps);
        ok = value->string != NULL;
    }
    else if (consume_word(ps, "true"))
    {
        value->type = RRC_JSON_BOOL;
        value->boolean = true;
        ok = true;
    }
    else if (consume_word(ps, "false"))
    {
        value->type = RRC_JSON_BOOL;
        value->boolean = false;
        ok = true;
    }
    else if (consume_word(ps, "null"))
    {
        value->type = RRC_JSON_NULL;
        ok = true;
    }
    else
    {
        value->type = RRC_JSON_NUMBER;
        ok = parse_number(ps, &value->number);
    }

    if (!ok)
    {
        rrc_json_free(value);
        return NULL;
    }

    return value;
}

struct rrc_json *rrc_json_parse(const char *data, size_t len)
{
    struct parser ps = {data, data + len, 0};

    // UTF-8 byte order mark, as some editors on Windows add
    if (len >= 3 && memcmp(data, "\xEF\xBB\xBF", 3) == 0)
        ps.p += 3;

    struct rrc_json *root = parse_value(&ps);
    skip_whitespace(&ps);
    if (root != NULL && ps.p != ps.end)
    {
        // trailing garbage
        rrc_json_free(root);
        return NULL;
    }

    return root;
}

void rrc_json_free(struct rrc_json *json)
{
    // walk siblings iteratively so long arrays don't recurse deeply
    while (json != NULL)
    {
        struct rrc_json *next = json->next;
        rrc_json_free(json->child);
        free(json->key);
        free(json->string);
        free(json);
        json = next;
    }
}

const struct rrc_json *rrc_json_get(const struct rrc_json *object, const char *key)
{
    if (object == NULL || object->type != RRC_JSON_OBJECT)
        return NULL;

    for (const struct rrc_json *member = object->child; member != NULL; member = member->next)
    {
        if (strcmp(member->key, key) == 0)
            return member;
    }

    return NULL;
}

const char *rrc_json_get_string(const struct rrc_json *object, const char *key)
{
    const struct rrc_json *member = rrc_json_get(object, key);
    return member != NULL && member->type == RRC_JSON_STRING ? member->string : NULL;
}

double rrc_json_get_number(const struct rrc_json *object, const char *key, double fallback)
{
    const struct rrc_json *member = rrc_json_get(object, key);
    return member != NULL && member->type == RRC_JSON_NUMBER ? member->number : fallback;
}

bool rrc_json_get_bool(const struct rrc_json *object, const char *key, bool fallback)
{
    const struct rrc_json *member = rrc_json_get(object, key);
    return member != NULL && member->type == RRC_JSON_BOOL ? member->boolean : fallback;
}
