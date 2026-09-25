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

#include "ghosts.h"
#include "../console.h"

struct rrc_result rrc_ghosts_download(void *xfb, int *count)
{
    *count = 0;

    rrc_con_clear(true);
    rrc_con_update("Ghosts: Prepare Network", 0);

    /*
        TODO: implement. The intended flow mirrors `rrc_update_do_updates' (see update/update.c):

        1. Initialise the network with wiisocket_init().
        2. Download RRC_GHOSTS_INDEX_URL, listing the available ghosts (e.g. track, file name, hash).
        3. Skip the ghosts already present in RRC_GHOSTS_DIR.
        4. Download the remaining ones into RRC_GHOSTS_DIR, calling rrc_con_update() for progress,
           and increment `*count' for each one.

        Errors should be returned as rrc_result_create_error_ghosts() (or the curl/errno variants),
        which the menu shows to the user.
    */

    return rrc_result_create_error_ghosts("Ghost downloads are not available yet.");
}
