/*
    http.c - downloading small files from the VanzaKart server

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

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <wiisocket.h>

#include "http.h"
#include "console.h"

struct download
{
    char *data;
    size_t len;
};

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct download *d = userdata;
    size_t n = size * nmemb;

    // returning less than `n' aborts the transfer
    if (d->len + n > RRC_HTTP_MAX_SIZE)
        return 0;

    char *grown = realloc(d->data, d->len + n + 1);
    if (grown == NULL)
        return 0;

    d->data = grown;
    memcpy(d->data + d->len, ptr, n);
    d->len += n;
    d->data[d->len] = '\0';
    return n;
}

struct progress
{
    char *label;
    int last;
};

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    struct progress *p = clientp;
    if (dltotal > 0)
    {
        int percent = (int)(dlnow * 100 / dltotal);
        if (percent != p->last)
        {
            p->last = percent;
            rrc_con_update(p->label, percent);
        }
    }

    return 0;
}

struct rrc_result rrc_http_get(const char *url, const char *label, char **data, size_t *len)
{
    *data = NULL;
    *len = 0;

    rrc_con_clear(true);
    rrc_con_update("Prepare Network", 0);
    if (wiisocket_init() < 0)
    {
        return rrc_result_create_error_curl(CURLE_COULDNT_CONNECT, "Could not connect to the internet.");
    }

    CURL *curl = curl_easy_init();
    if (curl == NULL)
    {
        return rrc_result_create_error_curl(CURLE_FAILED_INIT, "Failed to start the download.");
    }

    struct download d = {NULL, 0};
    struct progress progress = {(char *)label, -1};
    rrc_con_update(progress.label, 0);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &d);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    // See the note in http.h: no CA certificates on the console, so HTTPS is encrypted but unverified.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        free(d.data);
        return rrc_result_create_error_curl(res, "Download failed.");
    }

    if (d.data == NULL)
    {
        // empty response
        d.data = calloc(1, 1);
        if (d.data == NULL)
            return rrc_result_create_error_errno(ENOMEM, "Out of memory.");
    }

    rrc_con_update(progress.label, 100);
    *data = d.data;
    *len = d.len;
    return rrc_result_success;
}
