/*
    http.h - downloading small files from the VanzaKart server

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

#ifndef RRC_HTTP_H
#define RRC_HTTP_H

#include <stddef.h>
#include "result.h"

/* Base URL of the VanzaKart web server (see `server_base_url' in Launcher/endpoints.json on the server). */
#define RRC_HTTP_SERVER_URL "https://vanzakart.net:8443"

/* Largest response accepted: these are small JSON documents. */
#define RRC_HTTP_MAX_SIZE (1024 * 1024)

/*
    Downloads `url' (HTTP or HTTPS) into a newly allocated, NUL-terminated buffer, initialising the
    network first if needed. Progress is shown on the console, labelled `label'.

    On success `*data' must be freed by the caller and `*len' is its length without the terminator.

    HTTPS certificates are not verified: the console has no CA certificates (and its clock may be
    off). Only use this for data that is displayed, never for anything that is executed or installed.
*/
struct rrc_result rrc_http_get(const char *url, const char *label, char **data, size_t *len);

#endif
