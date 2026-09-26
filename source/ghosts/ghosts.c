/*
    ghosts.c - downloading time trial ghosts to the SD card

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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#include "ghosts.h"
#include "leaderboard.h"
#include "../http.h"
#include "../sd.h"
#include "../text.h"

/* A ghost file is at least its header plus the CRC32 at the end, and at most the game's RKG size. */
#define RKG_HEADER_SIZE 0x88
#define RKG_MAX_SIZE 0x2800

/*
    Keeps only letters and digits (lowercased) and bytes above 0x7F (accented letters), and drops the
    game's \c{...} colour codes, so "\c{yor7}3DS \c{off}Wuhu Loop" and "3DS Wuhu Loop" compare equal,
    as do "Bowser Jr's Fort" and "Bowser Jr’s Fort". `s' must already be in the console font's
    character set (see `rrc_text_utf8_to_cp437').
*/
static void normalise_name(char *s)
{
    char *out = s;
    for (char *p = s; *p != '\0';)
    {
        if (p[0] == '\\' && p[1] == 'c' && p[2] == '{')
        {
            char *end = strchr(p, '}');
            if (end != NULL)
            {
                p = end + 1;
                continue;
            }
        }

        unsigned char c = *p++;
        if (isalnum(c))
            *out++ = tolower(c);
        else if (c >= 0x80)
            *out++ = c;
    }
    *out = '\0';
}

static bool is_crc32(const char *s)
{
    if (strlen(s) != 8)
        return false;

    for (int i = 0; i < 8; i++)
    {
        if (!isxdigit((unsigned char)s[i]))
            return false;
    }

    return true;
}

struct rrc_result rrc_ghost_folders_load(struct rrc_ghost_folders *folders)
{
    folders->folders = NULL;
    folders->count = 0;

    FILE *f = fopen(RRC_GHOSTS_FOLDER_LIST, "r");
    if (f == NULL)
    {
        return rrc_result_create_error_ghosts("The pack has no ghost folder list.\nUpdate the pack to download ghosts.");
    }

    int capacity = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) != NULL)
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' '))
            line[--len] = '\0';

        // "<name> = <crc32>"; the name itself may contain " = ", so split at the last one
        char *sep = NULL;
        for (char *p = strstr(line, " = "); p != NULL; p = strstr(p + 1, " = "))
            sep = p;
        if (sep == NULL)
            continue;

        *sep = '\0';
        const char *crc = sep + 3;
        if (!is_crc32(crc))
            continue;

        if (folders->count == capacity)
        {
            int new_capacity = capacity == 0 ? 64 : capacity * 2;
            struct rrc_ghost_folder *grown = realloc(folders->folders, new_capacity * sizeof(*grown));
            if (grown == NULL)
                break;

            folders->folders = grown;
            capacity = new_capacity;
        }

        struct rrc_ghost_folder *folder = &folders->folders[folders->count];
        folder->name = strdup(line);
        if (folder->name == NULL)
            break;

        rrc_text_utf8_to_cp437(folder->name);
        normalise_name(folder->name);
        for (int i = 0; i < 8; i++)
            folder->crc32[i] = tolower((unsigned char)crc[i]);
        folder->crc32[8] = '\0';
        folders->count++;
    }

    fclose(f);
    return rrc_result_success;
}

void rrc_ghost_folders_free(struct rrc_ghost_folders *folders)
{
    for (int i = 0; i < folders->count; i++)
        free(folders->folders[i].name);

    free(folders->folders);
    folders->folders = NULL;
    folders->count = 0;
}

const char *rrc_ghost_folders_find(const struct rrc_ghost_folders *folders, const char *track_name)
{
    char *name = strdup(track_name);
    if (name == NULL)
        return NULL;
    normalise_name(name);

    const char *folder = NULL;
    for (int i = 0; i < folders->count && folder == NULL; i++)
    {
        if (name[0] != '\0' && strcmp(folders->folders[i].name, name) == 0)
            folder = folders->folders[i].crc32;
    }

    free(name);
    return folder;
}

