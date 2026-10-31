// Headphones Guy - a pixel smiley on the Windows taskbar that nods to whatever is playing.
// Win32 + WASAPI loopback, no dependencies. Build: see build.sh / README.md
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include <winsock2.h>
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
#include "sprites.h"
#include "animator.h"
#include "stats.h"
#include "openrgb.h"
#include "applisten.h"

// ---------- UI text (English, or Russian on a Russian Windows; override with language=en|ru in settings.ini) ----------
enum Str { S_APP, S_TIP_BPM, S_TIP_SILENCE, S_TIP_LISTEN, S_LEFT, S_RIGHT, S_RESET, S_OFFSET, S_EARLIER, S_LATER, S_EARLIER50, S_LATER50,
           S_COLOR, S_CUSTOM, S_MOVES, S_AUTOSTART, S_EXIT,
           S_PINK, S_RED, S_ORANGE, S_GREEN, S_CYAN, S_BLUE, S_PURPLE, S_WHITE,
           S_SKIN, S_OPEN_SKINS, S_HATS, S_HAT_AUTO, S_HAT_OFF, S_HAT_WITCH, S_HAT_SANTA, S_HAT_PARTY, S_EGGS, S_TRAYANIM,
           S_APPS, S_APPS_NONE, S_APPS_OLD, S_APPS_HINT, S_STREAM, S_STREAM_TITLE, S_RGB, S_RGB_NOSERVER, S_RGB_OK, S_STATS, S_STATS_FAIL,
           S_CARD_LISTENED, S_CARD_HOURS, S_CARD_AVG, S_CARD_DROPS, S_CARD_RECORD, S_CARD_FOOTER,
           S_DRAW, S_SKIN_OK, S_SKIN_BAD, S_COUNT };
static const wchar_t* kEn[S_COUNT] = {
    L"Headphones Guy", L"Headphones Guy · %ld BPM", L"Headphones Guy · silence", L"Headphones Guy · listening",
    L"Move left", L"Move right", L"Reset position", L"Nod offset: %d ms", L"Nod earlier (−10 ms)", L"Nod later (+10 ms)",
    L"Much earlier (−50 ms)", L"Much later (+50 ms, Bluetooth)",
    L"Headphone color", L"Custom color...", L"Go harder on loud parts and drops", L"Start with Windows", L"Exit",
    L"Pink", L"Red", L"Orange", L"Green", L"Cyan", L"Blue", L"Purple", L"White",
    L"Skin", L"Open skins folder...", L"Hat", L"By the date", L"No hat", L"Witch hat", L"Santa hat", L"Party hat",
    L"Tempo easter eggs (sweat, smoke, sleep)", L"Nodding tray icon",
    L"Guys for single apps", L"Nothing is playing right now", L"Needs Windows 10 2004 or newer", L"Pick up to 3 apps playing sound now:",
    L"Stream window (for OBS)", L"Headphones Guy · stream",
    L"Sync RGB lighting (OpenRGB)", L"OpenRGB not found: start it and turn on SDK Server", L"OpenRGB: %d device(s)",
    L"Monthly stats card...", L"Couldn't save the stats card",
    L"listened", L"h", L"average tempo", L"drops survived", L"top steady tempo", L"headphones guy · github.com/kondrati0/headphones-guy",
    L"Draw a skin...", L"Skin installed: %ls", L"That link doesn't contain a skin" };
static const wchar_t* kRu[S_COUNT] = {
    L"Чел в наушниках", L"Чел в наушниках · %ld BPM", L"Чел в наушниках · тишина", L"Чел в наушниках · слушает",
    L"Сдвинуть левее", L"Сдвинуть правее", L"Вернуть на место", L"Сдвиг кивка: %d мс", L"Кивать раньше (−10 мс)", L"Кивать позже (+10 мс)",
    L"Сильно раньше (−50 мс)", L"Сильно позже (+50 мс, блютуз)",
    L"Цвет наушников", L"Свой цвет...", L"Качает сильнее на громком и на дропах", L"Запускать вместе с Windows", L"Выход",
    L"Розовые", L"Красные", L"Оранжевые", L"Зелёные", L"Голубые", L"Синие", L"Фиолетовые", L"Белые",
    L"Скин", L"Открыть папку со скинами...", L"Шапка", L"По дате", L"Без шапки", L"Шляпа ведьмы", L"Шапка Санты", L"Праздничный колпак",
    L"Пасхалки от темпа (пот, дым, сон)", L"Иконка в трее тоже кивает",
    L"Челы для отдельных приложений", L"Сейчас ничего не играет", L"Нужна Windows 10 2004 или новее", L"До 3 приложений, где сейчас есть звук:",
    L"Окно для стрима (OBS)", L"Чел в наушниках · стрим",
    L"Подсветка в бит (OpenRGB)", L"OpenRGB не найден: запусти его и включи SDK Server", L"OpenRGB: устройств %d",
    L"Сводка за месяц...", L"Не получилось сохранить сводку",
    L"наслушал", L"ч", L"средний темп", L"дропов пережил", L"рекорд темпа", L"чел в наушниках · github.com/kondrati0/headphones-guy",
    L"Нарисовать скин...", L"Скин установлен: %ls", L"В этой ссылке нет скина" };
static const wchar_t* kMonthEn[12] = {L"January", L"February", L"March", L"April", L"May", L"June", L"July", L"August", L"September", L"October", L"November", L"December"};
static const wchar_t* kMonthRu[12] = {L"Январь", L"Февраль", L"Март", L"Апрель", L"Май", L"Июнь", L"Июль", L"Август", L"Сентябрь", L"Октябрь", L"Ноябрь", L"Декабрь"};
static bool g_ru = false;
static const wchar_t* T_(Str id) { return (g_ru && kRu[id]) ? kRu[id] : kEn[id]; }

// ---------- settings ----------
struct Settings {
    int offset = 0; int latencyMs = 10; DWORD cups = 0xFF5D73; bool log = false; bool sharp = true; bool moves = true;
    std::string skin = "classic"; int hat = -1;          // -1 = by the date
    bool eggs = true, trayAnim = false, stream = false, streamBpm = true, rgb = false;
    RECT streamRect = {100, 100, 100 + 360, 100 + 440}; DWORD streamBg = 0x00B140;
    std::vector<std::wstring> apps;                      // exe names with their own guy
} g_set;
static std::wstring g_dir, g_iniPath;
static const char* kHatKeys[] = {"off", "witch", "santa", "party"};

