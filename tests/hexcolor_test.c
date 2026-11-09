/*
    Unit tests for the hex color helpers.

    Built and executed by "zig build test".
*/
#include "../src/hexcolor.h"

#include <stdio.h>
#include <wchar.h>

static int g_failures = 0;

static void ExpectValid(const wchar_t *text, COLORREF expected)
{
    COLORREF color = 0;

    if (!ParseHexColor(text, &color))
    {
        printf("FAIL: \"%ls\" should be valid\n", text);
        g_failures++;
        return;
    }

    if (color != expected)
    {
        printf("FAIL: \"%ls\" parsed as %06X, expected %06X\n",
               text,
               (unsigned int)color,
               (unsigned int)expected);
        g_failures++;
    }
}

static void ExpectInvalid(const wchar_t *text)
{
    COLORREF color = 0;

    if (ParseHexColor(text, &color))
    {
        printf("FAIL: \"%ls\" should be rejected\n", text);
        g_failures++;
    }
}

int main(void)
{
    /*
        Accepted forms.
    */
    ExpectValid(L"#1a2B3C", RGB(0x1A, 0x2B, 0x3C));
    ExpectValid(L"1a2b3c", RGB(0x1A, 0x2B, 0x3C));
    ExpectValid(L"0xFFB000", RGB(0xFF, 0xB0, 0x00));
    ExpectValid(L"  #FFFFFF  ", RGB(255, 255, 255));
    ExpectValid(L"#000000", RGB(0, 0, 0));
    ExpectValid(L"#abc", RGB(0xAA, 0xBB, 0xCC));

    /*
        Rejected forms.
    */
    ExpectInvalid(L"");
    ExpectInvalid(L"#");
    ExpectInvalid(L"#GGGGGG");
    ExpectInvalid(L"#FFFF");
    ExpectInvalid(L"#FFFFFFF");
    ExpectInvalid(L"hello");
    ExpectInvalid(L"#12 3456");
    ExpectInvalid(L"#FF88  ");

    /*
        Formatting, and a round trip back through the parser.
    */
    wchar_t text[8];

    FormatHexColor(RGB(0x1A, 0x2B, 0x3C), text);

    if (wcscmp(text, L"#1A2B3C") != 0)
    {
        printf("FAIL: formatted as \"%ls\", expected \"#1A2B3C\"\n", text);
        g_failures++;
    }

    COLORREF round_trip = 0;

    if (!ParseHexColor(text, &round_trip) || round_trip != RGB(0x1A, 0x2B, 0x3C))
    {
        printf("FAIL: round trip through FormatHexColor/ParseHexColor broke\n");
        g_failures++;
    }

    if (g_failures != 0)
    {
        printf("%d test(s) failed\n", g_failures);
        return 1;
    }

    printf("all hexcolor tests passed\n");

    return 0;
}
