#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <stdio.h>

#include "hexcolor.h"

#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY 1
#define ID_TIMER 1
#define ID_EXIT 100
#define ID_CUSTOM 101

/*
    Preset menu items use ID_PRESET + index into g_presets.
*/
#define ID_PRESET 200

/*
    Control ids of the custom color dialog.
*/
#define IDC_TEXT_EDIT 1001
#define IDC_BACK_EDIT 1002
#define IDC_TEXT_PICK 1003
#define IDC_BACK_PICK 1004
#define IDC_SWATCH_TEXT 1005
#define IDC_SWATCH_BACK 1006
#define IDC_TRANSPARENT 1007
#define IDC_HINT 1008

/*
    Marker stored when the colors do not come from a preset.
*/
#define PRESET_CUSTOM (-1)

#define ICON_SIZE 28

/*
    Byte width of one scan line of the icon's monochrome mask.
*/
#define ICON_MASK_STRIDE (((ICON_SIZE + 31) / 32) * 4)

/*
    Where the chosen colors are persisted.
*/
#define SETTINGS_KEY L"Software\\WeekNumberIcon"

static HICON g_trayIcon = NULL;

/*
    Hidden message window owning the tray icon.
*/
static HWND g_mainWnd = NULL;

/*
    Colors used to paint the tray icon.

    Loaded from the registry at startup and written back
    whenever the user picks something else.

    "preset" is an index into g_presets, or PRESET_CUSTOM
    when the colors came from the custom dialog.
*/
typedef struct
{
    COLORREF textColor;
    COLORREF backColor;
    BOOL backTransparent;
    int preset;
} IconStyle;

/*
    Default look: white number on a transparent background,
    i.e. whatever the taskbar shows behind the tray icon.
*/
static IconStyle g_style = {
    RGB(255, 255, 255),
    RGB(0, 0, 0),
    TRUE,
    0};

/*
    A named color pair selectable straight from the tray menu.
*/
typedef struct
{
    const wchar_t *name;
    const wchar_t *textHex;
    const wchar_t *backHex;
    BOOL backTransparent;
} ColorPreset;

static const ColorPreset g_presets[] = {
    {L"Default (white, transparent)", L"#FFFFFF", L"#000000", TRUE},
    {L"White on black", L"#FFFFFF", L"#000000", FALSE},
    {L"Black on white", L"#000000", L"#FFFFFF", FALSE},
    {L"Amber on black", L"#FFB000", L"#000000", FALSE},
    {L"Green on black", L"#00E676", L"#000000", FALSE},
    {L"Blue on white", L"#1565C0", L"#FFFFFF", FALSE},
};

#define PRESET_COUNT ((int)(sizeof(g_presets) / sizeof(g_presets[0])))

/*
    Working copy edited by the custom color dialog.
*/
static IconStyle g_dialogStyle;

/*
    The 16 custom slots remembered by the Windows color picker.
*/
static COLORREF g_pickerCustomColors[16] = {
    RGB(255, 255, 255), RGB(0, 0, 0), RGB(192, 192, 192), RGB(128, 128, 128),
    RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 0),
    RGB(0, 255, 255), RGB(255, 0, 255), RGB(128, 0, 0), RGB(0, 128, 0),
    RGB(0, 0, 128), RGB(128, 128, 0), RGB(192, 192, 0), RGB(128, 0, 128)};

static void UpdateTrayIcon(HWND hwnd);

/*
    Perceived brightness of a color: 0 (black) to 255 (white).
*/
static int ColorLuma(COLORREF color)
{
    return (GetRValue(color) * 299 +
            GetGValue(color) * 587 +
            GetBValue(color) * 114) /
           1000;
}

/*
    True when text and background are so close in brightness
    that the number would become unreadable.
*/
static BOOL ColorsTooSimilar(COLORREF first, COLORREF second)
{
    int firstLuma = ColorLuma(first);
    int secondLuma = ColorLuma(second);

    int delta = firstLuma > secondLuma ?
                    firstLuma - secondLuma :
                    secondLuma - firstLuma;

    return delta < 60;
}

/*
    Read the colors previously saved by SaveSettings().

    Missing or broken values simply leave the defaults in place.
*/
static void LoadSettings(void)
{
    HKEY key = NULL;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY,
                      0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        return;
    }

    DWORD value = 0;
    DWORD type = 0;
    DWORD size = sizeof(value);

    if (RegQueryValueExW(key, L"TextColor", NULL, &type,
                         (BYTE *)&value, &size) == ERROR_SUCCESS &&
        type == REG_DWORD &&
        size == sizeof(value))
    {
        g_style.textColor = value;
    }

    value = 0;
    type = 0;
    size = sizeof(value);

    if (RegQueryValueExW(key, L"BackColor", NULL, &type,
                         (BYTE *)&value, &size) == ERROR_SUCCESS &&
        type == REG_DWORD &&
        size == sizeof(value))
    {
        g_style.backColor = value;
    }

    value = 0;
    type = 0;
    size = sizeof(value);

    if (RegQueryValueExW(key, L"BackTransparent", NULL, &type,
                         (BYTE *)&value, &size) == ERROR_SUCCESS &&
        type == REG_DWORD &&
        size == sizeof(value))
    {
        g_style.backTransparent = value ? TRUE : FALSE;
    }

    value = 0;
    type = 0;
    size = sizeof(value);

    if (RegQueryValueExW(key, L"Preset", NULL, &type,
                         (BYTE *)&value, &size) == ERROR_SUCCESS &&
        type == REG_DWORD &&
        size == sizeof(value) &&
        ((int)value == PRESET_CUSTOM ||
         ((int)value >= 0 && (int)value < PRESET_COUNT)))
    {
        g_style.preset = (int)value;
    }

    RegCloseKey(key);
}