static std::wstring Widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }
static std::string Narrow(const wchar_t* w) { std::string r; for (; *w; w++) r += (char)(*w < 128 ? *w : '_'); return r; }
static DWORD ReadHex(const wchar_t* sec, const wchar_t* key, DWORD def, const wchar_t* f) {
    wchar_t b[32]; GetPrivateProfileStringW(sec, key, L"", b, 32, f);
    return b[0] ? (DWORD)(wcstoul(b[0] == L'#' ? b + 1 : b, NULL, 16) & 0xFFFFFF) : def;
}
static void LoadSettings() {
    wchar_t p[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, p))) {
        g_dir = std::wstring(p) + L"\\HeadphonesGuy";
        CreateDirectoryW(g_dir.c_str(), NULL);
        g_iniPath = g_dir + L"\\settings.ini";
    }
    if (g_iniPath.empty()) return;
    const wchar_t* f = g_iniPath.c_str();
    g_set.offset = (int)GetPrivateProfileIntW(L"guy", L"offset", 0, f);
    g_set.latencyMs = (int)GetPrivateProfileIntW(L"guy", L"nod_offset_ms", 10, f);
    g_set.log = GetPrivateProfileIntW(L"guy", L"log", 0, f) != 0;
    g_set.sharp = GetPrivateProfileIntW(L"guy", L"sharp_onsets", 1, f) != 0;   // 0 = old 2048-window onset timing
    g_set.moves = GetPrivateProfileIntW(L"guy", L"moves", 1, f) != 0;          // 0 = plain nods, no drop face / loudness
    wchar_t lang[16]; GetPrivateProfileStringW(L"guy", L"language", L"auto", lang, 16, f);
    if (!wcscmp(lang, L"en")) g_ru = false; else if (!wcscmp(lang, L"ru")) g_ru = true;
    g_set.cups = ReadHex(L"guy", L"cups_color", 0xFF5D73, f);
    wchar_t b[512];
    GetPrivateProfileStringW(L"guy", L"skin", L"classic", b, 512, f); g_set.skin = Narrow(b);
    GetPrivateProfileStringW(L"guy", L"hat", L"auto", b, 512, f);
    g_set.hat = -1; for (int i = 0; i < 4; i++) if (Widen(kHatKeys[i]) == b) g_set.hat = i;
    g_set.eggs = GetPrivateProfileIntW(L"guy", L"easter_eggs", 1, f) != 0;
    g_set.trayAnim = GetPrivateProfileIntW(L"guy", L"tray_nod", 0, f) != 0;
    g_set.rgb = GetPrivateProfileIntW(L"guy", L"openrgb", 0, f) != 0;
    g_set.stream = GetPrivateProfileIntW(L"stream", L"open", 0, f) != 0;
    g_set.streamBpm = GetPrivateProfileIntW(L"stream", L"show_bpm", 1, f) != 0;
    g_set.streamBg = ReadHex(L"stream", L"background", 0x00B140, f);
    RECT& r = g_set.streamRect;
    r.left = (int)GetPrivateProfileIntW(L"stream", L"x", r.left, f); r.top = (int)GetPrivateProfileIntW(L"stream", L"y", r.top, f);
    r.right = r.left + (int)GetPrivateProfileIntW(L"stream", L"w", 360, f); r.bottom = r.top + (int)GetPrivateProfileIntW(L"stream", L"h", 440, f);
    GetPrivateProfileStringW(L"guy", L"apps", L"", b, 512, f);
    std::wstring list = b; size_t a0 = 0;
    while (a0 < list.size() && g_set.apps.size() < 3) {
        size_t a1 = list.find(L';', a0); if (a1 == std::wstring::npos) a1 = list.size();
        if (a1 > a0) g_set.apps.push_back(applisten::Lower(list.substr(a0, a1 - a0)));
        a0 = a1 + 1;
    }
}
static void SaveSettings() {
    if (g_iniPath.empty()) return;
    const wchar_t* f = g_iniPath.c_str(); wchar_t b[64];
    auto put = [&](const wchar_t* sec, const wchar_t* k, int v) { swprintf(b, 64, L"%d", v); WritePrivateProfileStringW(sec, k, b, f); };
    put(L"guy", L"offset", g_set.offset); put(L"guy", L"nod_offset_ms", g_set.latencyMs);
    swprintf(b, 64, L"%06X", (unsigned)g_set.cups); WritePrivateProfileStringW(L"guy", L"cups_color", b, f);
    put(L"guy", L"moves", g_set.moves); put(L"guy", L"easter_eggs", g_set.eggs); put(L"guy", L"tray_nod", g_set.trayAnim); put(L"guy", L"openrgb", g_set.rgb);
    WritePrivateProfileStringW(L"guy", L"skin", Widen(g_set.skin).c_str(), f);
    WritePrivateProfileStringW(L"guy", L"hat", g_set.hat < 0 ? L"auto" : Widen(kHatKeys[g_set.hat]).c_str(), f);
    std::wstring apps; for (auto& a : g_set.apps) { if (!apps.empty()) apps += L";"; apps += a; }
    WritePrivateProfileStringW(L"guy", L"apps", apps.c_str(), f);
    put(L"stream", L"open", g_set.stream); put(L"stream", L"show_bpm", g_set.streamBpm);
    swprintf(b, 64, L"%06X", (unsigned)g_set.streamBg); WritePrivateProfileStringW(L"stream", L"background", b, f);
    RECT& r = g_set.streamRect;
    put(L"stream", L"x", r.left); put(L"stream", L"y", r.top); put(L"stream", L"w", r.right - r.left); put(L"stream", L"h", r.bottom - r.top);
}

