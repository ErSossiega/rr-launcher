/*
    ephfile.h - Routines for handling of ephemeral files

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

#ifndef RRC_EPHFILE_H
#define RRC_EPHFILE_H

// Do not use the channel base dir constant here, as the game is unaware of this.
// It will always write into the "main" folder. Since it's an ephemeral file, it doesn't really matter.
#define RRC_LAUNCHED_FROM_RR_FILE_PATH "/VanzaKartChannel/.lfrr"

bool rrc_launched_from_rr();

#endif