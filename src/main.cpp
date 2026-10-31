// Headphones Guy - a pixel smiley on the Windows taskbar that nods to whatever is playing.
// Win32 + WASAPI loopback, no dependencies. Build: see build.sh / README.md
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cmath>
#include "tracker.h"

// ---------- sprites (16x16) ----------
// k outline, y face, e eyes/mouth, B headband, C/D/L cup (main / rim / highlight, from cups_color), t tongue
enum Frame { F_SIL_OPEN, F_SIL_BLINK, F_AWAKE_OPEN, F_AWAKE_BLINK, F_LISTEN, F_LOUD, F_PAUSE, F_BUILD, F_DROP, F_COUNT };
static const char* g_maps[F_COUNT][16] = {
    /* F_SIL_OPEN */ {"................","....kkkkkkkk....","...kyyyyyyyyk...","..kyyyyyyyyyyk..","..kyyyyyyyyyyk..","..kyyyyyyyyyyk..","..kyyeyyyyeyyk..","..kyyeyyyyeyyk..","..kyyyyyyyyyyk..","...kyyyeeyyyk...","....kyyyyyyk....",".DC..kkkkkk..CD.",".DL..........LD.","..BBBBBBBBBBBB..","................","................"},
    /* F_SIL_BLINK */ {"................","....kkkkkkkk....","...kyyyyyyyyk...","..kyyyyyyyyyyk..","..kyyyyyyyyyyk..","..kyyyyyyyyyyk..","..kyyyyyyyyyyk..","..kyeeeyyeeeyk..","..kyyyyyyyyyyk..","...kyyyeeyyyk...","....kyyyyyyk....",".DC..kkkkkk..CD.",".DL..........LD.","..BBBBBBBBBBBB..","................","................"},
    /* F_AWAKE_OPEN */ {"....BBBBBBBB....","..BBkkkkkkkkBB..",".B.kyyyyyyyyk.B.",".B.kyyyyyyyyk.B.",".BkyyyyyyyyyykB.",".DCyyyyyyyyyyCD.",".DLyyeyyyyeyyLD.",".DCyyeyyyyeyyCD.",".DCyyyyyyyyyyCD.",".DCyyyyyyyyyyCD.","..kyyyyeeyyyyk..","...kyyyyyyyyk...","....kkkkkkkk....","................","................","................"},
    /* F_AWAKE_BLINK */ {"....BBBBBBBB....","..BBkkkkkkkkBB..",".B.kyyyyyyyyk.B.",".B.kyyyyyyyyk.B.",".BkyyyyyyyyyykB.",".DCyyyyyyyyyyCD.",".DLyyyyyyyyyyLD.",".DCyeeeyyeeeyCD.",".DCyyyyyyyyyyCD.",".DCyyyyyyyyyyCD.","..kyyyyeeyyyyk..","...kyyyyyyyyk...","....kkkkkkkk....","................","................","................"},
    /* F_LISTEN */ {"....BBBBBBBB....","..BBkkkkkkkkBB..",".B.kyyyyyyyyk.B.",".B.kyyyyyyyyk.B.",".BkyyyyyyyyyykB.",".DCyyyyyyyyyyCD.",".DLyeyeyyeyeyLD.",".DCyyeyyyyeyyCD.",".DCyyyyyyyyyyCD.",".DCyyyyyyeyyyCD.","..kyyyyeeyyyyk..","...kyyyyyyyyk...","....kkkkkkkk....","................","................","................"},
    /* F_LOUD */ {"....BBBBBBBB....","..BBkkkkkkkkBB..",".B.kyyyyyyyyk.B.",".B.kyyyyyyyyk.B.",".BkyyyyyyyyyykB.",".DCyyyyyyyyyyCD.",".DLyeyeyyeyeyLD.",".DCyyeyyyyeyyCD.",".DCyyyyyyyyyyCD.",".DCyyyeyyeyyyCD.","..kyyyyeeyyyyk..","...kyyyyyyyyk...","....kkkkkkkk....","................","................","................"},
    /* F_PAUSE */ {"....BBBBBBBB....","..BBkkkkkkkkBB..",".B.kyyyyyyyyk.B.",".B.kyyyyyyyyk.B.",".BkyyyyyyyyyykB.",".DCyyyyyyyyyyCD.",".DLyeyeyyeyeyLD.",".DCyyeyyyyeyyCD.",".DCyyyyyyyyyyCD.",".DCyyyyyyyyyyCD.","..kyyyyeeyyyyk..","...kyyyyyyyyk...","....kkkkkkkk....","................","................","................"},
    /* F_BUILD */ {"....BBBBBBBB....","..BBkkkkkkkkBB..",".B.kyyyyyyyyk.B.",".B.kyyyyyyyyk.B.",".BkyyyyyyyyyykB.",".DCyyeyyyyeyyCD.",".DLyyeyyyyeyyLD.",".DCyyyyyyyyyyCD.",".DCyyyyyyyyyyCD.",".DCyyyyyyyyyyCD.","..kyyyeeeeyyyk..","...kyyyyyyyyk...","....kkkkkkkk....","................","................","................"},
    /* F_DROP */ {"....BBBBBBBB....","..BBkkkkkkkkBB..",".B.kyyyyyyyyk.B.",".B.kyyyyyyyyk.B.",".BkyyyyyyyyyykB.",".DCyeeyyyyeeyCD.",".DLyyyeyyeyyyLD.",".DCyeeyyyyeeyCD.",".DCyyyyyyyyyyCD.",".DCyyyeyyeyyyCD.","..kyyyyeeyyyyk..","...kyyyyyyyyk...","....kkkkkkkk....","................","................","................"},
};
// vertical placement in cells inside the 18-row box: rest / nod / loud nod / drop jump
enum { Y_REST = 2, Y_NOD = 3, Y_LOUD = 4, Y_JUMP = 1 };