// ---------- skins ----------
static std::vector<Skin> g_skins;
static size_t g_skinIdx = 0;
static std::wstring SkinsDir() { return g_dir + L"\\skins"; }
// custom skin: an .ini with [skin] colors, [overlay] accessories and optional [frames], see docs/SKINS.md
static bool ReadFileUtf8(const std::wstring& path, std::string& out, size_t cap = 1 << 16) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz; bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart <= (LONGLONG)cap;
    if (ok) { out.resize((size_t)sz.QuadPart); DWORD rd = 0; ok = ReadFile(h, &out[0], (DWORD)out.size(), &rd, NULL) && rd == out.size(); }
    CloseHandle(h); return ok;
}
static bool LoadSkinFile(const std::wstring& path, const std::wstring& stem, Skin& s) {
    std::string text; if (!ReadFileUtf8(path, text)) return false;
    s.id = "custom:" + Narrow(stem.c_str());
    if (!ParseSkinIni(text, s)) return false;
    if (s.name == "My skin") { std::string n; for (wchar_t w : stem) { char u[8]; int k = WideCharToMultiByte(CP_UTF8, 0, &w, 1, u, 8, NULL, NULL); n.append(u, k); } s.name = n; }
    return true;
}
static void LoadSkins() {
    g_skins = BuiltinSkins();
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((SkinsDir() + L"\\*.ini").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring nm = fd.cFileName, stem = nm.substr(0, nm.size() - 4); Skin s;
            if (LoadSkinFile(SkinsDir() + L"\\" + nm, stem, s)) g_skins.push_back(s);
        } while (FindNextFileW(h, &fd) && g_skins.size() < 60);
        FindClose(h);
    }
    g_skinIdx = 0; for (size_t i = 0; i < g_skins.size(); i++) if (g_skins[i].id == g_set.skin) g_skinIdx = i;
}
static const Skin& CurSkin() { return g_skins[g_skinIdx < g_skins.size() ? g_skinIdx : 0]; }
static std::wstring SkinLabel(const Skin& s) {
    if (g_ru && SkinNameRu(s.id)) { std::string u = SkinNameRu(s.id); wchar_t w[64]; MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, w, 64); return w; }
    wchar_t w[128]; MultiByteToWideChar(CP_UTF8, 0, s.name.c_str(), -1, w, 128); return w;
}
static const char kExampleSkin[] =
    "; Headphones Guy skin. Copy this file, rename it, change the colors, then pick it in the tray menu (Skin).\r\n"
    "; Colors are RRGGBB. Anything you leave out stays like the classic guy. Full guide: docs/SKINS.md in the repo.\r\n"
    "[skin]\r\nname=Example: night shift\r\nface=7FA8FF\r\noutline=1B2440\r\neyes=1B2440\r\nheadband=C8CCD6\r\ncups=FFC83D\r\n"
    "; extra colors for your own frames: 1=RRGGBB ... 9=RRGGBB\r\n\r\n"
    "; [frames] replaces whole 16x16 frames: listen.0= ... listen.15= (16 characters each).\r\n"
    "; Frames: idle idle_blink awake awake_blink listen loud pause build drop talk\r\n"
    "; Letters: k outline, e eyes, y face, B headband, C D L headphone cup, t tongue, 1-9 your colors, . empty\r\n"
    "[frames]\r\n";
