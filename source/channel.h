/*
    channel.h - installing and removing the launcher channel in the Wii Menu

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

#ifndef RRC_CHANNEL_H
#define RRC_CHANNEL_H

#include <stdbool.h>
#include "result.h"

/*
    Title ID of the launcher channel (WAD): 00010001 (downloaded channel) followed by the 4 character ID.

    TODO: placeholder ("VKLC"), there is no WAD yet. It must match the title ID the WAD is built with,
    otherwise the channel is never detected as installed.
*/
#define RRC_CHANNEL_TITLE_ID 0x00010001564B4C43ULL

/*
    Returns true if the launcher channel is installed on the console (or Dolphin's emulated NAND).
*/
bool rrc_channel_is_installed();

/*
    Installs the launcher channel to the Wii Menu.

    NOT IMPLEMENTED YET: always returns an error.
*/
struct rrc_result rrc_channel_install(void *xfb);

/*
    Removes the launcher channel from the Wii Menu. Files on the SD card are not touched.

    NOT IMPLEMENTED YET: always returns an error.
*/
struct rrc_result rrc_channel_remove(void *xfb);

#endif
