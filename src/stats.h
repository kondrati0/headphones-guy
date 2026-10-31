// Monthly listening stats (time nodding, average tempo, drops, steady top tempo), kept in stats.ini,
// and the shareable PNG card for them.
#pragma once
#include <windows.h>
#include <wincodec.h>
#include <string>
#include <cmath>
#include "sprites.h"

struct MonthStats {
    int year = 0, month = 0;
    double seconds = 0, bpmSeconds = 0;   // time nodding, and sum of bpm over it (for the average)
    int drops = 0; double record = 0;     // record: highest tempo held steady for 10 s
};

class StatsKeeper {
public:
    std::wstring path;
    MonthStats cur;
    void load(int y, int m) {
        cur = MonthStats(); cur.year = y; cur.month = m;
        if (path.empty()) return;
        wchar_t sec[16]; swprintf(sec, 16, L"%04d-%02d", y, m);
        wchar_t b[64];
        GetPrivateProfileStringW(sec, L"seconds", L"0", b, 64, path.c_str()); cur.seconds = _wtof(b);
        GetPrivateProfileStringW(sec, L"bpm_seconds", L"0", b, 64, path.c_str()); cur.bpmSeconds = _wtof(b);
        cur.drops = (int)GetPrivateProfileIntW(sec, L"drops", 0, path.c_str());
        GetPrivateProfileStringW(sec, L"record", L"0", b, 64, path.c_str()); cur.record = _wtof(b);
    }
    void save() {
        if (path.empty() || !cur.year) return;
        wchar_t sec[16]; swprintf(sec, 16, L"%04d-%02d", cur.year, cur.month);
        wchar_t b[64];
        swprintf(b, 64, L"%.0f", cur.seconds); WritePrivateProfileStringW(sec, L"seconds", b, path.c_str());
        swprintf(b, 64, L"%.0f", cur.bpmSeconds); WritePrivateProfileStringW(sec, L"bpm_seconds", b, path.c_str());
        swprintf(b, 64, L"%d", cur.drops); WritePrivateProfileStringW(sec, L"drops", b, path.c_str());
        swprintf(b, 64, L"%.0f", cur.record); WritePrivateProfileStringW(sec, L"record", b, path.c_str());
    }
    // once a second
    void tick(bool nodding, double bpm, bool drop) {
        SYSTEMTIME t; GetLocalTime(&t);
        if (t.wYear != cur.year || t.wMonth != cur.month) { save(); load(t.wYear, t.wMonth); }
        if (nodding && bpm > 0) {
            cur.seconds += 1; cur.bpmSeconds += bpm;
            if (steadyBpm > 0 && fabs(bpm / steadyBpm - 1) < 0.02) { if (++steadyRun >= 10 && steadyBpm > cur.record) cur.record = steadyBpm; }
            else { steadyBpm = bpm; steadyRun = 0; }
        } else steadyRun = 0;
        if (drop && !wasDrop) cur.drops++;
        wasDrop = drop;
        if (++sinceSave >= 60) { sinceSave = 0; save(); }
    }
private:
    double steadyBpm = 0; int steadyRun = 0, sinceSave = 0; bool wasDrop = false;
};

// ---------- PNG card ----------
static const GUID kCLSID_WICFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID kIID_WICFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID kContainerPng = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID kFormatBGRA = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f}};

static bool SavePng(const wchar_t* file, const uint32_t* px, int W, int H) {
    IWICImagingFactory* f = NULL; IWICStream* st = NULL; IWICBitmapEncoder* enc = NULL; IWICBitmapFrameEncode* fr = NULL; IPropertyBag2* pb = NULL;
    bool ok = SUCCEEDED(CoCreateInstance(kCLSID_WICFactory, NULL, CLSCTX_INPROC_SERVER, kIID_WICFactory, (void**)&f))
           && SUCCEEDED(f->CreateStream(&st)) && SUCCEEDED(st->InitializeFromFilename(file, GENERIC_WRITE))
           && SUCCEEDED(f->CreateEncoder(kContainerPng, NULL, &enc)) && SUCCEEDED(enc->Initialize(st, WICBitmapEncoderNoCache))
           && SUCCEEDED(enc->CreateNewFrame(&fr, &pb)) && SUCCEEDED(fr->Initialize(pb)) && SUCCEEDED(fr->SetSize(W, H));
    if (ok) { WICPixelFormatGUID pf = kFormatBGRA; ok = SUCCEEDED(fr->SetPixelFormat(&pf)) && IsEqualGUID(pf, kFormatBGRA); }
    if (ok) ok = SUCCEEDED(fr->WritePixels(H, W * 4, W * H * 4, (BYTE*)px)) && SUCCEEDED(fr->Commit()) && SUCCEEDED(enc->Commit());
    if (pb) pb->Release();
    if (fr) fr->Release();
    if (enc) enc->Release();
    if (st) st->Release();
    if (f) f->Release();
    return ok;
}