// ---------- UI text (English, or Russian on a Russian Windows; override with language=en|ru in settings.ini) ----------
enum Str { S_APP, S_TIP_BPM, S_TIP_SILENCE, S_TIP_LISTEN, S_LEFT, S_RIGHT, S_RESET, S_OFFSET, S_EARLIER, S_LATER, S_EARLIER50, S_LATER50,
           S_COLOR, S_CUSTOM, S_MOVES, S_AUTOSTART, S_EXIT,
           S_PINK, S_RED, S_ORANGE, S_GREEN, S_CYAN, S_BLUE, S_PURPLE, S_WHITE, S_COUNT };
static const wchar_t* kEn[S_COUNT] = {
    L"Headphones Guy", L"Headphones Guy · %ld BPM", L"Headphones Guy · silence", L"Headphones Guy · listening",
    L"Move left", L"Move right", L"Reset position", L"Nod offset: %d ms", L"Nod earlier (−10 ms)", L"Nod later (+10 ms)",
    L"Much earlier (−50 ms)", L"Much later (+50 ms, Bluetooth)",
    L"Headphone color", L"Custom color...", L"Go harder on loud parts and drops", L"Start with Windows", L"Exit",
    L"Pink", L"Red", L"Orange", L"Green", L"Cyan", L"Blue", L"Purple", L"White" };
static const wchar_t* kRu[S_COUNT] = {
    L"Чел в наушниках", L"Чел в наушниках · %ld BPM", L"Чел в наушниках · тишина", L"Чел в наушниках · слушает",
    L"Сдвинуть левее", L"Сдвинуть правее", L"Вернуть на место", L"Сдвиг кивка: %d мс", L"Кивать раньше (−10 мс)", L"Кивать позже (+10 мс)",
    L"Сильно раньше (−50 мс)", L"Сильно позже (+50 мс, блютуз)",
    L"Цвет наушников", L"Свой цвет...", L"Качает сильнее на громком и на дропах", L"Запускать вместе с Windows", L"Выход",
    L"Розовые", L"Красные", L"Оранжевые", L"Зелёные", L"Голубые", L"Синие", L"Фиолетовые", L"Белые" };
static bool g_ru = false;
static const wchar_t* T_(Str id) { return (g_ru && kRu[id]) ? kRu[id] : kEn[id]; }

// ---------- settings ----------
struct Settings { int offset = 0; int latencyMs = 10; DWORD cups = 0xFF5D73; bool log = false; bool sharp = true; bool moves = true; } g_set;
static std::wstring g_iniPath;

