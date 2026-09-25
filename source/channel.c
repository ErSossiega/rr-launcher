/*
    channel.c - installing and removing the launcher channel in the Wii Menu

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

#include <gccore.h>
#include <ogc/es.h>

#include "channel.h"

bool rrc_channel_is_installed()
{
    // A title is installed if ES has a TMD stored for it.
    u32 tmd_size;
    return ES_GetStoredTMDSize(RRC_CHANNEL_TITLE_ID, &tmd_size) >= 0 && tmd_size > 0;
}

struct rrc_result rrc_channel_install(void *xfb)
{
    /*
        TODO: implement. Install the channel WAD (e.g. shipped on the SD card next to the
        runtime-ext files) with ES_AddTitleStart/ES_AddTicket/ES_AddContentStart/... ES_AddTitleFinish.
    */
    return rrc_result_create_error_channel("Installing the channel is not available yet.");
}

struct rrc_result rrc_channel_remove(void *xfb)
{
    /*
        TODO: implement. Remove the title with ES_DeleteTitleContent, ES_DeleteTitle and
        ES_DeleteTicket for RRC_CHANNEL_TITLE_ID.
    */
    return rrc_result_create_error_channel("Removing the channel is not available yet.");
}
