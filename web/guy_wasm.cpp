// WebAssembly build of the tracker + sprites for the browser demo. Same code as the Windows app.
// Build: python3 -m ziglang c++ --target=wasm32-wasi -O2 -mexec-model=reactor -fno-exceptions -I../src guy_wasm.cpp -o guy.wasm
#include "tracker.h"
#include "sprites.h"
#include "animator.h"
#include <vector>

#define EXPORT(name) extern "C" __attribute__((export_name(#name)))

static BeatTracker g_tr;
static Animator g_anim;
static GuyStatus g_st;
static std::vector<float> g_in;
static std::vector<uint32_t> g_px;
static std::vector<uint8_t> g_rgba;
static std::vector<Skin> g_skins = BuiltinSkins();
static std::string g_text;

EXPORT(hg_init) void hg_init(double sr) { g_tr = BeatTracker(); g_tr.init(sr); g_anim = Animator(); g_st = GuyStatus(); }
EXPORT(hg_buffer) float* hg_buffer(int n) { if ((int)g_in.size() < n) g_in.resize(n); return g_in.data(); }
EXPORT(hg_feed) void hg_feed(int n, double t0) { g_tr.feed(g_in.data(), n, t0); }
EXPORT(hg_update) int hg_update(double now) { g_st = g_tr.status(now); return g_st.mode; }
EXPORT(hg_bpm) double hg_bpm() { return g_st.bpm; }
// 0 normal, 1 loud, 2 quiet, 3 build-up, 4 drop
EXPORT(hg_section) int hg_section() { return g_st.drop ? 4 : g_st.build ? 3 : g_st.intensity == 2 ? 1 : g_st.intensity == 0 ? 2 : 0; }
EXPORT(hg_skin_count) int hg_skin_count() { return (int)g_skins.size(); }

// ---- skin editor helpers ----
EXPORT(hg_text_buf) char* hg_text_buf(int n) { g_text.assign(n, 0); return &g_text[0]; }
// parse the ini in the text buffer into the custom skin slot (index 5); 1 if it looked like a skin
EXPORT(hg_skin_load) int hg_skin_load() {
    Skin s; s.id = "custom:editor"; bool ok = ParseSkinIni(g_text, s);
    if (g_skins.size() < 6) g_skins.push_back(s); else g_skins[5] = s;
    return ok ? 1 : 0;
}
EXPORT(hg_frame_count) int hg_frame_count() { return F_COUNT; }
EXPORT(hg_frame_name) const char* hg_frame_name(int f) { return kFrameNames[f]; }
EXPORT(hg_frame_row) const char* hg_frame_row(int f, int r) { return g_maps[f][r]; }
// RRGGBB of a legend char in a skin (k e y B t 1-9); cups for 'C'
EXPORT(hg_skin_color) uint32_t hg_skin_color(int skin, int ch) {
    const Skin& s = g_skins[(skin >= 0 && skin < (int)g_skins.size()) ? skin : 0];
    if (ch == 'C') return s.hasCups ? s.cups : 0xFF5D73;
    return (ch > 0 && ch < 128) ? s.col[ch] & 0xFFFFFF : 0;
}

static uint8_t* ToRgba(int W, int H);
// draw a fixed pose (frame, vertical offset in cells) for previews
EXPORT(hg_draw_pose) uint8_t* hg_draw_pose(int frame, int y, int scale, uint32_t cups, int skin, int hat) {
    Pose p; p.frame = (frame >= 0 && frame < F_COUNT) ? frame : F_LISTEN; p.y = y; p.hat = hat;
    int W = BOX_W * scale, H = BOX_H * scale;
    g_px.assign((size_t)W * H, 0);
    DrawGuy(g_px.data(), W, scale, BOX_H, p, g_skins[(skin >= 0 && skin < (int)g_skins.size()) ? skin : 0], cups);
    return ToRgba(W, H);
}

// draw the guy at 'now' into an RGBA buffer of (16*scale) x (18*scale); returns a pointer to it
EXPORT(hg_draw) uint8_t* hg_draw(double now, int scale, uint32_t cups, int skin, int hat, int eggs, double latency, int night) {
    AnimOptions o; o.latency = latency; o.eggs = eggs != 0; o.hat = hat; o.night = night != 0;
    Pose p = g_anim.step(g_st, now, o, nullptr);
    int W = BOX_W * scale, H = BOX_H * scale;
    g_px.assign((size_t)W * H, 0);
    DrawGuy(g_px.data(), W, scale, BOX_H, p, g_skins[(skin >= 0 && skin < (int)g_skins.size()) ? skin : 0], cups);
    return ToRgba(W, H);
}
static uint8_t* ToRgba(int W, int H) {
    g_rgba.resize((size_t)W * H * 4);
    for (size_t i = 0; i < g_px.size(); i++) {           // premultiplied ARGB -> straight RGBA
        uint32_t c = g_px[i], a = c >> 24;
        uint8_t* d = &g_rgba[i * 4];
        if (!a) { d[0] = d[1] = d[2] = d[3] = 0; continue; }
        d[0] = (uint8_t)std::min<uint32_t>(255, ((c >> 16) & 255) * 255 / a);
        d[1] = (uint8_t)std::min<uint32_t>(255, ((c >> 8) & 255) * 255 / a);
        d[2] = (uint8_t)std::min<uint32_t>(255, (c & 255) * 255 / a);
        d[3] = (uint8_t)a;
    }
    return g_rgba.data();
}