static void LoadSettings() {
    wchar_t p[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, p))) {
        std::wstring dir = std::wstring(p) + L"\\HeadphonesGuy";
        CreateDirectoryW(dir.c_str(), NULL);
        g_iniPath = dir + L"\\settings.ini";
    }
    if (g_iniPath.empty()) return;
    const wchar_t* f = g_iniPath.c_str();
    g_set.offset = (int)GetPrivateProfileIntW(L"guy", L"offset", 0, f);
    g_set.latencyMs = (int)GetPrivateProfileIntW(L"guy", L"nod_offset_ms", 10, f);
    g_set.log = GetPrivateProfileIntW(L"guy", L"log", 0, f) != 0;
    g_set.sharp = GetPrivateProfileIntW(L"guy", L"sharp_onsets", 1, f) != 0;   // 0 = old 2048-window onset timing
    g_set.moves = GetPrivateProfileIntW(L"guy", L"moves", 1, f) != 0;                 // 0 = plain nods, no drop face / loudness
    wchar_t lang[16]; GetPrivateProfileStringW(L"guy", L"language", L"auto", lang, 16, f);
    if (!wcscmp(lang, L"en")) g_ru = false; else if (!wcscmp(lang, L"ru")) g_ru = true;
    wchar_t buf[32];
    GetPrivateProfileStringW(L"guy", L"cups_color", L"FF5D73", buf, 32, f);
    g_set.cups = wcstoul(buf, NULL, 16) & 0xFFFFFF;
}
static void SaveSettings() {
    if (g_iniPath.empty()) return;
    const wchar_t* f = g_iniPath.c_str(); wchar_t b[32];
    swprintf(b, 32, L"%d", g_set.offset); WritePrivateProfileStringW(L"guy", L"offset", b, f);
    swprintf(b, 32, L"%d", g_set.latencyMs); WritePrivateProfileStringW(L"guy", L"nod_offset_ms", b, f);
    swprintf(b, 32, L"%06X", (unsigned)g_set.cups); WritePrivateProfileStringW(L"guy", L"cups_color", b, f);
    WritePrivateProfileStringW(L"guy", L"moves", g_set.moves ? L"1" : L"0", f);
}

static DWORD Mix(DWORD rgb, double f) {   // f < 1 darker, f > 1 towards white
    int c[3] = {(int)(rgb >> 16) & 255, (int)(rgb >> 8) & 255, (int)rgb & 255};
    for (int i = 0; i < 3; i++) c[i] = f <= 1 ? (int)(c[i] * f) : (int)(c[i] + (255 - c[i]) * (f - 1));
    return 0xFF000000 | (c[0] << 16) | (c[1] << 8) | c[2];
}
static DWORD Color(char c) {
    switch (c) {
    case 'k': case 'e': return 0xFF3B2410;
    case 'y': return 0xFFFFC83D;
    case 't': return 0xFFFF6F7F;
    case 'B': return 0xFFC8CCD6;
    case 'C': return 0xFF000000 | g_set.cups;
    case 'D': return Mix(g_set.cups, 0.62);
    case 'L': return Mix(g_set.cups, 1.5);
    default: return 0;
    }
}

// ---------- globals ----------
static HINSTANCE g_inst;
static HWND g_wnd, g_msgWnd;
static volatile LONG g_running = 1;
static CRITICAL_SECTION g_cs;
static BeatTracker g_tracker;
static bool g_trackerReady = false;
static double g_qpcFreq = 1;
static int g_scale = 2, g_x = -1000, g_y = -1000, g_frame = F_SIL_OPEN, g_yoff = Y_REST, g_xoff = 0;
static bool g_visible = false;
static UINT WM_TASKBARCREATED = 0;
static NOTIFYICONDATAW g_nid = {};
static volatile LONG g_mode = MODE_SILENCE, g_bpmX10 = 0;

#define WM_APP_FRAME (WM_APP + 1)
#define WM_APP_TRAY  (WM_APP + 2)
#define TIMER_POS 1
#define TIMER_TIP 2

static double Now() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart / g_qpcFreq; }

// ---------- drawing ----------
static HDC g_memDC = NULL; static HBITMAP g_bmp = NULL; static DWORD* g_bits = NULL; static int g_bmpScale = 0;

static void DrawFrame(DWORD* px, int stride, int s, int frame, int yOffCells, int maxRows, int xOffCells = 0) {
    for (int r = 0; r < 16 && r + yOffCells < maxRows; r++)
        for (int c = 0; c < 16; c++) {
            int cs = c - xOffCells; if (cs < 0 || cs >= 16) continue;
            DWORD col = Color(g_maps[frame][r][cs]);
            if (!col) continue;
            for (int dy = 0; dy < s; dy++) {
                DWORD* row = px + (size_t)((r + yOffCells) * s + dy) * stride + c * s;
                for (int dx = 0; dx < s; dx++) row[dx] = col;
            }
        }
}

