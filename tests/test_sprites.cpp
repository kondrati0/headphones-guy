// Sprites, skins, hats and the animator: every frame is 16x16, hats are 16 wide, easter eggs fire at the right tempo.
#include "animator.h"
#include <cstdio>
#include <cstring>
static int fail = 0;
static void expect(bool ok, const char* what) { if (!ok) { printf("FAIL %s\n", what); fail++; } }
int main() {
    for (int f = 0; f < F_COUNT; f++) for (int r = 0; r < 16; r++) expect(strlen(g_maps[f][r]) == 16, "frame row is 16 wide");
    for (int h = 1; h < HAT_COUNT; h++) for (int r = 0; r < HAT_ROWS; r++) expect(strlen(kHats[h].rows[r]) == 16, "hat row is 16 wide");
    expect(BuiltinSkins().size() == 5, "5 built-in skins");
    expect(HatForDate(10, 31) == HAT_WITCH && HatForDate(1, 1) == HAT_SANTA && HatForDate(6, 1) == HAT_NONE, "hats by date");
    // nodding guy at three tempos: plain, sweating, smoking
    double bpms[3] = {200, 280, 298}; int want[3] = {0, FX_SWEAT, FX_SMOKE};
    for (int i = 0; i < 3; i++) {
        GuyStatus st; st.mode = MODE_NOD; st.period = 60 / bpms[i]; st.gridT0 = 0; st.bpm = bpms[i]; st.locked = true;
        Animator a; AnimOptions o; Pose p = a.step(st, 10.0, o, nullptr);
        expect((p.fx & (FX_SWEAT | FX_SMOKE)) == want[i], "tempo easter egg");
        o.eggs = false; expect(a.step(st, 10.0, o, nullptr).fx == 0, "easter eggs can be turned off");
    }
    // asleep at night in silence
    { GuyStatus st; Animator a; AnimOptions o; o.night = true; Pose p = a.step(st, 5.0, o, nullptr); expect(p.fx == FX_ZZZ && p.frame == F_SIL_BLINK, "sleeps at night"); }
    // drawing never writes outside the box, even with a hat on a jump and the build-up shiver
    { const int s = 2, W = BOX_W * s, H = BOX_H * s; std::vector<uint32_t> px(W * H + 64, 0xDEADBEEF);
      for (int i = 0; i < W * H; i++) px[i] = 0;
      Pose p; p.frame = F_BUILD; p.y = Y_JUMP; p.x = 1; p.hat = HAT_WITCH; p.fx = FX_SMOKE | FX_SWEAT | FX_ZZZ;
      for (int ph = 0; ph < 6; ph++) { p.phase = ph; DrawGuy(px.data(), W, s, BOX_H, p, BuiltinSkins()[3], 0xFF5D73); }
      bool clean = true; for (int i = W * H; i < W * H + 64; i++) clean &= px[i] == 0xDEADBEEF; expect(clean, "draws inside the box"); }
    // skin files: BOM, CRLF, comments, overlay, a partly replaced frame, junk characters
    {
        std::string ini = "\xEF\xBB\xBF; comment\r\n[skin]\r\nname=Shades\r\nface=7FA8FF   ; blue\r\ncups=#FFC83D\r\n1=111111\r\n"
                          "[overlay]\r\n6=...1111..1111..\r\n7=....11....11...\r\n"
                          "[frames]\r\ndrop.9=.DCyyyyXXyyyyCD.\r\n";
        Skin s; expect(ParseSkinIni(ini, s), "parses a skin");
        expect(s.name == "Shades", "name");
        expect(s.col['y'] == Argb(0x7FA8FF) && s.hasCups && s.cups == 0xFFC83D && s.col['1'] == Argb(0x111111), "colors");
        expect(s.overlay.size() == 16 && s.overlay[6] == "...1111..1111.." + std::string(".") && s.overlay[0] == "................", "overlay rows");
        expect(s.frame[F_DROP].size() == 16 && s.frame[F_DROP][9] == ".DCyyyy..yyyyCD." && s.frame[F_DROP][0] == g_maps[F_DROP][0], "partial frame keeps the other rows");
        expect(s.frame[F_LISTEN].empty(), "untouched frames stay built-in");
        Skin junk; expect(!ParseSkinIni("hello world", junk), "not a skin");
        // overlay is drawn on top of the frame
        const int sc = 1; std::vector<uint32_t> px(BOX_W * BOX_H, 0); Pose p; p.frame = F_LISTEN; p.y = Y_REST;
        DrawGuy(px.data(), BOX_W, sc, BOX_H, p, s, 0xFF5D73);
        expect(px[(6 + Y_REST) * BOX_W + 3] == Argb(0x111111), "overlay drawn over the face");
        std::string dec; expect(Base64Decode("W3NraW5dCm5hbWU9WA", dec) && dec == "[skin]\nname=X", "base64url");
    }
    printf(fail ? "%d check(s) failed\n" : "sprites ok\n", fail);
    return fail ? 1 : 0;
}
