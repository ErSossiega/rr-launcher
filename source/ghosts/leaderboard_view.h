/*
    leaderboard_view.h - time trial leaderboard screens

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

#ifndef RRC_LEADERBOARD_VIEW_H
#define RRC_LEADERBOARD_VIEW_H

#include "leaderboard.h"

/*
    Shows `tracks' as a list in the banner box. A opens the 150cc leaderboard of the selected track
    (downloaded on demand), B or HOME go back. Returns when the user leaves the track list.
*/
void rrc_leaderboard_view_display(void *xfb, const struct rrc_tt_tracks *tracks);

#endif
