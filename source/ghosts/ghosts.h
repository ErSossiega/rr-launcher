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

#include <dir.h>
#include "../result.h"

/*
    Where downloaded ghosts are stored on the SD card.

    TODO: confirm the folder (and per-track layout) the game reads ghosts from.
*/
#define RRC_GHOSTS_DIR "/" RRC_RETRO_REWIND_BASE_DIR "/Ghosts"

/*
    List of ghosts available on the server.

    TODO: placeholder, nothing is served here yet. Define the format together with the server side.
*/
#define RRC_GHOSTS_INDEX_URL "http://vanzakart.net:8000/VanzaKart/Ghosts/index.txt"

/*
    Downloads the ghosts listed on the server that are not on the SD card yet into RRC_GHOSTS_DIR,
    showing progress on the console like `rrc_update_do_updates'.

    `count' receives the number of ghosts downloaded.

    NOT IMPLEMENTED YET: this currently always returns an error, so the menu path can already be
    exercised end to end.
*/
struct rrc_result rrc_ghosts_download(void *xfb, int *count);

#endif
