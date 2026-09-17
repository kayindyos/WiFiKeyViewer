// WiFiKeyViewer — small Win32 utility that lists Wi-Fi profiles saved on THIS PC
// and shows their passwords (key content) using the standard Windows `netsh` command.
//
// Use only on a device you own. It reads credentials already stored locally by the
// current user; it does not attack, scan, or access any other machine or network.
//
// Author of this adaptation: YOUR_NAME
// Based on an original MIT-licensed project by MR.ShadowMan (2025). See LICENSE.
//
// Build (MinGW):
//   g++ wifikeyviewer.cpp -o WiFiKeyViewer.exe -mwindows -std=c++17 \
//       -lgdiplus -lwinmm -lole32 -luuid -lcomctl32

#include <windows.h>
#include <mmsystem.h>
#include <gdiplus.h>
#include <cstdlib>
#include <vector>
#include <string>
#include <ctime>
#include <fstream>

using namespace Gdiplus;
using namespace std;

HWND      gLogBox = NULL;
HWND      gFooter = NULL;
HBRUSH    gBgBrush = NULL;
COLORREF  gNeon = RGB(0,255,0);
ULONG_PTR gGdiplusToken = 0;
Image*    gCurrentImage = nullptr;

vector<wstring> gImages = {
    L"images\\image (1).jpg",
    L"images\\image (1).png",
    L"images\\image (2).jpg",
    L"images\\image (3).jpg",
    L"images\\image (4).jpg",
    L"images\\image (5).jpg",
    L"images\\image (6).jpg"
};
const UINT TIMER_IMAGE = 2001;

// last results kept in memory so we can export them
static vector<string> gLastResults;

// ---- append to on-screen log and to logs.log ----
static void AppendLog(const string &s) {
    if (!gLogBox) return;
    int len = GetWindowTextLengthA(gLogBox);
    SendMessageA(gLogBox, EM_SETSEL, len, len);
    SendMessageA(gLogBox, EM_REPLACESEL, FALSE, (LPARAM)s.c_str());
    SendMessageA(gLogBox, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
    SendMessageA(gLogBox, EM_SCROLLCARET, 0, 0);

    ofstream logFile("logs.log", ios::app);
    if (logFile.is_open()) logFile << s << "\n";
}

// ---- owner-drawn button ----
static void DrawOwnerButton(LPDRAWITEMSTRUCT dis, const char* text) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    BOOL pressed = (dis->itemState & ODS_SELECTED) != 0;
    BOOL focus   = (dis->itemState & ODS_FOCUS) != 0;

    COLORREF fill   = pressed ? RGB(45,45,45) : RGB(35,35,35);
    COLORREF border = pressed ? RGB(70,70,70) : RGB(90,90,90);

    HBRUSH b   = CreateSolidBrush(fill);
    HPEN   pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldB = SelectObject(hdc, b);
    HGDIOBJ oldP = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);

    HPEN pen2 = CreatePen(PS_INSIDEFRAME, 1, RGB(60,60,60));
    SelectObject(hdc, pen2);
    RECT r2 = rc; InflateRect(&r2, -1, -1);
    RoundRect(hdc, r2.left, r2.top, r2.right, r2.bottom, 8, 8);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0,255,0));
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ oldF = SelectObject(hdc, hFont);
    DrawTextA(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (focus) { RECT r3 = rc; InflateRect(&r3, -3, -3); DrawFocusRect(hdc, &r3); }

    SelectObject(hdc, oldF);
    SelectObject(hdc, oldP);
    SelectObject(hdc, oldB);
    DeleteObject(b);
    DeleteObject(pen);
    DeleteObject(pen2);
}

// ---- rotate background image ----
static void ShowRandomImage(HWND hwnd) {
    if (gCurrentImage) delete gCurrentImage;
    int idx = rand() % gImages.size();
    gCurrentImage = new Image(gImages[idx].c_str());
    InvalidateRect(hwnd, NULL, TRUE);
}

// ---- read saved Wi-Fi profiles + key content on THIS machine ----
static void ListSavedWifiKeys() {
    gLastResults.clear();
    AppendLog("[INFO] Reading Wi-Fi profiles saved on this PC...");

    const char* cmd =
        "powershell -NoProfile -Command \""
        "$profiles = netsh wlan show profiles | Select-String 'All User Profile' | "
        "ForEach-Object { ($_ -split ':')[1].Trim() }; "
        "if (-not $profiles) { 'No Wi-Fi profiles found on this PC.' } "
        "else { foreach ($p in $profiles) { "
        "$d = netsh wlan show profile name=\\\"$p\\\" key=clear; "
        "$k = ($d | Select-String 'Key Content') -replace '.*:\\s*',''; "
        "if (-not $k) { $k = '(open network / no password)' }; "
        "\\\"$p = $k\\\" } }\"";

    FILE* pipe = popen(cmd, "r");
    if (!pipe) { AppendLog("[ERROR] Failed to run PowerShell."); return; }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        string line = buffer;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        if (!line.empty()) { gLastResults.push_back(line); AppendLog("  " + line); }
    }
    pclose(pipe);

    AppendLog("[DONE] Found " + to_string(gLastResults.size()) + " profile(s).");
    AppendLog("-------------------------------------------");
}

