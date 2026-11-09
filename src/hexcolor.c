#include "hexcolor.h"

/*
    Number of hexadecimal digits collected before giving up.

    More than 6 digits is always invalid, so the loop stops there.
*/
#define MAX_DIGITS 8

/*
    Return the value of a single hexadecimal digit,
    or -1 when the character is not a hex digit.
*/
static int HexDigitValue(wchar_t character)
{
    if (character >= L'0' && character <= L'9')
        return (int)(character - L'0');

    if (character >= L'a' && character <= L'f')
        return (int)(character - L'a') + 10;

    if (character >= L'A' && character <= L'F')
        return (int)(character - L'A') + 10;

    return -1;
}

/*
    Inverse of HexDigitValue.
*/
static wchar_t HexDigitCharacter(int value)
{
    if (value < 10)
        return (wchar_t)(L'0' + value);

    return (wchar_t)(L'A' + (value - 10));
}

static BOOL IsBlank(wchar_t character)
{
    return character == L' ' ||
           character == L'\t' ||
           character == L'\r' ||
           character == L'\n';
}

BOOL ParseHexColor(const wchar_t *text, COLORREF *color)
{
    if (!text || !color)
        return FALSE;

    const wchar_t *cursor = text;

    /*
        Ignore surrounding blanks and accept the optional
        "#" or "0x" prefix.
    */
    while (IsBlank(*cursor))
        cursor++;

    if (*cursor == L'#')
    {
        cursor++;
    }
    else if (cursor[0] == L'0' &&
             (cursor[1] == L'x' || cursor[1] == L'X'))
    {
        cursor += 2;
    }

    int digits[MAX_DIGITS];
    int count = 0;

    while (*cursor != L'\0' && count < MAX_DIGITS)
    {
        int value = HexDigitValue(*cursor);

        if (value < 0)
            break;

        digits[count++] = value;
        cursor++;
    }

    /*
        Anything but blanks may follow the digits.
    */
    while (IsBlank(*cursor))
        cursor++;

    if (*cursor != L'\0')
        return FALSE;

    /*
        Expand the three digit shorthand:
        "#abc" becomes "aabbcc".
    */
    if (count == 3)
    {
        int short_digits[3] = {digits[0], digits[1], digits[2]};

        for (int i = 0; i < 3; i++)
        {
            digits[i * 2]     = short_digits[i];
            digits[i * 2 + 1] = short_digits[i];
        }

        count = 6;
    }

    if (count != 6)
        return FALSE;

    int red   = digits[0] * 16 + digits[1];
    int green = digits[2] * 16 + digits[3];
    int blue  = digits[4] * 16 + digits[5];

    *color = RGB(red, green, blue);

    return TRUE;
}

void FormatHexColor(COLORREF color, wchar_t *out)
{
    if (!out)
        return;

    const int components[3] = {
        GetRValue(color),
        GetGValue(color),
        GetBValue(color)};

    out[0] = L'#';

    for (int i = 0; i < 3; i++)
    {
        out[1 + i * 2]     = HexDigitCharacter((components[i] >> 4) & 0x0F);
        out[2 + i * 2]     = HexDigitCharacter(components[i] & 0x0F);
    }

    out[7] = L'\0';
}