static void Render() {
    int s = g_scale, W = 16 * s, H = 18 * s;
    HDC screen = GetDC(NULL);
    if (g_bmpScale != s) {
        if (g_bmp) DeleteObject(g_bmp);
        if (!g_memDC) g_memDC = CreateCompatibleDC(screen);
        BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        g_bmp = CreateDIBSection(g_memDC, &bi, DIB_RGB_COLORS, (void**)&g_bits, NULL, 0);
        SelectObject(g_memDC, g_bmp);
        g_bmpScale = s;
    }
    memset(g_bits, 0, (size_t)W * H * 4);
    DrawFrame(g_bits, W, s, g_frame, g_yoff, 18, g_xoff);
    GdiFlush();
    POINT src = {0, 0}, dst = {g_x, g_y}; SIZE sz = {W, H};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(g_wnd, screen, &dst, &sz, g_memDC, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, screen);
}

static HICON MakeIcon() {
    const int S = 32;
    BITMAPV5HEADER bh = {}; bh.bV5Size = sizeof(bh); bh.bV5Width = S; bh.bV5Height = -S; bh.bV5Planes = 1; bh.bV5BitCount = 32;
    bh.bV5Compression = BI_BITFIELDS; bh.bV5RedMask = 0x00FF0000; bh.bV5GreenMask = 0x0000FF00; bh.bV5BlueMask = 0x000000FF; bh.bV5AlphaMask = 0xFF000000;
    HDC dc = GetDC(NULL); DWORD* bits = NULL;
    HBITMAP col = CreateDIBSection(dc, (BITMAPINFO*)&bh, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
    ReleaseDC(NULL, dc);
    memset(bits, 0, S * S * 4);
    DrawFrame(bits, S, 2, F_LISTEN, 1, 16);
    HBITMAP mask = CreateBitmap(S, S, 1, 1, NULL);
    ICONINFO ii = {}; ii.fIcon = TRUE; ii.hbmColor = col; ii.hbmMask = mask;
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(col); DeleteObject(mask);
    return ic;
}

// ---------- placement ----------
static bool FullscreenOn(HMONITOR mon) {
    QUERY_USER_NOTIFICATION_STATE q;
    if (SUCCEEDED(SHQueryUserNotificationState(&q)) && q == QUNS_RUNNING_D3D_FULL_SCREEN) return true;
    HWND fg = GetForegroundWindow(); if (!fg) return false;
    wchar_t cls[64] = {}; GetClassNameW(fg, cls, 64);
    if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd")) return false;
    if (MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST) != mon) return false;
    RECT r; GetWindowRect(fg, &r);
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
    return r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top && r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom;
}

static void UpdatePlacement() {
    HWND tb = FindWindowW(L"Shell_TrayWnd", NULL);
    bool show = false; int s = g_scale, x = g_x, y = g_y;
    if (tb) {
        RECT tr; GetWindowRect(tb, &tr);
        HMONITOR mon = MonitorFromWindow(tb, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
        RECT vis; IntersectRect(&vis, &tr, &mi.rcMonitor);
        int tw = tr.right - tr.left, th = tr.bottom - tr.top;
        bool horiz = tw >= th;
        int visThick = horiz ? (vis.bottom - vis.top) : (vis.right - vis.left);
        int thick = horiz ? th : tw;
        RECT nr = tr; bool haveNotify = false;
        HWND tn = FindWindowExW(tb, NULL, L"TrayNotifyWnd", NULL);
        if (tn) { GetWindowRect(tn, &nr); haveNotify = true; }
        s = std::max(1, (thick - 4) / 16);
        int W = 16 * s, H = 18 * s;
        if (horiz) {
            int right = haveNotify ? nr.left : tr.right - 200;
            x = right - W - 4 * s + g_set.offset;
            y = (tr.top + tr.bottom) / 2 - 9 * s;
        } else {
            int bottom = haveNotify ? nr.top : tr.bottom - 200;
            x = (tr.left + tr.right) / 2 - 8 * s;
            y = bottom - H - 4 * s + g_set.offset;
        }
        show = visThick >= thick / 2 && IsWindowVisible(tb) && !FullscreenOn(mon);
    }
    bool redraw = (s != g_scale) || x != g_x || y != g_y;
    g_scale = s; g_x = x; g_y = y;
    if (redraw) Render();
    if (show != g_visible) { ShowWindow(g_wnd, show ? SW_SHOWNOACTIVATE : SW_HIDE); g_visible = show; }
    if (show) SetWindowPos(g_wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

// ---------- audio ----------
static const GUID kFloatSub = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID kPcmSub   = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

template <class T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = NULL; } }

static std::wstring DefaultDeviceId(IMMDeviceEnumerator* en) {
    IMMDevice* d = NULL; std::wstring r;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &d))) { LPWSTR id = NULL; if (SUCCEEDED(d->GetId(&id))) { r = id; CoTaskMemFree(id); } d->Release(); }
    return r;
}

