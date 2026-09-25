/*
    text.h - converting text for the console font

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

#ifndef RRC_TEXT_H
#define RRC_TEXT_H

/*
    Converts UTF-8 text in place to the character set of the console font (code page 437), which
    `rrc_gfx_draw_text' renders. Letters the font has are kept (accented Latin letters, some Greek),
    typographic punctuation becomes its ASCII equivalent and anything else (e.g. emoji) is dropped.
    Tabs become spaces and other control characters except newlines are dropped.

    The result is never longer than the input.
*/
void rrc_text_utf8_to_cp437(char *s);

#endif
