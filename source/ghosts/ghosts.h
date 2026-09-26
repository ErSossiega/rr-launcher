/*
    ghosts.h - downloading time trial ghosts to the SD card

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

#ifndef RRC_GHOSTS_H
#define RRC_GHOSTS_H

#include <stdbool.h>
#include <dir.h>
#include "../result.h"

/*
    Where the game (Pulsar) keeps time trial ghosts: one folder per track, named after the CRC32 of the
    track file in lowercase hex, with a subfolder per mode, e.g. /VanzaKart/Ghosts/c7e18faa/150/1m57s383.rkg.
    The game lists every valid ghost file in the folder of the track being played.
*/
#define RRC_GHOSTS_DIR "/" RRC_RETRO_REWIND_BASE_DIR "/Ghosts"

/* Written by the Pulsar pack creator: one "<track name> = <CRC32>" line per track. */
#define RRC_GHOSTS_FOLDER_LIST RRC_GHOSTS_DIR "/FolderToTrackName.txt"

/* Subfolder of the 150cc ghosts (the game also has 200, 150F and 200F). */
#define RRC_GHOSTS_MODE "150"

/* The game reads at most this many files per folder (maxFileCount in Pulsar's IO.hpp). */
#define RRC_GHOSTS_MAX_PER_FOLDER 100

struct rrc_ghost_folder
{
    /* Track name, normalised for matching (see ghosts.c). */
    char *name;
    /* Folder name: the CRC32 in lowercase hex. */
    char crc32[9];
};

/* The track folders listed in RRC_GHOSTS_FOLDER_LIST. */
struct rrc_ghost_folders
{
    struct rrc_ghost_folder *folders;
    int count;
};

/*
    Reads RRC_GHOSTS_FOLDER_LIST. On success `folders' must be freed with `rrc_ghost_folders_free'.
    Fails if the file is missing, which happens with packs built before it was added.
*/
struct rrc_result rrc_ghost_folders_load(struct rrc_ghost_folders *folders);

void rrc_ghost_folders_free(struct rrc_ghost_folders *folders);

/*
    Returns the folder of the track named `track_name' (as the server names it), or NULL if the pack has
    none, e.g. for track variants, which can't be raced in time trials. Case, punctuation and the game's
    colour codes are ignored when comparing names.
*/
const char *rrc_ghost_folders_find(const struct rrc_ghost_folders *folders, const char *track_name);

/* Whether a ghost with finish time `time' ("m:ss.mmm") is already in the track folder `folder'. */
bool rrc_ghosts_is_downloaded(const char *folder, const char *time);

/*
    Downloads ghost submission `id' from the server into the track folder `folder', named after its finish time
    `time' like the game names its own ghosts, showing progress on the console. The file is checked to be
    a valid ghost before it is saved. Downloading a ghost that is already there succeeds without a copy.
*/
struct rrc_result rrc_ghosts_download(int id, const char *folder, const char *time);

#endif
