/*
    leaderboard.h - time trial leaderboards from the VanzaKart server

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

#ifndef RRC_LEADERBOARD_H
#define RRC_LEADERBOARD_H

#include <stdbool.h>
#include "../result.h"

/* Time trial API of the rwfc-web backend. */
#define RRC_TT_API_URL "https://vanzakart.net:8443/api/timetrial"

/* Only 150cc leaderboards are shown for now. */
#define RRC_TT_CC 150

/* Entries fetched per leaderboard (the API allows up to 100 per page). */
#define RRC_TT_LEADERBOARD_SIZE 50

/* Strings are converted to the console font's character set, see `rrc_text_utf8_to_cp437'. */
struct rrc_tt_track
{
    int id;
    char *name;
    /* "Retro" or "Custom". */
    bool retro;
};

struct rrc_tt_tracks
{
    struct rrc_tt_track *tracks;
    int count;
};

struct rrc_tt_entry
{
    /* Submission id, used to download the ghost. */
    int id;
    int rank;
    /* Mii name stored in the ghost, or the name of the player's time trial profile if it has none. */
    char *player;
    /* ISO 3166 two-letter country code of the profile (e.g. "IT"), or empty if it has none. */
    char country[3];
    int character;
    int vehicle;
    /* e.g. "1:23.456" */
    char *time;
    bool glitch;
    bool shroomless;
};

struct rrc_tt_leaderboard
{
    struct rrc_tt_entry *entries;
    int count;
    /* Times on the leaderboard, which may be more than `count'. */
    int total;
};

/* Downloads the tracks that have leaderboards, in the server's display order. */
struct rrc_result rrc_tt_fetch_tracks(struct rrc_tt_tracks *tracks);

void rrc_tt_free_tracks(struct rrc_tt_tracks *tracks);

/* Downloads the top RRC_TT_LEADERBOARD_SIZE times at RRC_TT_CC on track `track_id'. */
struct rrc_result rrc_tt_fetch_leaderboard(int track_id, struct rrc_tt_leaderboard *leaderboard);

void rrc_tt_free_leaderboard(struct rrc_tt_leaderboard *leaderboard);

/* Short display names for the character and vehicle IDs stored in ghost files. */
const char *rrc_tt_character_name(int id);
const char *rrc_tt_vehicle_name(int id);

#endif