/*
    Store the current colors in HKCU\Software\WeekNumberIcon.

    Colors are kept as plain COLORREF values (0x00BBGGRR).
*/
static void SaveSettings(void)
{
    HKEY key = NULL;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, NULL, 0,
                        KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
    {
        return;
    }

    DWORD textColor = g_style.textColor;
    DWORD backColor = g_style.backColor;
    DWORD backTransparent = g_style.backTransparent ? 1 : 0;
    DWORD preset = (DWORD)g_style.preset;

    RegSetValueExW(key, L"TextColor", 0, REG_DWORD,
                   (const BYTE *)&textColor, sizeof(textColor));

    RegSetValueExW(key, L"BackColor", 0, REG_DWORD,
                   (const BYTE *)&backColor, sizeof(backColor));

    RegSetValueExW(key, L"BackTransparent", 0, REG_DWORD,
                   (const BYTE *)&backTransparent, sizeof(backTransparent));

    RegSetValueExW(key, L"Preset", 0, REG_DWORD,
                   (const BYTE *)&preset, sizeof(preset));

    RegCloseKey(key);
}

/*
    Make a style current, remember it and repaint the icon.
*/
static void ApplyStyle(const IconStyle *style)
{
    g_style = *style;

    SaveSettings();

    if (g_mainWnd)
    {
        UpdateTrayIcon(g_mainWnd);
    }
}

/*
    Select one of the menu presets.
*/
static void ApplyPreset(int index)
{
    if (index < 0 || index >= PRESET_COUNT)
        return;

    IconStyle style = g_style;

    style.preset = index;
    style.backTransparent = g_presets[index].backTransparent;

    if (!ParseHexColor(g_presets[index].textHex, &style.textColor))
        return;

    if (!ParseHexColor(g_presets[index].backHex, &style.backColor))
        return;

    ApplyStyle(&style);
}

/*
    Return the ISO-8601 week number for a Gregorian date.

    ISO rules:
      - Monday is the first day of the week.
      - Week 1 is the week containing January 4.
*/
static int GetISOWeek(int year, int month, int day)
{
    SYSTEMTIME date = {0};
    date.wYear = (WORD)year;
    date.wMonth = (WORD)month;
    date.wDay = (WORD)day;

    SYSTEMTIME jan4 = {0};
    jan4.wYear = (WORD)year;
    jan4.wMonth = 1;
    jan4.wDay = 4;

    FILETIME ftDate;
    FILETIME ftJan4;

    if (!SystemTimeToFileTime(&date, &ftDate))
        return 0;

    if (!SystemTimeToFileTime(&jan4, &ftJan4))
        return 0;

    ULARGE_INTEGER dateValue;
    ULARGE_INTEGER jan4Value;

    dateValue.LowPart = ftDate.dwLowDateTime;
    dateValue.HighPart = ftDate.dwHighDateTime;

    jan4Value.LowPart = ftJan4.dwLowDateTime;
    jan4Value.HighPart = ftJan4.dwHighDateTime;

    /*
        Windows:
            Sunday    = 0
            Monday    = 1
            ...
            Saturday  = 6

        Convert to:
            Monday    = 0
            ...
            Sunday    = 6
    */
    int jan4Weekday;

    {
        SYSTEMTIME temp;

        FileTimeToSystemTime(&ftJan4, &temp);

        jan4Weekday =
            ((int)temp.wDayOfWeek + 6) % 7;
    }

    int dateWeekday;

    {
        SYSTEMTIME temp;

        FileTimeToSystemTime(&ftDate, &temp);

        dateWeekday =
            ((int)temp.wDayOfWeek + 6) % 7;
    }

    const ULONGLONG TICKS_PER_DAY =
        24ULL * 60ULL * 60ULL * 10000000ULL;

    /*
        Find Monday of ISO week 1.
    */
    ULONGLONG mondayWeek1 =
        jan4Value.QuadPart -
        (ULONGLONG)jan4Weekday * TICKS_PER_DAY;

    /*
        Find Monday of the current week.
    */
    ULONGLONG mondayCurrent =
        dateValue.QuadPart -
        (ULONGLONG)dateWeekday * TICKS_PER_DAY;

    /*
        Dates before ISO week 1 belong to the
        last ISO week of the previous year.
    */
    if (mondayCurrent < mondayWeek1)
    {
        return GetISOWeek(year - 1, 12, 31);
    }

    int week =
        (int)((mondayCurrent - mondayWeek1) /
              (7ULL * TICKS_PER_DAY)) +
        1;

    /*
        Determine whether this year has ISO week 53.
    */
    if (week == 53)
    {
        SYSTEMTIME nextJan1 = {0};

        nextJan1.wYear = (WORD)(year + 1);
        nextJan1.wMonth = 1;
        nextJan1.wDay = 1;

        FILETIME ftNextJan1;

        if (SystemTimeToFileTime(
                &nextJan1,
                &ftNextJan1))
        {
            SYSTEMTIME temp;

            FileTimeToSystemTime(
                &ftNextJan1,
                &temp);

            /*
                If January 1 of the next year is Monday,
                Tuesday, Wednesday, or Thursday, the current
                year has only 52 ISO weeks.

                More directly, ISO week 53 exists when the
                next year's January 1 is Thursday, or when
                this is a leap-year case.
            */

            int nextJan1Weekday =
                ((int)temp.wDayOfWeek + 6) % 7;

            /*
                If next year's Jan 1 is Friday (4), then
                the current year has week 53.

                If next year's Jan 1 is Monday-Thursday,
                the current year ends at week 52.
            */
            if (nextJan1Weekday < 4)
            {
                return 1;
            }
        }
    }

    return week;
}

