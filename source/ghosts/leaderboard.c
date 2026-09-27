/*
    leaderboard.c - time trial leaderboards from the VanzaKart server

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "leaderboard.h"
#include "../http.h"
#include "../json.h"
#include "../text.h"

/*
    Same IDs as MarioKartMappings.cs in rwfc-web, with the Mii outfits shortened to fit a table column
    (S/M/L = size, A/B/C = outfit).
*/
static const char *const character_names[] = {
    "Mario", "Baby Peach", "Waluigi", "Bowser", "Baby Daisy", "Dry Bones", "Baby Mario", "Luigi",
    "Toad", "Donkey Kong", "Yoshi", "Wario", "Baby Luigi", "Toadette", "Koopa Troopa", "Daisy",
    "Peach", "Birdo", "Diddy Kong", "King Boo", "Bowser Jr.", "Dry Bowser", "Funky Kong", "Rosalina",
    "Mii S-A Male", "Mii S-A Fem.", "Mii S-B Male", "Mii S-B Fem.", "Mii S-C Male", "Mii S-C Fem.",
    "Mii M-A Male", "Mii M-A Fem.", "Mii M-B Male", "Mii M-B Fem.", "Mii M-C Male", "Mii M-C Fem.",
    "Mii L-A Male", "Mii L-A Fem.", "Mii L-B Male", "Mii L-B Fem.", "Mii L-C Male", "Mii L-C Fem.",
    "Medium Mii", "Small Mii", "Large Mii", "Peach", "Daisy", "Rosalina",
};

static const char *const vehicle_names[] = {
    "Standard Kart S", "Standard Kart M", "Standard Kart L", "Baby Booster", "Classic Dragster",
    "Offroader", "Mini Beast", "Wild Wing", "Flame Flyer", "Cheep Charger", "Super Blooper",
    "Piranha Prowler", "Tiny Titan", "Daytripper", "Jetsetter", "Blue Falcon", "Sprinter", "Honeycoupe",
    "Standard Bike S", "Standard Bike M", "Standard Bike L", "Bullet Bike", "Mach Bike", "Flame Runner",
    "Bit Bike", "Sugarscoot", "Wario Bike", "Quacker", "Zip Zip", "Shooting Star", "Magikruiser",
    "Sneakster", "Spear", "Jet Bubble", "Dolphin Dasher", "Phantom",
};

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

const char *rrc_tt_character_name(int id)
{
    return id >= 0 && id < COUNT_OF(character_names) ? character_names[id] : "?";
}

const char *rrc_tt_vehicle_name(int id)
{
    return id >= 0 && id < COUNT_OF(vehicle_names) ? vehicle_names[id] : "?";
}

/* A converted copy of the string member `key' of `object', or `fallback' if it is missing. */
static char *copy_text(const struct rrc_json *object, const char *key, const char *fallback)
{
    const char *value = rrc_json_get_string(object, key);
    char *copy = strdup(value != NULL ? value : fallback);
    if (copy != NULL)
        rrc_text_utf8_to_cp437(copy);
    return copy;
}

static int count_children(const struct rrc_json *array)
{
    int count = 0;
    for (const struct rrc_json *item = array->child; item != NULL; item = item->next)
        count++;
    return count;
}

/* Downloads and parses `url'; on success `*json' must be freed with `rrc_json_free'. */
static struct rrc_result fetch_json(const char *url, const char *label, struct rrc_json **json)
{
    char *data;
    size_t len;
    struct rrc_result res = rrc_http_get(url, label, &data, &len);
    if (rrc_result_is_error(res))
        return res;

    *json = rrc_json_parse(data, len);
    free(data);
    if (*json == NULL)
        return rrc_result_create_error_leaderboard("The server sent an invalid response.");

    return rrc_result_success;
}

/* Alphabetical, ignoring case; tracks with the same name keep a fixed order by id. */
static int compare_tracks(const void *a, const void *b)
{
    const struct rrc_tt_track *ta = a, *tb = b;
    int cmp = strcasecmp(ta->name, tb->name);
    if (cmp != 0)
        return cmp;
    return (ta->id > tb->id) - (ta->id < tb->id);
}

struct rrc_result rrc_tt_fetch_tracks(struct rrc_tt_tracks *tracks)
{
    tracks->tracks = NULL;
    tracks->count = 0;

    struct rrc_json *json;
    struct rrc_result res = fetch_json(RRC_TT_API_URL "/tracks", "Fetching Tracks", &json);
    if (rrc_result_is_error(res))
        return res;

    if (json->type != RRC_JSON_ARRAY)
    {
        rrc_json_free(json);
        return rrc_result_create_error_leaderboard("The server sent an invalid track list.");
    }

    int count = count_children(json);
    tracks->tracks = calloc(count > 0 ? count : 1, sizeof(*tracks->tracks));
    if (tracks->tracks == NULL)
    {
        rrc_json_free(json);
        return rrc_result_create_error_leaderboard("Out of memory reading the tracks.");
    }