// ---- export last results to a timestamped text report ----
static void ExportReport() {
    if (gLastResults.empty()) { AppendLog("[!] Nothing to export. Run a scan first."); return; }

    time_t t = time(NULL);
    char stamp[32]; strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", localtime(&t));

    ofstream f("wifi_report.txt", ios::trunc);
    if (!f.is_open()) { AppendLog("[ERROR] Could not write wifi_report.txt"); return; }
    f << "WiFiKeyViewer report\nGenerated: " << stamp << "\n";
    f << "-------------------------------------------\n";
    for (const auto& r : gLastResults) f << r << "\n";
    f.close();

    AppendLog("[DONE] Saved report to wifi_report.txt");
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        {
            srand((unsigned int)time(NULL));

            CreateWindowA("STATIC", "=== WiFiKeyViewer ===\r\nSaved Wi-Fi keys on this PC",
                WS_CHILD|WS_VISIBLE|SS_CENTER, 10, 10, 450, 40, hwnd, (HMENU)100, GetModuleHandle(NULL), NULL);

            CreateWindowA("BUTTON", "Show Wi-Fi Keys", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
                30, 60, 130, 40, hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);
            CreateWindowA("BUTTON", "Export Report", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
                175, 60, 130, 40, hwnd, (HMENU)2, GetModuleHandle(NULL), NULL);
            CreateWindowA("BUTTON", "Exit", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
                320, 60, 130, 40, hwnd, (HMENU)3, GetModuleHandle(NULL), NULL);

            gLogBox = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                470, 10, 400, 400, hwnd, (HMENU)200, GetModuleHandle(NULL), NULL);

            HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, FF_DONTCARE, "Consolas");
            SendMessage(gLogBox, WM_SETFONT, (WPARAM)hFont, TRUE);

            AppendLog("[INFO] WiFiKeyViewer ready. Local use only.");
            AppendLog("-------------------------------------------");

            gFooter = CreateWindowA("STATIC", "WiFiKeyViewer - view your own saved Wi-Fi keys",
                WS_CHILD|WS_VISIBLE|SS_CENTER, 10, 420, 660, 20, hwnd, (HMENU)300, GetModuleHandle(NULL), NULL);

            ShowRandomImage(hwnd);
            SetTimer(hwnd, TIMER_IMAGE, 5000, NULL);
        }
        return 0;

    case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            if ((HWND)lParam == gFooter) SetTextColor(hdc, RGB(210,210,210));
            else SetTextColor(hdc, gNeon);
            SetBkColor(hdc, RGB(0,0,0));
            return (LRESULT)gBgBrush;
        }
    case WM_CTLCOLOREDIT:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, gNeon);
            SetBkColor(hdc, RGB(0,0,0));
            return (LRESULT)gBgBrush;
        }

    case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == 1) DrawOwnerButton(dis, "Show Wi-Fi Keys");
            else if (dis->CtlID == 2) DrawOwnerButton(dis, "Export Report");
            else if (dis->CtlID == 3) DrawOwnerButton(dis, "Exit");
            return TRUE;
        }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
            case 1: ListSavedWifiKeys(); break;
            case 2: ExportReport();      break;
            case 3: PostQuitMessage(0);  break;
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_IMAGE) ShowRandomImage(hwnd);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, gBgBrush);

            if (gCurrentImage) {
                Graphics g(hdc);
                int bannerWidth = 450;
                int bannerHeight = gCurrentImage->GetHeight() * bannerWidth / gCurrentImage->GetWidth();
                Rect r(10, 110, bannerWidth, bannerHeight);
                g.DrawImage(gCurrentImage, r);
                Pen pen(Color(255, 200, 0, 0), 2.0f);
                g.DrawRectangle(&pen, r);
            }

            RECT client; GetClientRect(hwnd, &client);
            SetWindowPos(gFooter, NULL, 10, client.bottom - 30, client.right - 20, 20,
                SWP_NOZORDER | SWP_NOACTIVATE);

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_IMAGE);
        if (gCurrentImage) delete gCurrentImage;
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    GdiplusStartupInput gpsi;
    GdiplusStartup(&gGdiplusToken, &gpsi, NULL);

    gBgBrush = CreateSolidBrush(RGB(0,0,0));

    // optional background music (first track, if present)
    mciSendStringA("open \"music\\\\music (1).mp3\" type mpegvideo alias bgm", NULL, 0, 0);
    mciSendStringA("play bgm repeat", NULL, 0, 0);

    const char* CLASS_NAME = "WiFiKeyViewerMainClass";
    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = MainProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = gBgBrush;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0, CLASS_NAME, "WiFiKeyViewer",
        WS_OVERLAPPED | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,
        120, 120, 900, 470, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mciSendStringA("close bgm", NULL, 0, 0);
    if (gCurrentImage) delete gCurrentImage;
    if (gGdiplusToken) GdiplusShutdown(gGdiplusToken);
    if (gBgBrush) DeleteObject(gBgBrush);
    return 0;
}