/*
    Blend a single channel: coverage 0 keeps "from", 255 returns "to".
*/
static int MixChannel(int from, int to, int coverage)
{
    if (coverage <= 0)
        return from;

    if (coverage >= 255)
        return to;

    return from + (to - from) * coverage / 255;
}

/*
    Draw the number into a grayscale coverage map.

    The glyph is painted white onto a black bitmap, so every byte of
    the result states how much of that pixel the glyph covers: 0
    outside, 255 inside and the antialiased ramp in between.

    The glyph is never drawn into the icon itself: GDI leaves the alpha
    byte of drawn text at zero, which makes the shell ignore the color
    the text was drawn in.
*/
static BOOL RenderNumberCoverage(const wchar_t *text, HFONT font,
                                 BOOL outline, unsigned char *coverage)
{
    BITMAPINFO bmi = {0};

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ICON_SIZE;
    bmi.bmiHeader.biHeight = -ICON_SIZE;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;

    HDC screenDC = GetDC(NULL);

    if (!screenDC)
        return FALSE;

    HBITMAP bitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS,
                                      &bits, NULL, 0);

    ReleaseDC(NULL, screenDC);

    if (!bitmap || !bits)
    {
        if (bitmap)
            DeleteObject(bitmap);

        return FALSE;
    }

    /*
        Black canvas, so the coverage starts at zero everywhere.
    */
    ZeroMemory(bits, (size_t)ICON_SIZE * ICON_SIZE * 4);

    HDC dc = CreateCompatibleDC(NULL);

    if (!dc)
    {
        DeleteObject(bitmap);
        return FALSE;
    }

    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

    HFONT oldFont = font ? (HFONT)SelectObject(dc, font) : NULL;

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));

    RECT rect = {0, 0, ICON_SIZE, ICON_SIZE};

    if (outline)
    {
        /*
            Four shifted copies build a one pixel outline.
        */
        static const int offsets[4][2] = {
            {-1, 0},
            {1, 0},
            {0, -1},
            {0, 1}};

        for (int i = 0; i < 4; i++)
        {
            RECT shifted = {
                rect.left + offsets[i][0],
                rect.top + offsets[i][1],
                rect.right + offsets[i][0],
                rect.bottom + offsets[i][1]};

            DrawTextW(dc, text, -1, &shifted,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    else
    {
        DrawTextW(dc, text, -1, &rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /*
        All channels are equal in a grayscale image; the red channel of
        a BGRA pixel sits at offset 2.
    */
    const unsigned char *source = (const unsigned char *)bits;

    for (int i = 0; i < ICON_SIZE * ICON_SIZE; i++)
        coverage[i] = source[i * 4 + 2];

    SelectObject(dc, oldBitmap);

    if (oldFont)
        SelectObject(dc, oldFont);

    DeleteDC(dc);

    DeleteObject(bitmap);

    return TRUE;
}

/*
    Create a 32x32 tray icon containing the week number.
*/
static HICON CreateWeekIcon(int week)
{
    wchar_t text[8];

    swprintf(
        text,
        sizeof(text) / sizeof(text[0]),
        L"%d",
        week);

    /*
        Create a bold font.

        25px is large enough for both
        one-digit and two-digit week numbers.
    */
    HFONT font =
        CreateFontW(
            25,      /* height */
            0,       /* width */
            0,       /* escapement */
            0,       /* orientation */
            FW_BOLD, /* weight */
            FALSE,   /* italic */
            FALSE,   /* underline */
            FALSE,   /* strikeout */
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            L"Segoe UI");

    unsigned char textCoverage[ICON_SIZE * ICON_SIZE];
    unsigned char outlineCoverage[ICON_SIZE * ICON_SIZE];

    ZeroMemory(textCoverage, sizeof(textCoverage));
    ZeroMemory(outlineCoverage, sizeof(outlineCoverage));

    /*
        Nearly identical text and background colors would swallow the
        number, so it gets an outline in the opposite color then.
    */
    BOOL outlineNeeded =
        !g_style.backTransparent &&
        ColorsTooSimilar(g_style.textColor, g_style.backColor);

    COLORREF outlineColor =
        ColorLuma(g_style.textColor) >= 128 ?
            RGB(0, 0, 0) :
            RGB(255, 255, 255);

    BOOL coverageOk =
        RenderNumberCoverage(text, font, FALSE, textCoverage);

    if (outlineNeeded && coverageOk)
        coverageOk = RenderNumberCoverage(text, font, TRUE, outlineCoverage);

    if (font)
        DeleteObject(font);

    if (!coverageOk)
        return NULL;

    BITMAPINFO bmi = {0};

    bmi.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);

    bmi.bmiHeader.biWidth =
        ICON_SIZE;

    /*
        Negative height creates a top-down bitmap.
    */
    bmi.bmiHeader.biHeight =
        -ICON_SIZE;

    bmi.bmiHeader.biPlanes =
        1;

    bmi.bmiHeader.biBitCount =
        32;

    bmi.bmiHeader.biCompression =
        BI_RGB;

    void *pixels = NULL;

    HDC screenDC =
        GetDC(NULL);

    if (!screenDC)
        return NULL;

    HBITMAP colorBitmap =
        CreateDIBSection(
            screenDC,
            &bmi,
            DIB_RGB_COLORS,
            &pixels,
            NULL,
            0);

    ReleaseDC(NULL, screenDC);

    if (!colorBitmap || !pixels)
        return NULL;

    /*
        32-bit DIB pixels are BGRA, stored top-down.

        With an opaque background the glyph is blended over it, which
        is what keeps both picked colors. With a transparent background
        the alpha byte carries the glyph instead, so the taskbar shows
        through everywhere the number is not.
    */
    unsigned char *bytes = (unsigned char *)pixels;

    const int backRed = GetRValue(g_style.backColor);
    const int backGreen = GetGValue(g_style.backColor);
    const int backBlue = GetBValue(g_style.backColor);

    const int textRed = GetRValue(g_style.textColor);
    const int textGreen = GetGValue(g_style.textColor);
    const int textBlue = GetBValue(g_style.textColor);

    const int outlineRed = GetRValue(outlineColor);
    const int outlineGreen = GetGValue(outlineColor);
    const int outlineBlue = GetBValue(outlineColor);

    /*
        AND mask of the icon: a set bit marks a fully transparent
        pixel. It is kept in step with the alpha channel so that both
        alpha blending and plain mask blitting show the same result.
    */
    unsigned char maskBits[ICON_MASK_STRIDE * ICON_SIZE];

    ZeroMemory(maskBits, sizeof(maskBits));

    for (int y = 0; y < ICON_SIZE; y++)
    {
        for (int x = 0; x < ICON_SIZE; x++)
        {
            const int index = y * ICON_SIZE + x;

            int coverage = textCoverage[index];

            int glyphRed = textRed;
            int glyphGreen = textGreen;
            int glyphBlue = textBlue;

            /*
                The outline wins wherever it covers more of the pixel
                than the number itself.
            */
            if (outlineCoverage[index] > coverage)
            {
                coverage = outlineCoverage[index];

                glyphRed = outlineRed;
                glyphGreen = outlineGreen;
                glyphBlue = outlineBlue;
            }

            unsigned char *pixel = &bytes[index * 4];

            if (g_style.backTransparent)
            {
                pixel[0] = (unsigned char)glyphBlue;
                pixel[1] = (unsigned char)glyphGreen;
                pixel[2] = (unsigned char)glyphRed;
                pixel[3] = (unsigned char)coverage;
            }
            else
            {
                pixel[0] = (unsigned char)MixChannel(backBlue, glyphBlue, coverage);
                pixel[1] = (unsigned char)MixChannel(backGreen, glyphGreen, coverage);
                pixel[2] = (unsigned char)MixChannel(backRed, glyphRed, coverage);
                pixel[3] = 0xFF;
            }

            if (pixel[3] == 0)
                maskBits[y * ICON_MASK_STRIDE + (x >> 3)] |=
                    (unsigned char)(0x80 >> (x & 7));
        }
    }

    /*
        Monochrome AND mask built from the alpha channel.
    */
    BITMAP maskDescription = {0};

    maskDescription.bmType = 0;
    maskDescription.bmWidth = ICON_SIZE;
    maskDescription.bmHeight = ICON_SIZE;
    maskDescription.bmWidthBytes = ICON_MASK_STRIDE;
    maskDescription.bmPlanes = 1;
    maskDescription.bmBitsPixel = 1;
    maskDescription.bmBits = maskBits;

    HBITMAP maskBitmap =
        CreateBitmapIndirect(
            &maskDescription);

    if (!maskBitmap)
    {
        DeleteObject(colorBitmap);
        return NULL;
    }

    ICONINFO iconInfo = {0};

    iconInfo.fIcon =
        TRUE;

    iconInfo.xHotspot =
        0;

    iconInfo.yHotspot =
        0;

    iconInfo.hbmMask =
        maskBitmap;

    iconInfo.hbmColor =
        colorBitmap;

    HICON icon =
        CreateIconIndirect(
            &iconInfo);

    DeleteObject(maskBitmap);
    DeleteObject(colorBitmap);

    return icon;
}

/*
    Update the tray icon with the current week number.
*/
static void UpdateTrayIcon(HWND hwnd)
{
    SYSTEMTIME now;

    GetLocalTime(&now);

    int week =
        GetISOWeek(
            now.wYear,
            now.wMonth,
            now.wDay);

    if (week <= 0)
        return;

    HICON newIcon =
        CreateWeekIcon(week);

    if (!newIcon)
        return;

    /*
        IMPORTANT:
        Explicitly use NOTIFYICONDATAW because
        Shell_NotifyIconW expects NOTIFYICONDATAW.
    */
    NOTIFYICONDATAW nid = {0};

    nid.cbSize =
        sizeof(nid);

    nid.hWnd =
        hwnd;

    nid.uID =
        ID_TRAY;

    nid.uFlags =
        NIF_ICON | NIF_TIP;

    nid.hIcon =
        newIcon;

    swprintf(
        nid.szTip,
        sizeof(nid.szTip) / sizeof(nid.szTip[0]),
        L"Week Number %d",
        week);

    Shell_NotifyIconW(
        NIM_MODIFY,
        &nid);

    /*
        The shell has its own copy/reference
        of the icon, so release our previous
        icon after updating it.
    */
    if (g_trayIcon)
    {
        DestroyIcon(
            g_trayIcon);
    }

    g_trayIcon =
        newIcon;
}

/*
    Custom color dialog.

    The dialog is described by a dialog template assembled in memory,
    so the project keeps compiling from plain .c files without needing
    a .rc resource.
*/

/*
    Ordinals of the predefined window classes used in the template.
*/
#define CLASS_BUTTON 0x0080
#define CLASS_EDIT 0x0081
#define CLASS_STATIC 0x0082

/*
    Dialog size in dialog units.
*/
#define DIALOG_WIDTH 240
#define DIALOG_HEIGHT 118

/*
    The template contents are fixed, so this buffer is
    simply sized generously.
*/
#define TEMPLATE_SIZE 4096

typedef struct
{
    unsigned char *bytes;
    size_t capacity;
    size_t length;
    int items;
} TemplateBuilder;

/*
    Template blocks always start on a double word boundary.
*/
static void TemplateAlign(TemplateBuilder *builder)
{
    while ((builder->length % 4U) != 0U)
    {
        builder->bytes[builder->length++] = 0;
    }
}

static void TemplatePutU16(TemplateBuilder *builder, WORD value)
{
    builder->bytes[builder->length++] = (unsigned char)(value & 0xFF);
    builder->bytes[builder->length++] = (unsigned char)((value >> 8) & 0xFF);
}

static void TemplatePutU32(TemplateBuilder *builder, DWORD value)
{
    TemplatePutU16(builder, (WORD)(value & 0xFFFF));
    TemplatePutU16(builder, (WORD)((value >> 16) & 0xFFFF));
}

static void TemplatePutText(TemplateBuilder *builder, const wchar_t *text)
{
    int characters = 0;

    for (const wchar_t *cursor = text; *cursor != L'\0'; cursor++)
    {
        TemplatePutU16(builder, (WORD)*cursor);
        characters++;
    }

    TemplatePutU16(builder, 0);
    characters++;

    /*
        Strings inside a dialog template are padded so that their size,
        terminators included, is a multiple of 4 bytes. Without this
        extra null the following control is read from the wrong offset
        and the dialog cannot be created.
    */
    if ((characters % 2) != 0)
        TemplatePutU16(builder, 0);
}

static void TemplateBegin(TemplateBuilder *builder, unsigned char *storage,
                          size_t capacity, const wchar_t *title)
{
    builder->bytes = storage;
    builder->capacity = capacity;
    builder->length = 0;
    builder->items = 0;

    ZeroMemory(storage, capacity);

    TemplatePutU32(builder,
                   DS_MODALFRAME | DS_SETFONT | DS_CENTER |
                       WS_POPUP | WS_CAPTION | WS_VISIBLE);

    TemplatePutU32(builder, 0); /* extended styles */
    TemplatePutU16(builder, 0); /* item count, patched in TemplateEnd */
    TemplatePutU16(builder, 0); /* x, position chosen by DS_CENTER */
    TemplatePutU16(builder, 0); /* y */
    TemplatePutU16(builder, DIALOG_WIDTH);
    TemplatePutU16(builder, DIALOG_HEIGHT);
    TemplatePutU16(builder, 0); /* menu */
    TemplatePutU16(builder, 0); /* predefined dialog window class */

    TemplatePutText(builder, title);

    /*
        Required by DS_SETFONT: point size, then the typeface.
    */
    TemplatePutU16(builder, 9);
    TemplatePutText(builder, L"Segoe UI");
}

static void TemplateAddItem(TemplateBuilder *builder, WORD windowClass,
                            DWORD style, DWORD extendedStyle,
                            short x, short y, short cx, short cy,
                            WORD id, const wchar_t *label)
{
    TemplateAlign(builder);

    TemplatePutU32(builder, style);
    TemplatePutU32(builder, extendedStyle);
    TemplatePutU16(builder, (WORD)x);
    TemplatePutU16(builder, (WORD)y);
    TemplatePutU16(builder, (WORD)cx);
    TemplatePutU16(builder, (WORD)cy);
    TemplatePutU16(builder, id);

    /*
        0xFFFF followed by the ordinal of a predefined control class.
    */
    TemplatePutU16(builder, 0xFFFF);
    TemplatePutU16(builder, windowClass);

    TemplatePutText(builder, label);

    builder->items++;
}

static void TemplateEnd(TemplateBuilder *builder)
{
    /*
        Patch the control count into the DLGTEMPLATE header.
    */
    builder->bytes[8] = (unsigned char)(builder->items & 0xFF);
    builder->bytes[9] = (unsigned char)((builder->items >> 8) & 0xFF);
}

/*
    Colors currently shown by the two preview swatches; they follow
    what is typed in the hex fields so the effect is visible right away.
*/
static COLORREF g_previewText = RGB(0, 0, 0);
static COLORREF g_previewBack = RGB(255, 255, 255);
static BOOL g_previewTextValid = TRUE;
static BOOL g_previewBackValid = TRUE;
static BOOL g_previewTransparent = FALSE;

static void SetEditColor(HWND dialog, int id, COLORREF color)
{
    wchar_t text[8];

    FormatHexColor(color, text);

    SetWindowTextW(GetDlgItem(dialog, id), text);
}

static BOOL GetEditColor(HWND dialog, int id, COLORREF *color)
{
    wchar_t text[32];

    GetWindowTextW(GetDlgItem(dialog, id), text,
                   (int)(sizeof(text) / sizeof(text[0])));

    return ParseHexColor(text, color);
}

static BOOL DialogIsTransparent(HWND dialog)
{
    return SendDlgItemMessageW(dialog, IDC_TRANSPARENT, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/*
    Enable or disable the background controls depending on the
    "Transparent background" checkbox, and keep the hint line in sync.
*/
static void UpdateBackgroundControls(HWND dialog)
{
    BOOL transparent = DialogIsTransparent(dialog);

    EnableWindow(GetDlgItem(dialog, IDC_BACK_EDIT), !transparent);
    EnableWindow(GetDlgItem(dialog, IDC_BACK_PICK), !transparent);

    SetDlgItemTextW(dialog, IDC_HINT,
                    transparent ?
                        L"Background stays transparent: the taskbar shows through." :
                        L"Accepts #RRGGBB, RRGGBB or 0xRRGGBB.");
}

static void RefreshPreview(HWND dialog)
{
    g_previewTextValid = GetEditColor(dialog, IDC_TEXT_EDIT, &g_previewText);

    if (!g_previewTextValid)
        g_previewText = RGB(0, 0, 0);

    g_previewTransparent = DialogIsTransparent(dialog);

    if (g_previewTransparent)
    {
        g_previewBackValid = TRUE;
        g_previewBack = RGB(255, 255, 255);
    }
    else
    {
        g_previewBackValid = GetEditColor(dialog, IDC_BACK_EDIT, &g_previewBack);

        if (!g_previewBackValid)
            g_previewBack = RGB(0, 0, 0);
    }

    InvalidateRect(GetDlgItem(dialog, IDC_SWATCH_TEXT), NULL, TRUE);
    InvalidateRect(GetDlgItem(dialog, IDC_SWATCH_BACK), NULL, TRUE);
}

/*
    Paint one of the owner drawn preview boxes.
*/
static void PaintSwatch(const DRAWITEMSTRUCT *item, COLORREF color,
                        BOOL valid, BOOL transparent)
{
    HDC dc = item->hDC;
    RECT area = item->rcItem;

    int width = area.right - area.left;
    int height = area.bottom - area.top;

    HBRUSH brush = NULL;

    if (transparent)
    {
        /*
            A checkerboard stands for "no background at all".
        */
        brush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(dc, &area, brush);
        DeleteObject(brush);

        brush = CreateSolidBrush(RGB(200, 200, 200));

        for (int y = 0; y < height; y += 4)
        {
            for (int x = 0; x < width; x += 4)
            {
                if (((x / 4) + (y / 4)) % 2 != 0)
                    continue;

                RECT cell = {
                    area.left + x,
                    area.top + y,
                    area.left + (x + 4 > width ? width : x + 4),
                    area.top + (y + 4 > height ? height : y + 4)};

                FillRect(dc, &cell, brush);
            }
        }

        DeleteObject(brush);
    }
    else
    {
        /*
            Invalid hex input is reported with a red swatch.
        */
        brush = CreateSolidBrush(valid ? color : RGB(232, 64, 64));
        FillRect(dc, &area, brush);
        DeleteObject(brush);
    }

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));

    Rectangle(dc, area.left, area.top, area.right, area.bottom);

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
}

/*
    Open the standard Windows color picker and write the chosen
    color back into the matching hex field.
*/
static void PickColor(HWND dialog, BOOL forText)
{
    int editId = forText ? IDC_TEXT_EDIT : IDC_BACK_EDIT;

    COLORREF initial = 0;

    if (!GetEditColor(dialog, editId, &initial))
        initial = forText ? g_previewText : g_previewBack;

    CHOOSECOLORW chooser = {0};

    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = dialog;
    chooser.lpCustColors = g_pickerCustomColors;
    chooser.rgbResult = initial;
    chooser.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;

    if (ChooseColorW(&chooser))
    {
        SetEditColor(dialog, editId, chooser.rgbResult);
        RefreshPreview(dialog);
    }
}

/*
    Refuse unparseable hex input and select the field for fixing.
*/
static void RejectEdit(HWND dialog, int editId)
{
    SetDlgItemTextW(dialog, IDC_HINT,
                    L"Invalid color: use six hex digits, for example #FF8800");

    MessageBeep(MB_ICONWARNING);

    HWND edit = GetDlgItem(dialog, editId);

    SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, 0, (LPARAM)-1);
}

/*
    Dialog procedure of the custom color dialog.
*/
static INT_PTR CALLBACK StyleDialogProc(HWND dialog, UINT message,
                                        WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        SetEditColor(dialog, IDC_TEXT_EDIT, g_dialogStyle.textColor);
        SetEditColor(dialog, IDC_BACK_EDIT, g_dialogStyle.backColor);

        SendDlgItemMessageW(dialog, IDC_TRANSPARENT, BM_SETCHECK,
                            g_dialogStyle.backTransparent ? BST_CHECKED : BST_UNCHECKED,
                            0);

        UpdateBackgroundControls(dialog);
        RefreshPreview(dialog);

        /*
            A tray application is usually not the foreground process,
            so bring the dialog to the front explicitly.
        */
        BringWindowToTop(dialog);
        SetForegroundWindow(dialog);

        HWND firstEdit = GetDlgItem(dialog, IDC_TEXT_EDIT);

        SetFocus(firstEdit);
        SendMessageW(firstEdit, EM_SETSEL, 0, (LPARAM)-1);

        return FALSE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_TEXT_EDIT:
        case IDC_BACK_EDIT:
            if (HIWORD(wParam) == EN_CHANGE)
                RefreshPreview(dialog);

            return TRUE;

        case IDC_TRANSPARENT:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                UpdateBackgroundControls(dialog);
                RefreshPreview(dialog);
            }

            return TRUE;

        case IDC_TEXT_PICK:
            PickColor(dialog, TRUE);
            return TRUE;

        case IDC_BACK_PICK:
            PickColor(dialog, FALSE);
            return TRUE;

        case IDOK:
        {
            COLORREF textColor = 0;
            COLORREF backColor = 0;

            BOOL transparent = DialogIsTransparent(dialog);

            if (!GetEditColor(dialog, IDC_TEXT_EDIT, &textColor))
            {
                RejectEdit(dialog, IDC_TEXT_EDIT);
                return TRUE;
            }

            if (!transparent && !GetEditColor(dialog, IDC_BACK_EDIT, &backColor))
            {
                RejectEdit(dialog, IDC_BACK_EDIT);
                return TRUE;
            }

            g_dialogStyle.textColor = textColor;

            if (!transparent)
                g_dialogStyle.backColor = backColor;

            g_dialogStyle.backTransparent = transparent;
            g_dialogStyle.preset = PRESET_CUSTOM;

            EndDialog(dialog, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }

        break;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT *item = (const DRAWITEMSTRUCT *)lParam;

        if (item->CtlID == IDC_SWATCH_TEXT)
        {
            PaintSwatch(item, g_previewText, g_previewTextValid, FALSE);
            return TRUE;
        }

        if (item->CtlID == IDC_SWATCH_BACK)
        {
            PaintSwatch(item, g_previewBack, g_previewBackValid, g_previewTransparent);
            return TRUE;
        }

        break;
    }

    case WM_CLOSE:
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

/*
    Show the modal custom color dialog and apply whatever it returns.
*/
static void ShowStyleDialog(void)
{
    unsigned char storage[TEMPLATE_SIZE];
    TemplateBuilder builder;

    g_dialogStyle = g_style;

    TemplateBegin(&builder, storage, sizeof(storage),
                  L"Week Number Icon - colors");

    /*
        Creation order defines the tab order: text hex field,
        text picker, background hex field, background picker,
        transparent checkbox, OK, Cancel.
    */
    TemplateAddItem(&builder, CLASS_STATIC,
                    WS_CHILD | WS_VISIBLE | SS_LEFT, 0,
                    7, 8, 84, 8, 0, L"Text color");

    TemplateAddItem(&builder, CLASS_EDIT,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    WS_EX_CLIENTEDGE,
                    7, 18, 84, 13, IDC_TEXT_EDIT, L"");

    TemplateAddItem(&builder, CLASS_BUTTON,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                    96, 17, 34, 15, IDC_TEXT_PICK, L"Pick...");

    TemplateAddItem(&builder, CLASS_STATIC,
                    WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, WS_EX_STATICEDGE,
                    136, 18, 24, 13, IDC_SWATCH_TEXT, L"");

    TemplateAddItem(&builder, CLASS_STATIC,
                    WS_CHILD | WS_VISIBLE | SS_LEFT, 0,
                    7, 36, 84, 8, 0, L"Background color");

    TemplateAddItem(&builder, CLASS_EDIT,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    WS_EX_CLIENTEDGE,
                    7, 46, 84, 13, IDC_BACK_EDIT, L"");

    TemplateAddItem(&builder, CLASS_BUTTON,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                    96, 45, 34, 15, IDC_BACK_PICK, L"Pick...");

    TemplateAddItem(&builder, CLASS_STATIC,
                    WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, WS_EX_STATICEDGE,
                    136, 46, 24, 13, IDC_SWATCH_BACK, L"");

    TemplateAddItem(&builder, CLASS_BUTTON,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0,
                    7, 64, 150, 10, IDC_TRANSPARENT, L"Transparent background");

    TemplateAddItem(&builder, CLASS_STATIC,
                    WS_CHILD | WS_VISIBLE | SS_LEFT, 0,
                    7, 80, 180, 8, IDC_HINT, L"");

    TemplateAddItem(&builder, CLASS_BUTTON,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0,
                    154, 94, 36, 15, IDOK, L"OK");

    TemplateAddItem(&builder, CLASS_BUTTON,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                    194, 94, 36, 15, IDCANCEL, L"Cancel");

    TemplateEnd(&builder);

    /*
        The tray window is message-only and therefore cannot own a
        visible dialog, so the dialog is created without an owner.
    */
    INT_PTR result = DialogBoxIndirectParamW(
        NULL,
        (LPCDLGTEMPLATE)storage,
        NULL,
        StyleDialogProc,
        0);

    if (result == IDOK)
        ApplyStyle(&g_dialogStyle);
}

/*
    Window procedure.
*/
LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_TRAYICON:
    {
        if (lParam == WM_RBUTTONUP ||
            lParam == WM_CONTEXTMENU)
        {
            HMENU menu =
                CreatePopupMenu();

            if (!menu)
                break;

            /*
                Colors submenu: presets plus a custom dialog.

                The active choice gets a check mark.
            */
            HMENU colorsMenu =
                CreatePopupMenu();

            if (colorsMenu)
            {
                for (int i = 0; i < PRESET_COUNT; i++)
                {
                    AppendMenuW(
                        colorsMenu,
                        MF_STRING |
                            (g_style.preset == i ? MF_CHECKED : 0),
                        (UINT_PTR)(ID_PRESET + i),
                        g_presets[i].name);
                }

                AppendMenuW(
                    colorsMenu,
                    MF_SEPARATOR,
                    0,
                    NULL);

                AppendMenuW(
                    colorsMenu,
                    MF_STRING |
                        (g_style.preset == PRESET_CUSTOM ? MF_CHECKED : 0),
                    ID_CUSTOM,
                    L"Custom...");

                AppendMenuW(
                    menu,
                    MF_POPUP,
                    (UINT_PTR)colorsMenu,
                    L"Colors");
            }

            AppendMenuW(
                menu,
                MF_SEPARATOR,
                0,
                NULL);

            AppendMenuW(
                menu,
                MF_STRING,
                ID_EXIT,
                L"Exit");

            POINT cursor;

            GetCursorPos(&cursor);

            /*
                Required so the menu closes when
                the user clicks elsewhere.
            */
            SetForegroundWindow(hwnd);

            TrackPopupMenu(
                menu,
                TPM_RIGHTBUTTON |
                    TPM_BOTTOMALIGN,
                cursor.x,
                cursor.y,
                0,
                hwnd,
                NULL);

            PostMessageW(
                hwnd,
                WM_NULL,
                0,
                0);

            DestroyMenu(menu);
        }

        break;
    }

    case WM_COMMAND:
    {
        WORD command = LOWORD(wParam);

        if (command == ID_EXIT)
        {
            DestroyWindow(hwnd);
        }
        else if (command >= ID_PRESET &&
                 command < ID_PRESET + PRESET_COUNT)
        {
            ApplyPreset((int)(command - ID_PRESET));
        }
        else if (command == ID_CUSTOM)
        {
            ShowStyleDialog();
        }

        break;
    }

    case WM_TIMER:
    {
        if (wParam == ID_TIMER)
        {
            /*
                Update every minute.

                This means the icon automatically
                changes when the ISO week changes.
            */
            UpdateTrayIcon(hwnd);
        }

        break;
    }

    case WM_DESTROY:
    {
        KillTimer(
            hwnd,
            ID_TIMER);

        if (g_trayIcon)
        {
            DestroyIcon(
                g_trayIcon);

            g_trayIcon = NULL;
        }

        PostQuitMessage(0);

        break;
    }
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam);
}