/*
    Path of the ghost with finish time `time' in the track folder `folder', named like the game names its own:
    "1m57s383.rkg", or with `letter' set, "1m57s38a.rkg" for further ghosts with the same time.
*/
static bool ghost_path(char *path, size_t size, const char *folder, const char *time, char letter)
{
    int minutes, seconds, milliseconds;
    if (sscanf(time, "%d:%d.%d", &minutes, &seconds, &milliseconds) != 3)
        return false;

    if (letter == 0)
        snprintf(path, size, RRC_GHOSTS_DIR "/%s/" RRC_GHOSTS_MODE "/%01dm%02ds%03d.rkg", folder, minutes, seconds, milliseconds);
    else
        snprintf(path, size, RRC_GHOSTS_DIR "/%s/" RRC_GHOSTS_MODE "/%01dm%02ds%02d%c.rkg", folder, minutes, seconds, milliseconds / 10, letter);

    return true;
}

bool rrc_ghosts_is_downloaded(const char *folder, const char *time)
{
    char path[160];
    return ghost_path(path, sizeof(path), folder, time, 0) && rrc_sd_file_exists(path);
}

/* Whether the file at `path' holds exactly `data'. */
static bool file_equals(const char *path, const char *data, size_t len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return false;

    bool equal = true;
    char buf[512];
    size_t offset = 0;
    size_t n;
    while (equal && (n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        equal = offset + n <= len && memcmp(buf, data + offset, n) == 0;
        offset += n;
    }

    fclose(f);
    return equal && offset == len;
}

static bool is_valid_ghost(const unsigned char *data, size_t len)
{
    if (len < RKG_HEADER_SIZE + 4 || len > RKG_MAX_SIZE || memcmp(data, "RKGD", 4) != 0)
        return false;

    // big-endian CRC32 of everything before it
    u32 stored = ((u32)data[len - 4] << 24) | ((u32)data[len - 3] << 16) | ((u32)data[len - 2] << 8) | data[len - 1];
    return crc32(0, data, len - 4) == stored;
}

/* Creates `path' if it doesn't exist. */
static bool make_folder(const char *path)
{
    return rrc_sd_folder_exists(path) || mkdir(path, 0777) == 0 || errno == EEXIST;
}

struct rrc_result rrc_ghosts_download(int id, const char *folder, const char *time)
{
    char track_folder[96], mode_folder[112];
    snprintf(track_folder, sizeof(track_folder), RRC_GHOSTS_DIR "/%s", folder);
    snprintf(mode_folder, sizeof(mode_folder), "%s/" RRC_GHOSTS_MODE, track_folder);

    // The pack creator makes the track folder and the game the mode folder, but either may be missing.
    if (!make_folder(RRC_GHOSTS_DIR) || !make_folder(track_folder) || !make_folder(mode_folder))
    {
        return rrc_result_create_error_errno(errno, "Failed to create the ghost folder on the SD card.");
    }

    struct rrc_result count_res = rrc_result_success;
    int count = rrc_sd_get_folder_file_count(mode_folder, &count_res);
    if (rrc_result_is_error(count_res))
        return count_res;
    if (count >= RRC_GHOSTS_MAX_PER_FOLDER)
    {
        return rrc_result_create_error_ghosts("This track already has 100 ghosts on the SD card.\nThe game would not show any more.");
    }

    char url[128];
    snprintf(url, sizeof(url), RRC_TT_API_URL "/ghost/%d/download", id);

    char *data;
    size_t len;
    struct rrc_result res = rrc_http_get(url, "Downloading Ghost", &data, &len);
    if (rrc_result_is_error(res))
        return res;

    if (!is_valid_ghost((const unsigned char *)data, len))
    {
        free(data);
        return rrc_result_create_error_ghosts("The server sent an invalid ghost file.");
    }

    // Use the game's name for the time; if another ghost has it, try the lettered names it uses too.
    char path[160];
    bool found_name = false;
    for (char letter = 0; letter <= 'z'; letter = letter == 0 ? 'a' : letter + 1)
    {
        if (!ghost_path(path, sizeof(path), folder, time, letter))
            break;

        if (!rrc_sd_file_exists(path))
        {
            found_name = true;
            break;
        }

        if (file_equals(path, data, len))
        {
            // already downloaded
            free(data);
            return rrc_result_success;
        }
    }

    if (!found_name)
    {
        free(data);
        return rrc_result_create_error_ghosts("Could not name the ghost file.");
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        free(data);
        return rrc_result_create_error_errno(errno, "Failed to save the ghost. The SD card may be write locked.");
    }

    size_t written = fwrite(data, 1, len, f);
    int close_res = fclose(f);
    free(data);

    if (written != len || close_res != 0)
    {
        remove(path);
        return rrc_result_create_error_errno(errno, "Failed to save the ghost to the SD card.");
    }

    return rrc_result_success;
}
