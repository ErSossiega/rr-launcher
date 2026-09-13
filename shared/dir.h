/*
    dir.h - File base directory handling based on macros

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

#ifndef RRC_DIR_H
#define RRC_DIR_H

#if defined(RRC_BETA) && RRC_BETA >= 1
#   define RRC_RETRO_REWIND_BASE_DIR "VKBeta"
#   define RRC_RETRO_REWIND_CHANNEL_DIR "VanzaKartChannelBeta"
#else
#   define RRC_RETRO_REWIND_BASE_DIR "VanzaKart"
#   define RRC_RETRO_REWIND_CHANNEL_DIR "VanzaKartChannel"
#endif

#endif