static void OpenSkinsFolder() {
    std::wstring d = SkinsDir(); CreateDirectoryW(d.c_str(), NULL);
    std::wstring ex = d + L"\\example.ini";
    if (GetFileAttributesW(ex.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE h = CreateFileW(ex.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, kExampleSkin, sizeof(kExampleSkin) - 1, &w, NULL); CloseHandle(h); }
    }
    ShellExecuteW(NULL, L"open", d.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// ---------- globals ----------
static HINSTANCE g_inst;
static HWND g_msgWnd, g_streamWnd;
static volatile LONG g_running = 1;
static CRITICAL_SECTION g_cs;
static BeatTracker g_tracker;
static bool g_trackerReady = false;
static double g_qpcFreq = 1;
static int g_scale = 2;
static UINT WM_TASKBARCREATED = 0;
static NOTIFYICONDATAW g_nid = {};
static volatile LONG g_mode = MODE_SILENCE, g_bpmX10 = 0, g_drop = 0;
static StatsKeeper g_stats;

#define WM_APP_FRAME (WM_APP + 1)
#define WM_APP_TRAY  (WM_APP + 2)
#define TIMER_POS 1
#define TIMER_TIP 2

static double Now() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart / g_qpcFreq; }

// ---------- a guy window (main one + one per app) ----------
struct GuyWin {
    HWND wnd = NULL; HDC dc = NULL; HBITMAP bmp = NULL; uint32_t* bits = NULL; int bmpS = 0;
    Pose pose; int x = -1000, y = -1000; bool visible = false; DWORD cups = 0xFF5D73; bool main = false;
};
static GuyWin g_main;

static WPARAM PackPose(const Pose& p) { return p.frame | (p.y & 15) << 4 | (p.x & 3) << 8 | (p.fx & 7) << 10 | (p.phase & 7) << 13 | (p.hat & 3) << 16; }
static Pose UnpackPose(WPARAM w) { Pose p; p.frame = w & 15; p.y = (w >> 4) & 15; p.x = (w >> 8) & 3; p.fx = (w >> 10) & 7; p.phase = (w >> 13) & 7; p.hat = (w >> 16) & 3; return p; }

static void RenderGuy(GuyWin& g) {
    int s = g_scale, W = BOX_W * s, H = BOX_H * s;
    HDC screen = GetDC(NULL);
    if (g.bmpS != s) {
        if (g.bmp) DeleteObject(g.bmp);
        if (!g.dc) g.dc = CreateCompatibleDC(screen);
        BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        g.bmp = CreateDIBSection(g.dc, &bi, DIB_RGB_COLORS, (void**)&g.bits, NULL, 0);
        SelectObject(g.dc, g.bmp);
        g.bmpS = s;
    }
    memset(g.bits, 0, (size_t)W * H * 4);
    DrawGuy(g.bits, W, s, BOX_H, g.pose, CurSkin(), g.main ? g_set.cups : g.cups);
    GdiFlush();
    POINT src = {0, 0}, dst = {g.x, g.y}; SIZE sz = {W, H};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(g.wnd, screen, &dst, &sz, g.dc, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, screen);
}

// tray icon: the guy at rest or in a nod
static HICON g_icon[2] = {};
static HICON MakeIcon(int y) {
    const int S = 32;
    BITMAPV5HEADER bh = {}; bh.bV5Size = sizeof(bh); bh.bV5Width = S; bh.bV5Height = -S; bh.bV5Planes = 1; bh.bV5BitCount = 32;
    bh.bV5Compression = BI_BITFIELDS; bh.bV5RedMask = 0x00FF0000; bh.bV5GreenMask = 0x0000FF00; bh.bV5BlueMask = 0x000000FF; bh.bV5AlphaMask = 0xFF000000;
    HDC dc = GetDC(NULL); uint32_t* bits = NULL;
    HBITMAP col = CreateDIBSection(dc, (BITMAPINFO*)&bh, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
    ReleaseDC(NULL, dc);
    memset(bits, 0, S * S * 4);
    Pose p; p.frame = F_LISTEN; p.y = y;
    DrawGuy(bits, S, 2, 16, p, CurSkin(), g_set.cups);
    HBITMAP mask = CreateBitmap(S, S, 1, 1, NULL);
    ICONINFO ii = {}; ii.fIcon = TRUE; ii.hbmColor = col; ii.hbmMask = mask;
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(col); DeleteObject(mask);
    return ic;
}
static int g_iconShown = 0;
static void SetTrayIcon(int which) {
    g_iconShown = which; g_nid.hIcon = g_icon[which]; g_nid.uFlags = NIF_ICON; Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static void MakeIcons() {
    for (int i = 0; i < 2; i++) { if (g_icon[i]) DestroyIcon(g_icon[i]); g_icon[i] = MakeIcon(i ? 2 : 1); }
    g_nid.hIcon = g_icon[0]; g_iconShown = 0;
}

// ---------- per-app guys ----------
static const DWORD kAppCups[3] = {0x3CCB5A, 0x5865F2, 0xFF8C1A};
struct AppSlot {
    std::wstring exe; bool voice = false;
    BeatTracker tracker; bool ready = false; double level = 0; CRITICAL_SECTION cs;
    volatile LONG run = 0, present = 0, active = 0; HANDLE thread = NULL;   // active: exe/voice are set, the render thread may use the slot
    Animator anim; GuyWin win; Pose last;
};
static AppSlot g_apps[3];
static bool g_appsSupported = false;

static DWORD WINAPI AppThread(LPVOID param) {
    AppSlot& sl = *(AppSlot*)param;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    std::vector<float> mono;
    while (sl.run) {
        DWORD pid = applisten::RootPid(sl.exe);
        if (!pid) { InterlockedExchange(&sl.present, 0); for (int i = 0; i < 20 && sl.run; i++) Sleep(100); continue; }
        InterlockedExchange(&sl.present, 1);
        HANDLE hp = OpenProcess(SYNCHRONIZE, FALSE, pid);
        WAVEFORMATEX wf; IAudioClient* ac = applisten::Open(pid, &wf);
        IAudioCaptureClient* cc = NULL; HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (ac && SUCCEEDED(ac->SetEventHandle(ev)) && SUCCEEDED(ac->GetService(__uuidof(IAudioCaptureClient), (void**)&cc)) && SUCCEEDED(ac->Start())) {
            EnterCriticalSection(&sl.cs); sl.tracker.init(wf.nSamplesPerSec); sl.ready = true; LeaveCriticalSection(&sl.cs);
            while (sl.run && (!hp || WaitForSingleObject(hp, 0) == WAIT_TIMEOUT)) {
                WaitForSingleObject(ev, 200);
                UINT32 pk = 0; bool bad = FAILED(cc->GetNextPacketSize(&pk));
                while (!bad && pk > 0) {
                    BYTE* data; UINT32 n; DWORD flags; UINT64 devPos, qpc;
                    if (FAILED(cc->GetBuffer(&data, &n, &flags, &devPos, &qpc))) { bad = true; break; }
                    mono.assign(n, 0.f); double e = 0;
                    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                        const float* f = (const float*)data;
                        for (UINT32 i = 0; i < n; i++) { mono[i] = 0.5f * (f[2 * i] + f[2 * i + 1]); e += mono[i] * mono[i]; }
                    }
                    double t0 = (qpc == 0 || (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)) ? Now() - n / 48000.0 : qpc * 1e-7;
                    EnterCriticalSection(&sl.cs);
                    sl.level = n ? sqrt(e / n) : 0;
                    sl.tracker.feed(mono.data(), (int)n, t0);
                    LeaveCriticalSection(&sl.cs);
                    cc->ReleaseBuffer(n);
                    bad = FAILED(cc->GetNextPacketSize(&pk));
                }
                if (bad) break;
            }
            ac->Stop();
        } else for (int i = 0; i < 30 && sl.run; i++) Sleep(100);   // could not open it: try again in 3 s
        EnterCriticalSection(&sl.cs); sl.ready = false; sl.level = 0; LeaveCriticalSection(&sl.cs);
        if (cc) cc->Release();
        if (ac) ac->Release();
        CloseHandle(ev);
        if (hp) CloseHandle(hp);
    }
    InterlockedExchange(&sl.present, 0);
    CoUninitialize();
    return 0;
}
static void ApplyApps() {
    for (int i = 0; i < 3; i++) {
        AppSlot& sl = g_apps[i];
        std::wstring want = (g_appsSupported && i < (int)g_set.apps.size()) ? g_set.apps[i] : L"";
        if (sl.exe == want) continue;
        InterlockedExchange(&sl.active, 0);
        if (sl.thread) { InterlockedExchange(&sl.run, 0); WaitForSingleObject(sl.thread, 10000); CloseHandle(sl.thread); sl.thread = NULL; }
        sl.exe = want; sl.voice = applisten::IsVoiceApp(want); InterlockedExchange(&sl.present, 0);
        if (!want.empty()) { InterlockedExchange(&sl.run, 1); sl.thread = CreateThread(NULL, 0, AppThread, &sl, 0, NULL); InterlockedExchange(&sl.active, 1); }
    }
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
static void Place(GuyWin& g, bool show, int x, int y, bool rescaled) {
    bool redraw = rescaled || x != g.x || y != g.y;
    g.x = x; g.y = y;
    if (redraw && show) RenderGuy(g);
    if (show != g.visible) { if (show) RenderGuy(g); ShowWindow(g.wnd, show ? SW_SHOWNOACTIVATE : SW_HIDE); g.visible = show; }
    if (show) SetWindowPos(g.wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}
static void UpdatePlacement() {
    HWND tb = FindWindowW(L"Shell_TrayWnd", NULL);
    bool show = false, horiz = true; int s = g_scale, x = g_main.x, y = g_main.y;
    if (tb) {
        RECT tr; GetWindowRect(tb, &tr);
        HMONITOR mon = MonitorFromWindow(tb, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
        RECT vis; IntersectRect(&vis, &tr, &mi.rcMonitor);
        int tw = tr.right - tr.left, th = tr.bottom - tr.top;
        horiz = tw >= th;
        int visThick = horiz ? (vis.bottom - vis.top) : (vis.right - vis.left);
        int thick = horiz ? th : tw;
        RECT nr = tr; bool haveNotify = false;
        HWND tn = FindWindowExW(tb, NULL, L"TrayNotifyWnd", NULL);
        if (tn) { GetWindowRect(tn, &nr); haveNotify = true; }
        s = std::max(1, (thick - 4) / 16);
        int W = BOX_W * s, H = BOX_H * s;
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
    bool rescaled = s != g_scale; g_scale = s;
    Place(g_main, show, x, y, rescaled);
    // app guys line up to the left of (or above) the main one
    int k = 0, step = BOX_W * s + 2 * s, stepV = BOX_H * s;
    for (auto& sl : g_apps) {
        bool on = show && !sl.exe.empty() && sl.present;
        if (on) k++;
        Place(sl.win, on, horiz ? x - k * step : x, horiz ? y : y - k * stepV, rescaled);
    }
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
static AnimOptions CurOptions() {
    AnimOptions o; o.latency = g_set.latencyMs / 1000.0; o.moves = g_set.moves; o.eggs = g_set.eggs;
    SYSTEMTIME t; GetLocalTime(&t);
    o.hat = g_set.hat < 0 ? HatForDate(t.wMonth, t.wDay) : g_set.hat == 0 ? HAT_NONE : g_set.hat;
    o.night = t.wHour < 6;
    return o;
}
static DWORD WINAPI RenderThread(LPVOID) {
    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /*HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    Animator anim; Pose last; bool first = true; AnimOptions o; double optT = -1;
    while (g_running) {
        double now = Now(); GuyStatus st;
        if (now - optT > 1.0) { o = CurOptions(); optT = now; }
        EnterCriticalSection(&g_cs);
        bool ready = g_trackerReady;
        if (ready) st = g_tracker.status(now);
        LeaveCriticalSection(&g_cs);
        double wake; Pose p = anim.step(st, now, o, &wake);
        InterlockedExchange(&g_mode, st.mode);
        InterlockedExchange(&g_bpmX10, (LONG)lround(st.bpm * 10));
        InterlockedExchange(&g_drop, st.mode == MODE_NOD && st.drop);
        if (first || p != last) { PostMessageW(g_main.wnd, WM_APP_FRAME, PackPose(p), 0); last = p; first = false; }
        for (auto& sl : g_apps) {
            if (!sl.active || !sl.present) continue;
            GuyStatus as; bool ar; double lvl;
            EnterCriticalSection(&sl.cs); ar = sl.ready; if (ar) as = sl.tracker.status(now); lvl = sl.level; LeaveCriticalSection(&sl.cs);
            double w2; Pose ap = sl.voice ? sl.anim.talk(lvl, now, o, &w2) : sl.anim.step(as, now, o, &w2);
            if (!sl.voice && !ar) ap.frame = F_SIL_OPEN;
            wake = std::min(wake, w2);
            if (ap != sl.last) { PostMessageW(sl.win.wnd, WM_APP_FRAME, PackPose(ap), 0); sl.last = ap; }
        }
        double dt = std::min(wake - now, 0.03); if (dt < 0.0005) dt = 0.0005;
        LARGE_INTEGER due; due.QuadPart = -(LONGLONG)(dt * 1e7);
        SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
        WaitForSingleObject(timer, 100);
    }
    CloseHandle(timer);
    return 0;
}

// ---------- RGB: the headphone color flashing on each beat ----------
static uint32_t RgbNow() {
    double now = Now(); GuyStatus st; bool ready;
    EnterCriticalSection(&g_cs); ready = g_trackerReady; if (ready) st = g_tracker.status(now); LeaveCriticalSection(&g_cs);
    double b = 0.1;
    if (ready && st.mode == MODE_NOD) {
        double T = st.period, t = now - (st.gridT0 + g_set.latencyMs / 1000.0), ph = t - floor(t / T) * T;
        b = 0.1 + 0.9 * exp(-ph / (0.22 * T));
        if (st.build) b = ((long long)floor(t / (T / 4)) & 1) ? 0.15 : 1.0;   // strobe on the 16ths during a build-up
        if (st.drop) b = std::max(b, 0.35);
    }
    DWORD c = g_set.cups; int r = (int)(((c >> 16) & 255) * b), g = (int)(((c >> 8) & 255) * b), bl = (int)((c & 255) * b);
    return (uint32_t)(r << 16 | g << 8 | bl);
}

// ---------- stream window: a big guy on a chroma-key background for OBS window capture ----------
static void PaintStream(HWND h) {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc); int W = std::max(1L, rc.right), H = std::max(1L, rc.bottom);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* px = NULL; HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void**)&px, NULL, 0);
    HGDIOBJ old = SelectObject(mem, bmp);
    uint32_t bg = Argb(g_set.streamBg); for (int i = 0; i < W * H; i++) px[i] = bg;
    int textH = g_set.streamBpm ? H / 7 : 0;
    int s = std::max(1, std::min(W / BOX_W, (H - textH) / BOX_H));
    int ox = (W - BOX_W * s) / 2, oy = std::max(0, (H - textH - BOX_H * s) / 2);
    if (ox >= 0) DrawGuy(px + (size_t)oy * W + ox, W, s, BOX_H, g_main.pose, CurSkin(), g_set.cups);
    GdiFlush();
    if (textH > 0 && g_mode == MODE_NOD) {
        wchar_t t[32]; swprintf(t, 32, L"%ld BPM", (long)((g_bpmX10 + 5) / 10));
        HFONT f = CreateFontW(-textH * 7 / 10, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
        HGDIOBJ of = SelectObject(mem, f); SetBkMode(mem, TRANSPARENT);
        RECT tr = {0, H - textH - textH / 4, W, H - textH / 4};
        SetTextColor(mem, RGB(0x1C, 0x1C, 0x1E)); OffsetRect(&tr, 2, 2); DrawTextW(mem, t, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(mem, RGB(255, 255, 255)); OffsetRect(&tr, -2, -2); DrawTextW(mem, t, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(mem, of); DeleteObject(f);
    }
    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
    EndPaint(h, &ps);
}
static void SaveStreamRect() { if (g_streamWnd && !IsIconic(g_streamWnd)) { GetWindowRect(g_streamWnd, &g_set.streamRect); SaveSettings(); } }
static void ShowStream(bool on) {
    if (on && !g_streamWnd) {
        RECT r = g_set.streamRect;
        g_streamWnd = CreateWindowExW(0, L"HeadphonesGuyStream", T_(S_STREAM_TITLE), WS_OVERLAPPEDWINDOW, r.left, r.top, r.right - r.left, r.bottom - r.top,
                                      NULL, NULL, g_inst, NULL);
        // make sure it lands on a monitor
        HMONITOR m = MonitorFromRect(&r, MONITOR_DEFAULTTONULL);
        if (!m) SetWindowPos(g_streamWnd, NULL, 100, 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    if (g_streamWnd) ShowWindow(g_streamWnd, on ? SW_SHOWNOACTIVATE : SW_HIDE);
}
static void ShowMenu();
static LRESULT CALLBACK StreamProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: PaintStream(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_EXITSIZEMOVE: SaveStreamRect(); return 0;
    case WM_RBUTTONUP: ShowMenu(); return 0;
    case WM_CLOSE: SaveStreamRect(); g_set.stream = false; SaveSettings(); ShowWindow(h, SW_HIDE); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
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

// ---------- headphonesguy:// links: the web skin editor sends skins straight into the app ----------
static const wchar_t* kEditorUrl = L"https://kondrati0.github.io/headphones-guy/editor.html";
static const ULONG_PTR kCopyLink = 0x4847;   // WM_COPYDATA tag: a headphonesguy:// link
static void RegisterProtocol() {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH);
    std::wstring cmd = L"\"" + std::wstring(exe) + L"\" \"%1\"", icon = L"\"" + std::wstring(exe) + L"\",0";
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\headphonesguy", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) != ERROR_SUCCESS) return;
    const wchar_t* desc = L"URL:Headphones Guy";
    RegSetValueExW(k, NULL, 0, REG_SZ, (const BYTE*)desc, (DWORD)((wcslen(desc) + 1) * 2));
    RegSetValueExW(k, L"URL Protocol", 0, REG_SZ, (const BYTE*)L"", 2);
    RegCloseKey(k);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\headphonesguy\\DefaultIcon", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, NULL, 0, REG_SZ, (const BYTE*)icon.c_str(), (DWORD)((icon.size() + 1) * 2)); RegCloseKey(k);
    }
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\headphonesguy\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, NULL, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * 2)); RegCloseKey(k);
    }
}
static void Notify(const wchar_t* text) {
    g_nid.uFlags = NIF_INFO; g_nid.dwInfoFlags = NIIF_INFO;
    wcsncpy(g_nid.szInfoTitle, T_(S_APP), 63); g_nid.szInfoTitle[63] = 0;
    wcsncpy(g_nid.szInfo, text, 255); g_nid.szInfo[255] = 0;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static void RefreshLook();
// headphonesguy://install?skin=<base64url of the skin ini>
static void InstallFromLink(const std::wstring& link) {
    size_t p = link.find(L"skin=");
    std::string b64;
    if (p != std::wstring::npos) for (size_t i = p + 5; i < link.size() && link[i] != L'&' && link[i] != L'"' && link[i] != L'/' ; i++) b64 += (char)link[i];
    std::string text; Skin s;
    if (b64.empty() || b64.size() > 200000 || !Base64Decode(b64, text) || !ParseSkinIni(text, s)) { Notify(T_(S_SKIN_BAD)); return; }
    std::wstring stem;
    for (char c : s.name) if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') stem += (wchar_t)c; else if (c == ' ' && !stem.empty() && stem.back() != L'-') stem += L'-';
    while (!stem.empty() && stem.back() == L'-') stem.pop_back();
    if (stem.empty()) stem = L"skin";
    stem = stem.substr(0, 40);
    std::wstring dir = SkinsDir(); CreateDirectoryW(dir.c_str(), NULL);
    std::wstring name = stem; std::string old;
    for (int n = 2; GetFileAttributesW((dir + L"\\" + name + L".ini").c_str()) != INVALID_FILE_ATTRIBUTES; n++) {
        if (ReadFileUtf8(dir + L"\\" + name + L".ini", old) && old == text) break;   // the same skin again: just pick it
        name = stem + L"-" + std::to_wstring(n);
    }
    HANDLE h = CreateFileW((dir + L"\\" + name + L".ini").c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { Notify(T_(S_SKIN_BAD)); return; }
    DWORD w; WriteFile(h, text.data(), (DWORD)text.size(), &w, NULL); CloseHandle(h);
    g_set.skin = "custom:" + Narrow(name.c_str());
    LoadSkins();
    if (CurSkin().hasCups) g_set.cups = CurSkin().cups;
    SaveSettings(); RefreshLook();
    wchar_t msg[200]; swprintf(msg, 200, T_(S_SKIN_OK), SkinLabel(CurSkin()).c_str()); Notify(msg);
}

// ---------- stats card ----------
static void MakeStatsCard() {
    g_stats.save();
    const MonthStats& s = g_stats.cur;
    wchar_t title[64]; swprintf(title, 64, L"%ls %d", (g_ru ? kMonthRu : kMonthEn)[(s.month + 11) % 12], s.year);
    CardText tx = {title, T_(S_CARD_LISTENED), T_(S_CARD_HOURS), T_(S_CARD_AVG), T_(S_CARD_DROPS), T_(S_CARD_RECORD), T_(S_CARD_FOOTER)};
    wchar_t file[MAX_PATH]; swprintf(file, MAX_PATH, L"%ls\\stats-%04d-%02d.png", g_dir.c_str(), s.year, s.month);
    if (RenderCard(file, s, CurSkin(), g_set.cups, tx, CurOptions().hat)) ShellExecuteW(NULL, L"open", file, NULL, NULL, SW_SHOWNORMAL);
    else MessageBoxW(NULL, T_(S_STATS_FAIL), T_(S_APP), MB_ICONWARNING);
}

// ---------- tray ----------
static void WriteLog() {
    if (!g_set.log || g_dir.empty()) return;
    std::wstring lp = g_dir + L"\\log.txt";
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
    g_stats.tick(m == MODE_NOD, b / 10.0, g_drop != 0);
    wchar_t tip[128];
    if (m == MODE_NOD) swprintf(tip, 128, T_(S_TIP_BPM), (long)((b + 5) / 10));
    else if (m == MODE_SILENCE) swprintf(tip, 128, L"%ls", T_(S_TIP_SILENCE));
    else swprintf(tip, 128, L"%ls", T_(S_TIP_LISTEN));
    if (wcscmp(tip, g_nid.szTip)) { wcsncpy(g_nid.szTip, tip, 127); g_nid.uFlags = NIF_TIP; Shell_NotifyIconW(NIM_MODIFY, &g_nid); }
    if (g_streamWnd && g_set.stream) InvalidateRect(g_streamWnd, NULL, FALSE);
}
static void AddTray() {
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = g_msgWnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_APP_TRAY;
    if (!g_icon[0]) MakeIcons();
    g_nid.hIcon = g_icon[0]; g_iconShown = 0;
    wcsncpy(g_nid.szTip, T_(S_APP), 127);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

enum { CMD_LEFT = 100, CMD_RIGHT, CMD_RESET, CMD_EARLIER, CMD_LATER, CMD_EARLIER50, CMD_LATER50, CMD_AUTOSTART, CMD_MOVES, CMD_COLOR_CUSTOM, CMD_EXIT,
       CMD_EGGS, CMD_TRAYANIM, CMD_STREAM, CMD_RGB, CMD_STATS, CMD_OPEN_SKINS, CMD_DRAW,
       CMD_COLOR0 = 200, CMD_SKIN0 = 300, CMD_HAT0 = 400, CMD_APP0 = 500 };
struct CupColor { Str name; DWORD rgb; };
static const CupColor kCups[] = { {S_PINK, 0xFF5D73}, {S_RED, 0xE53935}, {S_ORANGE, 0xFF8C1A}, {S_GREEN, 0x3CCB5A},
                                  {S_CYAN, 0x3DB8FF}, {S_BLUE, 0x3D5DFF}, {S_PURPLE, 0x9B5DE5}, {S_WHITE, 0xF2F2F2} };
static const int kCupCount = sizeof(kCups) / sizeof(kCups[0]);

static void RefreshLook() {
    RenderGuy(g_main);
    for (auto& sl : g_apps) if (sl.win.visible) RenderGuy(sl.win);
    MakeIcons(); SetTrayIcon(0);
    if (g_streamWnd) InvalidateRect(g_streamWnd, NULL, FALSE);
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
    LoadSkins();                          // pick up skins dropped into the folder since the last time
    HMENU m = CreatePopupMenu();
    auto chk = [](bool b) { return MF_STRING | (b ? MF_CHECKED : 0); };
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
    for (int i = 0; i < kCupCount; i++) AppendMenuW(cm, chk(g_set.cups == kCups[i].rgb), CMD_COLOR0 + i, T_(kCups[i].name));
    AppendMenuW(cm, MF_SEPARATOR, 0, NULL);
    AppendMenuW(cm, MF_STRING, CMD_COLOR_CUSTOM, T_(S_CUSTOM));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)cm, T_(S_COLOR));
    HMENU sm = CreatePopupMenu();
    for (size_t i = 0; i < g_skins.size() && i < 60; i++) {
        if (i == 5) AppendMenuW(sm, MF_SEPARATOR, 0, NULL);
        AppendMenuW(sm, chk(i == g_skinIdx), CMD_SKIN0 + (UINT)i, SkinLabel(g_skins[i]).c_str());
    }
    AppendMenuW(sm, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sm, MF_STRING, CMD_DRAW, T_(S_DRAW));
    AppendMenuW(sm, MF_STRING, CMD_OPEN_SKINS, T_(S_OPEN_SKINS));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sm, T_(S_SKIN));
    HMENU hm = CreatePopupMenu();
    const Str hatNames[5] = {S_HAT_AUTO, S_HAT_OFF, S_HAT_WITCH, S_HAT_SANTA, S_HAT_PARTY};
    for (int i = 0; i < 5; i++) { AppendMenuW(hm, chk(g_set.hat == i - 1), CMD_HAT0 + i, T_(hatNames[i])); if (i == 1) AppendMenuW(hm, MF_SEPARATOR, 0, NULL); }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)hm, T_(S_HATS));
    AppendMenuW(m, chk(g_set.moves), CMD_MOVES, T_(S_MOVES));
    AppendMenuW(m, chk(g_set.eggs), CMD_EGGS, T_(S_EGGS));
    AppendMenuW(m, chk(g_set.trayAnim), CMD_TRAYANIM, T_(S_TRAYANIM));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    // per-app guys: the ones already chosen plus whatever is making sound right now
    HMENU am = CreatePopupMenu(); std::vector<std::wstring> appList;
    if (!g_appsSupported) AppendMenuW(am, MF_STRING | MF_GRAYED, 0, T_(S_APPS_OLD));
    else {
        appList = g_set.apps;
        wchar_t self[MAX_PATH]; GetModuleFileNameW(NULL, self, MAX_PATH); const wchar_t* sb = wcsrchr(self, L'\\');
        std::wstring me = applisten::Lower(sb ? sb + 1 : self);
        for (auto& e : applisten::PlayingNow()) if (e != me && std::find(appList.begin(), appList.end(), e) == appList.end()) appList.push_back(e);
        AppendMenuW(am, MF_STRING | MF_GRAYED, 0, T_(S_APPS_HINT));
        if (appList.empty()) AppendMenuW(am, MF_STRING | MF_GRAYED, 0, T_(S_APPS_NONE));
        for (size_t i = 0; i < appList.size() && i < 40; i++) {
            bool on = std::find(g_set.apps.begin(), g_set.apps.end(), appList[i]) != g_set.apps.end();
            AppendMenuW(am, chk(on) | (!on && g_set.apps.size() >= 3 ? MF_GRAYED : 0), CMD_APP0 + (UINT)i, appList[i].c_str());
        }
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)am, T_(S_APPS));
    AppendMenuW(m, chk(g_set.stream), CMD_STREAM, T_(S_STREAM));
    AppendMenuW(m, chk(g_set.rgb), CMD_RGB, T_(S_RGB));
    if (g_set.rgb) {
        wchar_t st[96];
        if (orgb::g_state == orgb::ST_CONNECTED) swprintf(st, 96, T_(S_RGB_OK), (int)orgb::g_devices); else wcsncpy(st, T_(S_RGB_NOSERVER), 95), st[95] = 0;
        AppendMenuW(m, MF_STRING | MF_GRAYED, 0, st);
    }
    AppendMenuW(m, MF_STRING, CMD_STATS, T_(S_STATS));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, chk(AutoStartOn()), CMD_AUTOSTART, T_(S_AUTOSTART));
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
    case CMD_EGGS: g_set.eggs = !g_set.eggs; break;
    case CMD_TRAYANIM: g_set.trayAnim = !g_set.trayAnim; if (!g_set.trayAnim) SetTrayIcon(0); break;
    case CMD_STREAM: g_set.stream = !g_set.stream; ShowStream(g_set.stream); break;
    case CMD_RGB: g_set.rgb = !g_set.rgb; InterlockedExchange(&orgb::g_enabled, g_set.rgb); break;
    case CMD_STATS: MakeStatsCard(); return;
    case CMD_OPEN_SKINS: OpenSkinsFolder(); return;
    case CMD_DRAW: ShellExecuteW(NULL, L"open", kEditorUrl, NULL, NULL, SW_SHOWNORMAL); return;
    case CMD_COLOR_CUSTOM: if (!PickCustomColor()) return; RefreshLook(); break;
    case CMD_AUTOSTART: SetAutoStart(!AutoStartOn()); break;
    case CMD_EXIT: DestroyWindow(g_msgWnd); return;
    default:
        if (cmd >= CMD_COLOR0 && cmd < CMD_COLOR0 + kCupCount) { g_set.cups = kCups[cmd - CMD_COLOR0].rgb; RefreshLook(); break; }
        if (cmd >= CMD_SKIN0 && cmd < CMD_SKIN0 + (int)g_skins.size()) {
            g_skinIdx = cmd - CMD_SKIN0; g_set.skin = g_skins[g_skinIdx].id;
            if (g_skins[g_skinIdx].hasCups) g_set.cups = g_skins[g_skinIdx].cups;
            RefreshLook(); break;
        }
        if (cmd >= CMD_HAT0 && cmd < CMD_HAT0 + 5) { g_set.hat = cmd - CMD_HAT0 - 1; break; }
        if (cmd >= CMD_APP0 && cmd < CMD_APP0 + (int)appList.size()) {
            const std::wstring& e = appList[cmd - CMD_APP0];
            auto it = std::find(g_set.apps.begin(), g_set.apps.end(), e);
            if (it != g_set.apps.end()) g_set.apps.erase(it); else if (g_set.apps.size() < 3) g_set.apps.push_back(e);
            ApplyApps(); break;
        }
        return;
    }
    g_set.latencyMs = std::max(-500, std::min(500, g_set.latencyMs));
    SaveSettings(); UpdatePlacement();
}

static LRESULT CALLBACK OverlayProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_APP_FRAME) {
        GuyWin* g = (GuyWin*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!g) return 0;
        g->pose = UnpackPose(wp);
        if (g->visible) RenderGuy(*g);
        if (g->main) {
            if (g_set.trayAnim) { int want = g->pose.y > Y_REST ? 1 : 0; if (want != g_iconShown) SetTrayIcon(want); }
            if (g_streamWnd && g_set.stream) InvalidateRect(g_streamWnd, NULL, FALSE);
        }
        return 0;
    }
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
    case WM_COPYDATA: {
        const COPYDATASTRUCT* cd = (const COPYDATASTRUCT*)lp;
        if (cd && cd->dwData == kCopyLink && cd->cbData >= 2 && cd->cbData < (1u << 20))
            InstallFromLink(std::wstring((const wchar_t*)cd->lpData, cd->cbData / 2));
        return TRUE;
    }
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

static HWND MakeOverlay(GuyWin& g) {
    g.wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                            L"HeadphonesGuyOverlay", T_(S_APP), WS_POPUP, 0, 0, 32, 36, NULL, NULL, g_inst, NULL);
    if (g.wnd) SetWindowLongPtrW(g.wnd, GWLP_USERDATA, (LONG_PTR)&g);
    return g.wnd;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR cmd, int) {
    // --card <file.png> [en|ru]: draw a sample stats card and quit (used to check the card renderer)
    if (cmd && !wcsncmp(cmd, L"--card ", 7)) {
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        std::wstring a = cmd + 7; g_ru = a.find(L" ru") != std::wstring::npos; std::wstring file = a.substr(0, a.find(L' '));
        g_skins = BuiltinSkins();
        MonthStats s; s.year = 2026; s.month = 10; s.seconds = 41 * 3600 + 1200; s.bpmSeconds = s.seconds * 196; s.drops = 312; s.record = 287;
        wchar_t title[64]; swprintf(title, 64, L"%ls %d", (g_ru ? kMonthRu : kMonthEn)[9], 2026);
        CardText tx = {title, T_(S_CARD_LISTENED), T_(S_CARD_HOURS), T_(S_CARD_AVG), T_(S_CARD_DROPS), T_(S_CARD_RECORD), T_(S_CARD_FOOTER)};
        return RenderCard(file.c_str(), s, g_skins[0], 0xFF5D73, tx, HAT_WITCH) ? 0 : 2;
    }
    std::wstring link;
    if (cmd) { const wchar_t* l = wcsstr(cmd, L"headphonesguy:"); if (l) { link = l; while (!link.empty() && (link.back() == L'"' || link.back() == L' ')) link.pop_back(); } }
    HANDLE mtx = CreateMutexW(NULL, TRUE, L"Local\\HeadphonesGuyOneInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // already running: hand the link over to it
        HWND other = FindWindowW(L"HeadphonesGuyTray", NULL);
        if (other && !link.empty()) {
            COPYDATASTRUCT cd = {kCopyLink, (DWORD)(link.size() * 2), (void*)link.data()};
            DWORD_PTR r; SendMessageTimeoutW(other, WM_COPYDATA, 0, (LPARAM)&cd, SMTO_ABORTIFHUNG, 5000, &r);
        }
        return 0;
    }
    g_inst = inst;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    SetDpiAware();
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpcFreq = (double)f.QuadPart;
    InitializeCriticalSection(&g_cs);
    for (auto& sl : g_apps) InitializeCriticalSection(&sl.cs);
    g_ru = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
    LoadSettings();
    LoadSkins();
    g_stats.path = g_dir.empty() ? L"" : g_dir + L"\\stats.ini";
    { SYSTEMTIME t; GetLocalTime(&t); g_stats.load(t.wYear, t.wMonth); }
    g_tracker.sharpOnsets = g_set.sharp;
    g_appsSupported = applisten::Supported();
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    FixAutoStartPath();
    RegisterProtocol();

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = OverlayProc; wc.hInstance = inst; wc.lpszClassName = L"HeadphonesGuyOverlay"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);
    WNDCLASSEXW wm = {sizeof(wm)};
    wm.lpfnWndProc = MsgProc; wm.hInstance = inst; wm.lpszClassName = L"HeadphonesGuyTray";
    RegisterClassExW(&wm);
    WNDCLASSEXW ws = {sizeof(ws)};
    ws.lpfnWndProc = StreamProc; ws.hInstance = inst; ws.lpszClassName = L"HeadphonesGuyStream"; ws.hCursor = LoadCursor(NULL, IDC_ARROW);
    ws.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    RegisterClassExW(&ws);

    g_main.main = true;
    for (int i = 0; i < 3; i++) g_apps[i].win.cups = kAppCups[i];
    bool ok = MakeOverlay(g_main) != NULL;
    for (auto& sl : g_apps) ok = ok && MakeOverlay(sl.win) != NULL;
    g_msgWnd = CreateWindowExW(WS_EX_TOOLWINDOW, wm.lpszClassName, T_(S_APP), WS_POPUP, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    if (!ok || !g_msgWnd) return 1;

    AddTray();
    UpdatePlacement();
    if (g_set.stream) ShowStream(true);
    SetTimer(g_msgWnd, TIMER_POS, 300, NULL);
    SetTimer(g_msgWnd, TIMER_TIP, 1000, NULL);

    orgb::g_running = &g_running; orgb::g_color = RgbNow; orgb::g_enabled = g_set.rgb;
    HANDLE ta = CreateThread(NULL, 0, AudioThread, NULL, 0, NULL);
    HANDLE tr = CreateThread(NULL, 0, RenderThread, NULL, 0, NULL);
    HANDLE tg = CreateThread(NULL, 0, orgb::Thread, NULL, 0, NULL);
    SetThreadPriority(tr, THREAD_PRIORITY_ABOVE_NORMAL);
    ApplyApps();
    if (!link.empty()) InstallFromLink(link);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    InterlockedExchange(&g_running, 0);
    for (auto& sl : g_apps) InterlockedExchange(&sl.run, 0);
    HANDLE th[3] = {ta, tr, tg};
    WaitForMultipleObjects(3, th, TRUE, 3000);
    for (auto& sl : g_apps) if (sl.thread) WaitForSingleObject(sl.thread, 2000);
    g_stats.save();
    if (g_streamWnd) DestroyWindow(g_streamWnd);
    DestroyWindow(g_main.wnd);
    for (auto& sl : g_apps) DestroyWindow(sl.win.wnd);
    for (auto& i : g_icon) if (i) DestroyIcon(i);
    if (mtx) CloseHandle(mtx);
    CoUninitialize();
    return 0;
}