    for (const struct rrc_json *item = json->child; item != NULL; item = item->next)
    {
        if (item->type != RRC_JSON_OBJECT)
            continue;

        struct rrc_tt_track *t = &tracks->tracks[tracks->count++];
        t->id = (int)rrc_json_get_number(item, "id", -1);
        t->name = copy_text(item, "name", "?");
        const char *category = rrc_json_get_string(item, "category");
        t->retro = category != NULL && strcmp(category, "Retro") == 0;

        if (t->name == NULL)
        {
            rrc_json_free(json);
            rrc_tt_free_tracks(tracks);
            return rrc_result_create_error_leaderboard("Out of memory reading the tracks.");
        }
    }

    rrc_json_free(json);
    qsort(tracks->tracks, tracks->count, sizeof(*tracks->tracks), compare_tracks);
    return rrc_result_success;
}

void rrc_tt_free_tracks(struct rrc_tt_tracks *tracks)
{
    for (int i = 0; i < tracks->count; i++)
        free(tracks->tracks[i].name);

    free(tracks->tracks);
    tracks->tracks = NULL;
    tracks->count = 0;
}

struct rrc_result rrc_tt_fetch_leaderboard(int track_id, struct rrc_tt_leaderboard *leaderboard)
{
    leaderboard->entries = NULL;
    leaderboard->count = 0;
    leaderboard->total = 0;

    char url[160];
    snprintf(url, sizeof(url), RRC_TT_API_URL "/leaderboard?trackId=%d&cc=%d&page=1&pageSize=%d",
             track_id, RRC_TT_CC, RRC_TT_LEADERBOARD_SIZE);

    struct rrc_json *json;
    struct rrc_result res = fetch_json(url, "Fetching Leaderboard", &json);
    if (rrc_result_is_error(res))
        return res;

    const struct rrc_json *submissions = rrc_json_get(json, "submissions");
    if (submissions == NULL || submissions->type != RRC_JSON_ARRAY)
    {
        rrc_json_free(json);
        return rrc_result_create_error_leaderboard("The server sent an invalid leaderboard.");
    }

    int count = count_children(submissions);
    leaderboard->total = (int)rrc_json_get_number(json, "totalSubmissions", count);
    leaderboard->entries = calloc(count > 0 ? count : 1, sizeof(*leaderboard->entries));
    if (leaderboard->entries == NULL)
    {
        rrc_json_free(json);
        return rrc_result_create_error_leaderboard("Out of memory reading the leaderboard.");
    }

    for (const struct rrc_json *item = submissions->child; item != NULL; item = item->next)
    {
        if (item->type != RRC_JSON_OBJECT)
            continue;

        struct rrc_tt_entry *e = &leaderboard->entries[leaderboard->count];
        e->id = (int)rrc_json_get_number(item, "id", -1);
        e->rank = (int)rrc_json_get_number(item, "rank", leaderboard->count + 1);
        // The Mii name stored in the ghost, rather than the profile name. Wii-only symbols in it
        // are dropped by the conversion, so fall back to the profile if nothing printable is left.
        e->player = copy_text(item, "miiName", "");
        if (e->player != NULL && e->player[strspn(e->player, " ")] == '\0')
        {
            free(e->player);
            e->player = copy_text(item, "playerName", "?");
        }

        const char *country = rrc_json_get_string(item, "countryAlpha2");
        if (country != NULL && isalpha((unsigned char)country[0]) && isalpha((unsigned char)country[1]) && country[2] == '\0')
        {
            e->country[0] = toupper((unsigned char)country[0]);
            e->country[1] = toupper((unsigned char)country[1]);
        }
        e->character = (int)rrc_json_get_number(item, "characterId", -1);
        e->vehicle = (int)rrc_json_get_number(item, "vehicleId", -1);
        e->time = copy_text(item, "finishTimeDisplay", "?");
        e->glitch = rrc_json_get_bool(item, "glitch", false);
        e->shroomless = rrc_json_get_bool(item, "shroomless", false);
        leaderboard->count++;

        if (e->player == NULL || e->time == NULL)
        {
            rrc_json_free(json);
            rrc_tt_free_leaderboard(leaderboard);
            return rrc_result_create_error_leaderboard("Out of memory reading the leaderboard.");
        }
    }

    rrc_json_free(json);
    return rrc_result_success;
}

void rrc_tt_free_leaderboard(struct rrc_tt_leaderboard *leaderboard)
{
    for (int i = 0; i < leaderboard->count; i++)
    {
        free(leaderboard->entries[i].player);
        free(leaderboard->entries[i].time);
    }

    free(leaderboard->entries);
    leaderboard->entries = NULL;
    leaderboard->count = 0;
    leaderboard->total = 0;
}
