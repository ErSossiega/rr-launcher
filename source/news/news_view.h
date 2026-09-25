/*
    news_view.h - news reader screen

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

#ifndef RRC_NEWS_VIEW_H
#define RRC_NEWS_VIEW_H

#include "news.h"

/*
    Shows the articles of `news' in the banner box, one at a time: Left/Right switch articles,
    Up/Down scroll, B or HOME go back. Returns when the user goes back.
*/
void rrc_news_view_display(void *xfb, const struct rrc_news *news);

#endif
