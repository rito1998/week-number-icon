#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY 1
#define ID_TIMER 1
#define ID_EXIT 100

#define ICON_SIZE 28

static HICON g_trayIcon = NULL;

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
    Create a 32x32 tray icon containing the week number.
*/
static HICON CreateWeekIcon(int week)
{
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
        Make the bitmap transparent.

        32-bit DIB pixels are BGRA.
    */
    ZeroMemory(
        pixels,
        ICON_SIZE * ICON_SIZE * 4);

    HDC dc =
        CreateCompatibleDC(NULL);

    if (!dc)
    {
        DeleteObject(colorBitmap);
        return NULL;
    }

    HGDIOBJ oldBitmap =
        SelectObject(dc, colorBitmap);

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

    HFONT oldFont = NULL;

    if (font)
    {
        oldFont =
            SelectObject(dc, font);
    }

    /*
        Transparent text background.
    */
    SetBkMode(
        dc,
        TRANSPARENT);

    /*
        White text.
    */
    SetTextColor(
        dc,
        RGB(255, 255, 255));

    wchar_t text[8];

    swprintf(
        text,
        sizeof(text) / sizeof(text[0]),
        L"%d",
        week);

    /*
        Draw the number centered in the icon.
    */
    RECT rect = {
        0,
        0,
        ICON_SIZE,
        ICON_SIZE};

    DrawTextW(
        dc,
        text,
        -1,
        &rect,
        DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE);

    /*
        Restore font and clean up.
    */
    if (font)
    {
        SelectObject(
            dc,
            oldFont);

        DeleteObject(font);
    }

    SelectObject(
        dc,
        oldBitmap);

    DeleteDC(dc);

    /*
        Create a monochrome mask.

        All zero means the color bitmap
        supplies the icon pixels.
    */
    HBITMAP maskBitmap =
        CreateBitmap(
            ICON_SIZE,
            ICON_SIZE,
            1,
            1,
            NULL);

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
        if (LOWORD(wParam) == ID_EXIT)
        {
            DestroyWindow(hwnd);
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

    Shell_NotifyIconW(
        NIM_DELETE,
        &nid);

    return 0;
}