/*
    Windows entry point.
*/
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    /*
        0. Restore the colors chosen earlier, if any.
    */

    LoadSettings();

    /*
        1. Register window class.
    */

    const wchar_t CLASS_NAME[] =
        L"WeekNumberTrayIcon";

    WNDCLASSW wc = {0};

    wc.lpfnWndProc =
        WindowProc;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        CLASS_NAME;

    if (!RegisterClassW(&wc))
        return 1;

    /*
        2. Create hidden message-only window.
    */

    HWND hwnd =
        CreateWindowExW(
            0,
            CLASS_NAME,
            L"Week Number Icon",
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            NULL,
            hInstance,
            NULL);

    if (!hwnd)
        return 1;

    /*
        Remembered so a color change can repaint the icon
        from anywhere.
    */
    g_mainWnd = hwnd;

    /*
        3. Determine current ISO week.
    */

    SYSTEMTIME now;

    GetLocalTime(&now);

    int week =
        GetISOWeek(
            now.wYear,
            now.wMonth,
            now.wDay);

    /*
        4. Create the initial tray icon.
    */

    g_trayIcon =
        CreateWeekIcon(week);

    if (!g_trayIcon)
        return 1;

    /*
        IMPORTANT:
        Use NOTIFYICONDATAW together with
        Shell_NotifyIconW.
    */
    NOTIFYICONDATAW nid = {0};

    nid.cbSize =
        sizeof(nid);

    nid.hWnd =
        hwnd;

    nid.uID =
        ID_TRAY;

    nid.uFlags =
        NIF_ICON |
        NIF_MESSAGE |
        NIF_TIP;

    nid.uCallbackMessage =
        WM_TRAYICON;

    nid.hIcon =
        g_trayIcon;

    swprintf(
        nid.szTip,
        sizeof(nid.szTip) /
            sizeof(nid.szTip[0]),
        L"Week Number %d",
        week);

    if (!Shell_NotifyIconW(
            NIM_ADD,
            &nid))
    {
        DestroyIcon(
            g_trayIcon);

        g_trayIcon = NULL;

        return 1;
    }

    /*
        Ask Windows to send WM_TIMER every
        60 seconds.

        This allows the icon to automatically
        change from, for example, week 39
        to week 40.
    */
    SetTimer(
        hwnd,
        ID_TIMER,
        60 * 1000,
        NULL);

    /*
        5. Standard Windows message loop.
    */

    MSG msg;

    while (GetMessageW(
               &msg,
               NULL,
               0,
               0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /*
        6. Remove tray icon.
    */

    g_mainWnd = NULL;

    Shell_NotifyIconW(
        NIM_DELETE,
        &nid);

    return 0;
}