struct CardText { const wchar_t *title, *listened, *hours, *avg, *drops, *record, *footer; };

// 1200x675 (16:9, good for Reddit / Telegram previews)
static bool RenderCard(const wchar_t* file, const MonthStats& s, const Skin& sk, uint32_t cups, const CardText& tx, int hat = HAT_NONE) {
    const int W = 1200, H = 675;
    HDC dc = CreateCompatibleDC(NULL);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* px = NULL; HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&px, NULL, 0);
    if (!bmp) { DeleteDC(dc); return false; }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    for (int i = 0; i < W * H; i++) px[i] = 0xFF1C1C1E;
    // the guy on the left, drop face
    Pose p; p.frame = F_DROP; p.y = Y_REST; p.hat = hat;
    const int sc = 24, gx = 70, gy = (H - BOX_H * sc) / 2;
    std::vector<uint32_t> g(BOX_W * sc * BOX_H * sc, 0);
    DrawGuy(g.data(), BOX_W * sc, sc, BOX_H, p, sk, cups);
    for (int y = 0; y < BOX_H * sc; y++) for (int x = 0; x < BOX_W * sc; x++) { uint32_t c = g[(size_t)y * BOX_W * sc + x]; if (c >> 24) px[(size_t)(gy + y) * W + gx + x] = c | 0xFF000000; }
    GdiFlush();
    SetBkMode(dc, TRANSPARENT);
    auto font = [&](int h, int w) { return CreateFontW(-h, 0, 0, 0, w, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI"); };
    HFONT fTitle = font(44, FW_SEMIBOLD), fLabel = font(26, FW_NORMAL), fNum = font(72, FW_SEMIBOLD), fFoot = font(24, FW_NORMAL);
    auto text = [&](HFONT f, COLORREF c, int x, int y, const wchar_t* t) { SelectObject(dc, f); SetTextColor(dc, c); TextOutW(dc, x, y, t, (int)wcslen(t)); };
    const int tx0 = 560;
    text(fTitle, RGB(255, 255, 255), tx0, 70, tx.title);
    wchar_t b[4][64];
    double hrs = s.seconds / 3600.0;
    swprintf(b[0], 64, hrs < 10 ? L"%.1f %ls" : L"%.0f %ls", hrs, tx.hours);
    swprintf(b[1], 64, L"%.0f", s.seconds > 0 ? s.bpmSeconds / s.seconds : 0.0);
    swprintf(b[2], 64, L"%d", s.drops);
    swprintf(b[3], 64, L"%.0f BPM", s.record);
    const wchar_t* labels[4] = {tx.listened, tx.avg, tx.drops, tx.record};
    COLORREF cupsRef = RGB((cups >> 16) & 255, (cups >> 8) & 255, cups & 255);
    COLORREF numCol[4] = {RGB(255, 255, 255), cupsRef, RGB(255, 255, 255), RGB(0xFF, 0xC8, 0x3D)};
    for (int i = 0; i < 4; i++) {
        int x = tx0 + (i % 2) * 300, y = 180 + (i / 2) * 190;
        text(fLabel, RGB(0x99, 0x99, 0x9E), x, y, labels[i]);
        text(fNum, numCol[i], x, y + 34, b[i]);
    }
    text(fFoot, RGB(0x77, 0x77, 0x7C), tx0, H - 80, tx.footer);
    GdiFlush();
    for (int i = 0; i < W * H; i++) px[i] |= 0xFF000000;   // GDI text leaves alpha at 0
    bool ok = SavePng(file, px, W, H);
    SelectObject(dc, oldBmp);
    DeleteObject(fTitle); DeleteObject(fLabel); DeleteObject(fNum); DeleteObject(fFoot); DeleteObject(bmp); DeleteDC(dc);
    return ok;
}
