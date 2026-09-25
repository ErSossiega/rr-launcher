/*
    menu.h - graphical main menu

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

#ifndef RRC_MENU_H
#define RRC_MENU_H

#include <stdbool.h>

#include "settingsfile.h"

enum rrc_menu_result
{
    RRC_MENU_LAUNCH = 0,
    RRC_MENU_EXIT = 1
};

/*
    Displays the tile-based main menu inside the banner and returns once the user chose to
    launch the game or exit the channel.

    If `allow_autolaunch' is set, the game is launched automatically after a few seconds
    unless a button is pressed.

    Actions that print progress (updates) or open the text settings menu use the console as before;
    the menu redraws itself when they return. The menu area is left cleared on return, ready for
    console output.
*/
enum rrc_menu_result rrc_menu_display(void *xfb, struct rrc_settingsfile *stored_settings, bool allow_autolaunch);

#endif
