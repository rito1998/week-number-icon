#ifndef HEXCOLOR_H
#define HEXCOLOR_H

#include <windows.h>

/*
    Parse a hexadecimal color.

    Accepted forms (case-insensitive, surrounding blanks allowed):

        #RRGGBB
        RRGGBB
        0xRRGGBB
        #RGB            three digit shorthand, expanded to #RRGGBB

    The result is a GDI COLORREF (0x00BBGGRR).

    Returns FALSE when the input is not a valid color.
*/
BOOL ParseHexColor(const wchar_t *text, COLORREF *color);

/*
    Format a COLORREF as "#RRGGBB" (upper case).

    "out" must be able to hold at least 8 wchar_t values.
*/
void FormatHexColor(COLORREF color, wchar_t *out);

#endif /* HEXCOLOR_H */
