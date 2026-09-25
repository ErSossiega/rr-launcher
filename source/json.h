/*
    json.h - minimal JSON parser for server responses

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

#ifndef RRC_JSON_H
#define RRC_JSON_H

#include <stdbool.h>
#include <stddef.h>

enum rrc_json_type
{
    RRC_JSON_NULL,
    RRC_JSON_BOOL,
    RRC_JSON_NUMBER,
    RRC_JSON_STRING,
    RRC_JSON_ARRAY,
    RRC_JSON_OBJECT
};

/*
    A parsed JSON value. Arrays and objects hold their elements as a linked list starting at `child';
    object members carry their name in `key'.
*/
struct rrc_json
{
    enum rrc_json_type type;
    /* Member name if this value is inside an object, otherwise NULL. */
    char *key;
    bool boolean;
    double number;
    /* UTF-8 text, for strings. */
    char *string;
    /* First element or member, for arrays and objects. */
    struct rrc_json *child;
    /* Next element or member of the same array or object. */
    struct rrc_json *next;
};

/*
    Parses `len' bytes of JSON. Returns NULL if the text is not valid JSON (or memory runs out).
    The result must be freed with `rrc_json_free'.
*/
struct rrc_json *rrc_json_parse(const char *data, size_t len);

void rrc_json_free(struct rrc_json *json);

/* Returns the member `key' of `object', or NULL if there is none or `object' isn't an object. */
const struct rrc_json *rrc_json_get(const struct rrc_json *object, const char *key);

/* Returns the string member `key' of `object', or NULL if it is missing or not a string. */
const char *rrc_json_get_string(const struct rrc_json *object, const char *key);

/* Returns the number member `key' of `object', or `fallback' if it is missing or not a number. */
double rrc_json_get_number(const struct rrc_json *object, const char *key, double fallback);

/* Returns the boolean member `key' of `object', or `fallback' if it is missing or not a boolean. */
bool rrc_json_get_bool(const struct rrc_json *object, const char *key, bool fallback);

#endif