static void RunCapture() {
    IMMDeviceEnumerator* en = NULL; IMMDevice* dev = NULL; IAudioClient* ac = NULL; IAudioCaptureClient* cc = NULL; WAVEFORMATEX* wf = NULL;
    std::wstring devId; std::vector<float> mono;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) goto done;
    if (FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev))) goto done;
    devId = DefaultDeviceId(en);
    if (FAILED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&ac))) goto done;
    if (FAILED(ac->GetMixFormat(&wf))) goto done;
    if (FAILED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 2000000, 0, wf, NULL))) goto done;
    if (FAILED(ac->GetService(__uuidof(IAudioCaptureClient), (void**)&cc))) goto done;
    {
        bool isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
        if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            WAVEFORMATEXTENSIBLE* ex = (WAVEFORMATEXTENSIBLE*)wf;
            isFloat = IsEqualGUID(ex->SubFormat, kFloatSub);
            if (!isFloat && !IsEqualGUID(ex->SubFormat, kPcmSub)) goto done;
        }
        int ch = wf->nChannels, bits = wf->wBitsPerSample, align = wf->nBlockAlign; double sr = wf->nSamplesPerSec;
        EnterCriticalSection(&g_cs); g_tracker.init(sr); g_trackerReady = true; LeaveCriticalSection(&g_cs);
        if (FAILED(ac->Start())) goto done;
        double lastCheck = Now();
        while (g_running) {
            Sleep(10);
            UINT32 pk = 0;
            if (FAILED(cc->GetNextPacketSize(&pk))) break;
            while (pk > 0) {
                BYTE* data; UINT32 n; DWORD flags; UINT64 devPos, qpc;
                if (FAILED(cc->GetBuffer(&data, &n, &flags, &devPos, &qpc))) goto stop;
                mono.assign(n, 0.f);
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    for (UINT32 i = 0; i < n; i++) {
                        const BYTE* fr = data + (size_t)i * align; double sum = 0;
                        for (int c = 0; c < ch; c++) {
                            const BYTE* p = fr + c * (bits / 8); double v = 0;
                            if (isFloat && bits == 32) v = *(const float*)p;
                            else if (bits == 16) v = *(const short*)p / 32768.0;
                            else if (bits == 24) { int x = (p[0] | (p[1] << 8) | (p[2] << 16)); if (x & 0x800000) x |= ~0xFFFFFF; v = x / 8388608.0; }
                            else if (bits == 32) v = *(const int*)p / 2147483648.0;
                            sum += v;
                        }
                        mono[i] = (float)(sum / ch);
                    }
                }
                double t0 = (qpc == 0 || (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)) ? Now() - n / sr : qpc * 1e-7;
                EnterCriticalSection(&g_cs);
                if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) g_tracker.markCut(t0);
                g_tracker.feed(mono.data(), (int)n, t0);
                LeaveCriticalSection(&g_cs);
                cc->ReleaseBuffer(n);
                if (FAILED(cc->GetNextPacketSize(&pk))) goto stop;
            }
            if (Now() - lastCheck > 2.0) { lastCheck = Now(); if (DefaultDeviceId(en) != devId) break; }
        }
    stop:
        ac->Stop();
    }
done:
    if (wf) CoTaskMemFree(wf);
    SafeRelease(cc); SafeRelease(ac); SafeRelease(dev); SafeRelease(en);
}

static DWORD WINAPI AudioThread(LPVOID) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    while (g_running) { RunCapture(); for (int i = 0; i < 10 && g_running; i++) Sleep(100); }
    CoUninitialize();
    return 0;
}

