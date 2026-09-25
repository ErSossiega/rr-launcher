/*
    text.c - converting text for the console font

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

#include <string.h>
#include <gctypes.h>

#include "text.h"

/*
    Code page 437 equivalents of U+00A0 to U+00FF (generated with Python's cp437 codec).
    Letters the font doesn't have fall back to their unaccented ASCII letter; 0 means dropped.
*/
static const u8 latin1_to_cp437[96] = {
    0x20, 0xAD, 0x9B, 0x9C, 0x00, 0x9D, 0x00, 0x00, /* U+00A0 */
    0x00, 0x00, 0xA6, 0xAE, 0xAA, 0x00, 0x00, 0x00, /* U+00A8 */
    0xF8, 0xF1, 0xFD, 0x00, 0x00, 0xE6, 0x00, 0xFA, /* U+00B0 */
    0x00, 0x00, 0xA7, 0xAF, 0xAC, 0xAB, 0x00, 0xA8, /* U+00B8 */
    0x41, 0x41, 0x41, 0x41, 0x8E, 0x8F, 0x92, 0x80, /* U+00C0 */
    0x45, 0x90, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49, /* U+00C8 */
    0x00, 0xA5, 0x4F, 0x4F, 0x4F, 0x4F, 0x99, 0x00, /* U+00D0 */
    0x00, 0x55, 0x55, 0x55, 0x9A, 0x59, 0x00, 0xE1, /* U+00D8 */
    0x85, 0xA0, 0x83, 0x61, 0x84, 0x86, 0x91, 0x87, /* U+00E0 */
    0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B, /* U+00E8 */
    0x00, 0xA4, 0x95, 0xA2, 0x93, 0x6F, 0x94, 0xF6, /* U+00F0 */
    0x00, 0x97, 0xA3, 0x96, 0x81, 0x79, 0x00, 0x98, /* U+00F8 */
};

/*
    Appends the code page 437 form of `cp' to `*out'. `encoded_len' is how many bytes `cp' took in
    the input; at most that many are written so the conversion can happen in place.
*/
static void append_codepoint(char **out, u32 cp, int encoded_len)
{
    if (cp == '\n')
        *(*out)++ = '\n';
    else if (cp == '\t')
        *(*out)++ = ' ';
    else if (cp < 0x20)
        return;
    else if (cp < 0x80)
        *(*out)++ = cp;
    else if (cp >= 0xA0 && cp <= 0xFF)
    {
        u8 c = latin1_to_cp437[cp - 0xA0];
        if (c != 0)
            *(*out)++ = c;
    }
    else
    {
        u8 c = 0;
        switch (cp)
        {
        case 0x2018: // curly single quotes
        case 0x2019:
            c = '\'';
            break;
        case 0x201C: // curly double quotes
        case 0x201D:
            c = '"';
            break;
        case 0x2013: // en and em dash
        case 0x2014:
            c = '-';
            break;
        case 0x2022: // bullet
            c = 0x07;
            break;
        case 0x2026: // ellipsis, 3 bytes in UTF-8
            if (encoded_len >= 3)
            {
                memcpy(*out, "...", 3);
                *out += 3;
            }
            return;
        // Greek letters the font has
        case 0x03B1: c = 0xE0; break; // alpha
        case 0x0393: c = 0xE2; break; // Gamma
        case 0x03C0: c = 0xE3; break; // pi
        case 0x03A3: c = 0xE4; break; // Sigma
        case 0x03C3: c = 0xE5; break; // sigma
        case 0x03BC: c = 0xE6; break; // mu
        case 0x03C4: c = 0xE7; break; // tau
        case 0x03A6: c = 0xE8; break; // Phi
        case 0x0398: c = 0xE9; break; // Theta
        case 0x03A9: c = 0xEA; break; // Omega
        case 0x03B4: c = 0xEB; break; // delta
        case 0x03C6: c = 0xED; break; // phi
        case 0x03B5: c = 0xEE; break; // epsilon
        case 0x221E: c = 0xEC; break; // infinity
        }

        if (c != 0)
            *(*out)++ = c;
    }
}

/* Decodes one UTF-8 sequence at `*p'. Invalid input yields U+FFFD, which is dropped. */
static u32 decode_utf8(const char **p)
{
    unsigned char c = *(*p)++;
    if (c < 0x80)
        return c;

    int extra = c >= 0xF0 ? 3 : (c >= 0xE0 ? 2 : (c >= 0xC0 ? 1 : -1));
    if (extra < 0)
        return 0xFFFD;

    u32 cp = c & (0x3F >> extra);
    for (int i = 0; i < extra; i++)
    {
        // stops at the terminator too, as it is not a continuation byte
        if ((**p & 0xC0) != 0x80)
            return 0xFFFD;
        cp = (cp << 6) | (*(*p)++ & 0x3F);
    }

    return cp;
}

void rrc_text_utf8_to_cp437(char *s)
{
    const char *in = s;
    char *out = s;

    while (*in != '\0')
    {
        const char *start = in;
        u32 cp = decode_utf8(&in);
        append_codepoint(&out, cp, in - start);
    }

    *out = '\0';
}
