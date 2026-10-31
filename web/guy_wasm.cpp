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

EXPORT(hg_init) void hg_init(double sr) { g_tr = BeatTracker(); g_tr.init(sr); g_anim = Animator(); g_st = GuyStatus(); }
EXPORT(hg_buffer) float* hg_buffer(int n) { if ((int)g_in.size() < n) g_in.resize(n); return g_in.data(); }
EXPORT(hg_feed) void hg_feed(int n, double t0) { g_tr.feed(g_in.data(), n, t0); }
EXPORT(hg_update) int hg_update(double now) { g_st = g_tr.status(now); return g_st.mode; }
EXPORT(hg_bpm) double hg_bpm() { return g_st.bpm; }
// 0 normal, 1 loud, 2 quiet, 3 build-up, 4 drop
EXPORT(hg_section) int hg_section() { return g_st.drop ? 4 : g_st.build ? 3 : g_st.intensity == 2 ? 1 : g_st.intensity == 0 ? 2 : 0; }
EXPORT(hg_skin_count) int hg_skin_count() { return (int)g_skins.size(); }

// draw the guy at 'now' into an RGBA buffer of (16*scale) x (18*scale); returns a pointer to it
EXPORT(hg_draw) uint8_t* hg_draw(double now, int scale, uint32_t cups, int skin, int hat, int eggs, double latency, int night) {
    AnimOptions o; o.latency = latency; o.eggs = eggs != 0; o.hat = hat; o.night = night != 0;
    Pose p = g_anim.step(g_st, now, o, nullptr);
    int W = BOX_W * scale, H = BOX_H * scale;
    g_px.assign((size_t)W * H, 0); g_rgba.resize((size_t)W * H * 4);
    DrawGuy(g_px.data(), W, scale, BOX_H, p, g_skins[(skin >= 0 && skin < (int)g_skins.size()) ? skin : 0], cups);
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