// ---------- animation clock ----------
static DWORD WINAPI RenderThread(LPVOID) {
    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /*HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    int last = -1; double nextBlink = Now() + 3; unsigned rnd = GetTickCount();
    while (g_running) {
        double now = Now(); GuyStatus st;
        EnterCriticalSection(&g_cs);
        bool ready = g_trackerReady;
        if (ready) st = g_tracker.status(now);
        LeaveCriticalSection(&g_cs);
        double wake = now + 0.03; int f = F_SIL_OPEN, y = Y_REST, x = 0;
        switch (st.mode) {
        case MODE_SILENCE: case MODE_AWAKE: {
            bool sil = st.mode == MODE_SILENCE;
            if (now >= nextBlink + 0.14) { rnd = rnd * 1103515245u + 12345u; nextBlink = now + 2.5 + (rnd >> 16) % 4000 / 1000.0; }
            bool blink = now >= nextBlink;
            f = sil ? (blink ? F_SIL_BLINK : F_SIL_OPEN) : (blink ? F_AWAKE_BLINK : F_AWAKE_OPEN);
            wake = blink ? nextBlink + 0.14 : nextBlink; break;
        }
        case MODE_LISTEN: f = F_LISTEN; break;
        case MODE_PAUSE: f = F_PAUSE; break;
        case MODE_NOD: {
            double T = st.period, t = now - (st.gridT0 + g_set.latencyMs / 1000.0);
            double kb = floor(t / T), ph = t - kb * T;
            int inten = g_set.moves ? st.intensity : 1; bool drop = g_set.moves && st.drop, fresh = drop && st.dropFresh;
            if (drop) inten = 2;
            // loud: deeper nod held a bit longer; quiet: nod on every other beat
            bool skip = inten == 0 && ((st.beatIdx + (long long)kb) & 1);
            double down = inten == 2 ? std::min(0.35 * T, 0.13) : std::min(0.3 * T, 0.12);
            f = drop ? F_DROP : inten == 2 ? F_LOUD : F_LISTEN;
            if (g_set.moves && st.build && !drop) {
                // build-up: stops nodding, eyes wide, shivers on the 16ths
                double q = T / 4, kq = floor(t / q);
                f = F_BUILD; y = Y_REST; x = ((long long)kq & 1) ? 1 : 0;
                wake = now + (q - (t - kq * q));
                break;
            }
            // above ~220 BPM a 2-cell nod 4 times a second turns into jitter: keep it to one cell, the face carries the drop
            if (ph < down && !skip) { y = (inten == 2 && T >= 0.27) ? Y_LOUD : Y_NOD; wake = now + (down - ph); }
            else { y = fresh ? Y_JUMP : Y_REST; wake = now + (ph < down ? down - ph : T - ph); }
            break;
        }
        }
        InterlockedExchange(&g_mode, st.mode);
        InterlockedExchange(&g_bpmX10, (LONG)lround(st.bpm * 10));
        int key = (f * 8 + y) * 4 + x + 1;
        if (key != last) { PostMessageW(g_wnd, WM_APP_FRAME, f, MAKELPARAM(y, x + 1)); last = key; }
        double dt = std::min(wake - now, 0.03); if (dt < 0.0005) dt = 0.0005;
        LARGE_INTEGER due; due.QuadPart = -(LONGLONG)(dt * 1e7);
        SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
        WaitForSingleObject(timer, 100);
    }
    CloseHandle(timer);
    return 0;
}

// ---------- autostart ----------
static const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static bool AutoStartOn() {
    HKEY k; bool on = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &k) == ERROR_SUCCESS) { on = RegQueryValueExW(k, L"HeadphonesGuy", NULL, NULL, NULL, NULL) == ERROR_SUCCESS; RegCloseKey(k); }
    return on;
}
static void SetAutoStart(bool on) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t p[MAX_PATH]; GetModuleFileNameW(NULL, p, MAX_PATH);
        std::wstring v = L"\"" + std::wstring(p) + L"\"";
        RegSetValueExW(k, L"HeadphonesGuy", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * 2));
    } else RegDeleteValueW(k, L"HeadphonesGuy");
    RegCloseKey(k);
}
// the exe was moved since autostart was turned on: point the Run entry at where it lives now
static void FixAutoStartPath() { if (AutoStartOn()) SetAutoStart(true); }

// ---------- tray ----------
static void WriteLog() {
    if (!g_set.log || g_iniPath.empty()) return;
    std::wstring lp = g_iniPath.substr(0, g_iniPath.size() - 12) + L"log.txt";
    FILE* f = _wfopen(lp.c_str(), L"a"); if (!f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    double conf, cand; EnterCriticalSection(&g_cs); conf = g_tracker.lastConf; cand = g_tracker.lastCandBpm; LeaveCriticalSection(&g_cs);
    static const char* names[] = {"silence", "awake", "listen", "nod", "pause"};
    fprintf(f, "%02d:%02d:%02d mode=%s bpm=%.1f cand=%.1f conf=%.2f\n", st.wHour, st.wMinute, st.wSecond, names[g_mode % 5], g_bpmX10 / 10.0, cand, conf);
    fclose(f);
}
static void UpdateTip() {
    WriteLog();
    LONG m = g_mode, b = g_bpmX10;
    wchar_t tip[128];
    if (m == MODE_NOD) swprintf(tip, 128, T_(S_TIP_BPM), (long)((b + 5) / 10));
    else if (m == MODE_SILENCE) swprintf(tip, 128, L"%ls", T_(S_TIP_SILENCE));
    else swprintf(tip, 128, L"%ls", T_(S_TIP_LISTEN));
    if (wcscmp(tip, g_nid.szTip)) { wcsncpy(g_nid.szTip, tip, 127); g_nid.uFlags = NIF_TIP; Shell_NotifyIconW(NIM_MODIFY, &g_nid); }
}
static void AddTray() {
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = g_msgWnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_APP_TRAY;
    if (!g_nid.hIcon) g_nid.hIcon = MakeIcon();
    wcsncpy(g_nid.szTip, T_(S_APP), 127);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

enum { CMD_LEFT = 100, CMD_RIGHT, CMD_RESET, CMD_EARLIER, CMD_LATER, CMD_EARLIER50, CMD_LATER50, CMD_AUTOSTART, CMD_MOVES, CMD_COLOR_CUSTOM, CMD_EXIT,
       CMD_COLOR0 = 200 };
struct CupColor { Str name; DWORD rgb; };
static const CupColor kCups[] = { {S_PINK, 0xFF5D73}, {S_RED, 0xE53935}, {S_ORANGE, 0xFF8C1A}, {S_GREEN, 0x3CCB5A},
                                  {S_CYAN, 0x3DB8FF}, {S_BLUE, 0x3D5DFF}, {S_PURPLE, 0x9B5DE5}, {S_WHITE, 0xF2F2F2} };
static const int kCupCount = sizeof(kCups) / sizeof(kCups[0]);

static void RefreshLook() {
    Render();
    HICON old = g_nid.hIcon; g_nid.hIcon = MakeIcon(); g_nid.uFlags = NIF_ICON; Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    if (old) DestroyIcon(old);
}
static bool PickCustomColor() {
    static COLORREF custom[16] = {};
    CHOOSECOLORW cc = {}; cc.lStructSize = sizeof(cc); cc.hwndOwner = g_msgWnd; cc.lpCustColors = custom;
    DWORD c = g_set.cups; cc.rgbResult = RGB((c >> 16) & 255, (c >> 8) & 255, c & 255); cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) return false;
    g_set.cups = ((DWORD)GetRValue(cc.rgbResult) << 16) | ((DWORD)GetGValue(cc.rgbResult) << 8) | GetBValue(cc.rgbResult);
    return true;
}

static void ShowMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, T_(S_APP));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, CMD_LEFT, T_(S_LEFT));
    AppendMenuW(m, MF_STRING, CMD_RIGHT, T_(S_RIGHT));
    AppendMenuW(m, MF_STRING, CMD_RESET, T_(S_RESET));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    wchar_t lat[64]; swprintf(lat, 64, T_(S_OFFSET), g_set.latencyMs);
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, lat);
    AppendMenuW(m, MF_STRING, CMD_EARLIER, T_(S_EARLIER));
    AppendMenuW(m, MF_STRING, CMD_LATER, T_(S_LATER));
    AppendMenuW(m, MF_STRING, CMD_EARLIER50, T_(S_EARLIER50));
    AppendMenuW(m, MF_STRING, CMD_LATER50, T_(S_LATER50));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    HMENU cm = CreatePopupMenu();
    for (int i = 0; i < kCupCount; i++) AppendMenuW(cm, MF_STRING | (g_set.cups == kCups[i].rgb ? MF_CHECKED : 0), CMD_COLOR0 + i, T_(kCups[i].name));
    AppendMenuW(cm, MF_SEPARATOR, 0, NULL);
    AppendMenuW(cm, MF_STRING, CMD_COLOR_CUSTOM, T_(S_CUSTOM));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)cm, T_(S_COLOR));
    AppendMenuW(m, MF_STRING | (g_set.moves ? MF_CHECKED : 0), CMD_MOVES, T_(S_MOVES));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (AutoStartOn() ? MF_CHECKED : 0), CMD_AUTOSTART, T_(S_AUTOSTART));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, CMD_EXIT, T_(S_EXIT));
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(g_msgWnd);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, g_msgWnd, NULL);
    PostMessageW(g_msgWnd, WM_NULL, 0, 0);
    DestroyMenu(m);
    int step = 4 * g_scale;
    switch (cmd) {
    case CMD_LEFT: g_set.offset -= step; break;
    case CMD_RIGHT: g_set.offset += step; break;
    case CMD_RESET: g_set.offset = 0; break;
    case CMD_EARLIER: g_set.latencyMs -= 10; break;
    case CMD_LATER: g_set.latencyMs += 10; break;
    case CMD_EARLIER50: g_set.latencyMs -= 50; break;
    case CMD_LATER50: g_set.latencyMs += 50; break;
    case CMD_MOVES: g_set.moves = !g_set.moves; break;
    case CMD_COLOR_CUSTOM: if (!PickCustomColor()) return; RefreshLook(); break;
    case CMD_AUTOSTART: SetAutoStart(!AutoStartOn()); break;
    case CMD_EXIT: DestroyWindow(g_msgWnd); return;
    default:
        if (cmd >= CMD_COLOR0 && cmd < CMD_COLOR0 + kCupCount) { g_set.cups = kCups[cmd - CMD_COLOR0].rgb; RefreshLook(); break; }
        return;
    }
    g_set.latencyMs = std::max(-500, std::min(500, g_set.latencyMs));
    SaveSettings(); UpdatePlacement();
}

static LRESULT CALLBACK OverlayProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_APP_FRAME) { g_frame = (int)wp; g_yoff = (int)LOWORD(lp); g_xoff = (int)HIWORD(lp) - 1; Render(); return 0; }
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(h, msg, wp, lp);
}

static LRESULT CALLBACK MsgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER:
        if (wp == TIMER_POS) UpdatePlacement();
        else if (wp == TIMER_TIP) UpdateTip();
        return 0;
    case WM_APP_TRAY:
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) ShowMenu();
        return 0;
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: case WM_DPICHANGED:
        UpdatePlacement(); return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0); return 0;
    }
    if (msg == WM_TASKBARCREATED && WM_TASKBARCREATED) { AddTray(); UpdatePlacement(); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

static void SetDpiAware() {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    typedef BOOL(WINAPI * Fn)(HANDLE);
    Fn f = u ? (Fn)GetProcAddress(u, "SetProcessDpiAwarenessContext") : NULL;
    if (!f || !f((HANDLE)-4 /*PER_MONITOR_AWARE_V2*/)) SetProcessDPIAware();
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    HANDLE mtx = CreateMutexW(NULL, TRUE, L"Local\\HeadphonesGuyOneInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    g_inst = inst;
    SetDpiAware();
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpcFreq = (double)f.QuadPart;
    InitializeCriticalSection(&g_cs);
    g_ru = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
    LoadSettings();
    g_tracker.sharpOnsets = g_set.sharp;
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    FixAutoStartPath();

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = OverlayProc; wc.hInstance = inst; wc.lpszClassName = L"HeadphonesGuyOverlay"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);
    WNDCLASSEXW wm = {sizeof(wm)};
    wm.lpfnWndProc = MsgProc; wm.hInstance = inst; wm.lpszClassName = L"HeadphonesGuyTray";
    RegisterClassExW(&wm);

    g_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                            wc.lpszClassName, T_(S_APP), WS_POPUP, 0, 0, 32, 36, NULL, NULL, inst, NULL);
    g_msgWnd = CreateWindowExW(WS_EX_TOOLWINDOW, wm.lpszClassName, T_(S_APP), WS_POPUP, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    if (!g_wnd || !g_msgWnd) return 1;

    AddTray();
    Render();
    UpdatePlacement();
    SetTimer(g_msgWnd, TIMER_POS, 300, NULL);
    SetTimer(g_msgWnd, TIMER_TIP, 1000, NULL);

    HANDLE ta = CreateThread(NULL, 0, AudioThread, NULL, 0, NULL);
    HANDLE tr = CreateThread(NULL, 0, RenderThread, NULL, 0, NULL);
    SetThreadPriority(tr, THREAD_PRIORITY_ABOVE_NORMAL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    InterlockedExchange(&g_running, 0);
    HANDLE th[2] = {ta, tr};
    WaitForMultipleObjects(2, th, TRUE, 2000);
    DestroyWindow(g_wnd);
    if (g_nid.hIcon) DestroyIcon(g_nid.hIcon);
    if (mtx) CloseHandle(mtx);
    return 0;
}
