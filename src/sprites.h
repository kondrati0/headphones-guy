// Headphones Guy sprites: frames, skins, seasonal hats and little effects (sweat, smoke, zzz).
// Portable C++, no OS code: the Windows app and the web demo draw the guy with the same code.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

// 16x16 frames. Legend: k outline, e eyes/mouth, y face, B headband, C/D/L cup (main / rim / highlight, from the
// headphone color), t tongue, 1-9 extra colors a skin can define, '.' transparent.
enum Frame { F_SIL_OPEN, F_SIL_BLINK, F_AWAKE_OPEN, F_AWAKE_BLINK, F_LISTEN, F_LOUD, F_PAUSE, F_BUILD, F_DROP, F_TALK, F_COUNT };
inline const char* kFrameNames[F_COUNT] = { "idle", "idle_blink", "awake", "awake_blink", "listen", "loud", "pause", "build", "drop", "talk" };
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
    /* F_TALK */ {"....BBBBBBBB....","..BBkkkkkkkkBB..",".B.kyyyyyyyyk.B.",".B.kyyyyyyyyk.B.",".BkyyyyyyyyyykB.",".DCyyyyyyyyyyCD.",".DLyyeyyyyeyyLD.",".DCyyeyyyyeyyCD.",".DCyyyyyyyyyyCD.",".DCyyyyeeyyyyCD.","..kyyyyeeyyyyk..","...kyyyyyyyyk...","....kkkkkkkk....","................","................","................"},
};
// box the guy is drawn into: 16 x 18 cells; sprite top row sits at these offsets
enum { BOX_W = 16, BOX_H = 18 };
enum { Y_REST = 2, Y_NOD = 3, Y_LOUD = 4, Y_JUMP = 1 };

static inline uint32_t Argb(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFFFFFF); }
// f < 1 darker, f > 1 towards white
static inline uint32_t MixRgb(uint32_t rgb, double f) {
    int c[3] = {(int)(rgb >> 16) & 255, (int)(rgb >> 8) & 255, (int)rgb & 255};
    for (int i = 0; i < 3; i++) c[i] = f <= 1 ? (int)(c[i] * f) : (int)(c[i] + (255 - c[i]) * (f - 1));
    return 0xFF000000u | (c[0] << 16) | (c[1] << 8) | c[2];
}
// premultiplied ARGB with alpha a (layered windows want premultiplied pixels)
static inline uint32_t Premul(uint32_t rgb, int a) {
    int r = ((rgb >> 16) & 255) * a / 255, g = ((rgb >> 8) & 255) * a / 255, b = (rgb & 255) * a / 255;
    return ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
}

// ---------- skins ----------
struct Skin {
    std::string id, name;
    uint32_t col[128] = {};               // ARGB per legend char, 0 = transparent
    bool hasCups = false; uint32_t cups = 0;
    std::vector<std::string> frame[F_COUNT];   // empty = built-in frame
    std::vector<std::string> overlay;          // accessories drawn over every frame (16 rows), empty = none
    const char* row(int f, int r) const { return frame[f].size() == 16 ? frame[f][r].c_str() : g_maps[f][r]; }
};
inline Skin MakeSkin(const char* id, const char* name, uint32_t face, uint32_t outline, uint32_t eyes, uint32_t band, int cups = -1) {
    Skin s; s.id = id; s.name = name;
    s.col['y'] = Argb(face); s.col['k'] = Argb(outline); s.col['e'] = Argb(eyes); s.col['B'] = Argb(band); s.col['t'] = Argb(0xFF6F7F);
    if (cups >= 0) { s.hasCups = true; s.cups = (uint32_t)cups; }
    return s;
}
inline std::vector<Skin> BuiltinSkins() {
    std::vector<Skin> v;
    v.push_back(MakeSkin("classic", "Classic", 0xFFC83D, 0x3B2410, 0x3B2410, 0xC8CCD6));
    v.push_back(MakeSkin("pumpkin", "Pumpkin", 0xFF8C1A, 0x7A3A06, 0x2A1200, 0x3A7A2A));
    v.push_back(MakeSkin("robot", "Robot", 0xB8BCC6, 0x4A4E58, 0xE53935, 0x6A6E78, 0x3DB8FF));
    v.push_back(MakeSkin("demon", "Demon", 0xE5533D, 0x5A1208, 0xFFC83D, 0x444444, 0x222222));
    v.push_back(MakeSkin("zombie", "Zombie", 0x9ACD5A, 0x2F4A12, 0x2F4A12, 0x8A8F7A, 0x9B5DE5));
    return v;
}
// ---------- skin files ----------
// One small ini, the same for the app and the web editor:
//   [skin]    name, face, outline, eyes, headband, tongue, cups (RRGGBB), 1..9 extra colors
//   [overlay] 0= .. 15=   accessories, 16 chars per row, drawn over every frame
//   [frames]  listen.0= .. listen.15= (and any other frame name) to replace whole frames
inline std::string Trim(const std::string& x) {
    size_t a = 0, b = x.size();
    while (a < b && (x[a] == ' ' || x[a] == '\t' || x[a] == '\r')) a++;
    while (b > a && (x[b - 1] == ' ' || x[b - 1] == '\t' || x[b - 1] == '\r')) b--;
    return x.substr(a, b - a);
}
inline std::string Lower(std::string x) { for (auto& c : x) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); return x; }
inline bool ParseHex(const std::string& v, uint32_t& out) {
    std::string h = v; if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.size() < 6) return false;
    uint32_t r = 0;
    for (int i = 0; i < 6; i++) { char c = h[i]; int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; if (d < 0) return false; r = r * 16 + d; }
    out = r; return true;
}
// a sprite row: 16 chars, anything unknown becomes transparent
inline std::string CleanRow(const std::string& v) {
    std::string r = v.substr(0, 16); r.resize(16, '.');
    for (auto& c : r) if (!strchr("keyBCDLt123456789", c)) c = '.';
    return r;
}
// starts from the classic guy; returns false if it doesn't look like a skin at all
inline bool ParseSkinIni(const std::string& text, Skin& s) {
    Skin base = BuiltinSkins()[0]; base.id = s.id; s = base;
    std::string sec; bool any = false;
    std::vector<std::string> fr[F_COUNT]; std::vector<std::string> ov(16, std::string(16, '.')); bool haveOv = false;
    for (int f = 0; f < F_COUNT; f++) fr[f].assign(16, std::string());
    size_t i = 0;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3;   // UTF-8 BOM
    while (i < text.size()) {
        size_t j = text.find('\n', i); if (j == std::string::npos) j = text.size();
        std::string line = Trim(text.substr(i, j - i)); i = j + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') { size_t e = line.find(']'); sec = Lower(Trim(line.substr(1, e == std::string::npos ? std::string::npos : e - 1))); continue; }
        size_t eq = line.find('='); if (eq == std::string::npos) continue;
        std::string key = Lower(Trim(line.substr(0, eq))), val = Trim(line.substr(eq + 1));
        if (sec == "skin") {
            uint32_t c;
            if (key == "name") { s.name = val.substr(0, 60); any = true; continue; }
            size_t sc = val.find(';'); if (sc != std::string::npos) val = Trim(val.substr(0, sc));
            if (!ParseHex(val, c)) continue;
            any = true;
            if (key == "face") s.col['y'] = Argb(c); else if (key == "outline") s.col['k'] = Argb(c); else if (key == "eyes") s.col['e'] = Argb(c);
            else if (key == "headband") s.col['B'] = Argb(c); else if (key == "tongue") s.col['t'] = Argb(c);
            else if (key == "cups") { s.hasCups = true; s.cups = c; }
            else if (key.size() == 1 && key[0] >= '1' && key[0] <= '9') s.col[(int)key[0]] = Argb(c);
        } else if (sec == "overlay") {
            std::string k = key.compare(0, 8, "overlay.") == 0 ? key.substr(8) : key;
            int r = atoi(k.c_str()); if (k.empty() || r < 0 || r > 15) continue;
            ov[r] = CleanRow(val); haveOv = any = true;
        } else if (sec == "frames") {
            size_t dot = key.rfind('.'); if (dot == std::string::npos) continue;
            std::string nm = key.substr(0, dot); int r = atoi(key.c_str() + dot + 1); if (r < 0 || r > 15) continue;
            for (int f = 0; f < F_COUNT; f++) if (nm == kFrameNames[f]) { fr[f][r] = CleanRow(val); any = true; }
        }
    }
    for (int f = 0; f < F_COUNT; f++) {
        bool used = false; for (auto& r : fr[f]) used |= !r.empty();
        if (used) { for (int r = 0; r < 16; r++) if (fr[f][r].empty()) fr[f][r] = g_maps[f][r]; s.frame[f] = fr[f]; }   // rows left out stay as drawn
    }
    if (haveOv) s.overlay = ov;
    if (s.name.empty()) s.name = "My skin";
    return any;
}
// base64url (and plain base64) decode, for skins sent by link
inline bool Base64Decode(const std::string& in, std::string& out) {
    out.clear(); int buf = 0, bits = 0;
    for (char c : in) {
        int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : (c == '-' || c == '+') ? 62 : (c == '_' || c == '/') ? 63 : c == '=' ? -2 : -1;
        if (v == -2) break;
        if (v < 0) return false;
        buf = (buf << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; out += (char)((buf >> bits) & 255); }
    }
    return true;
}

// skin ids that have a Russian name in the UI
inline const char* SkinNameRu(const std::string& id) {
    static const char* ids[] = {"classic", "pumpkin", "robot", "demon", "zombie"};
    static const char* ru[] = {"Классика", "Тыква", "Робот", "Демон", "Зомби"};
    for (int i = 0; i < 5; i++) if (id == ids[i]) return ru[i];
    return nullptr;
}

// ---------- seasonal hats: 6 rows on sprite rows -2..3, pulled down over the forehead ----------
enum Hat { HAT_NONE, HAT_WITCH, HAT_SANTA, HAT_PARTY, HAT_COUNT };
enum { HAT_ROWS = 6 };
struct HatArt { const char* rows[HAT_ROWS]; char keys[6]; uint32_t cols[6]; };
static const HatArt kHats[HAT_COUNT] = {
    {{"", "", "", "", "", ""}, "", {}},
    {{"..........pp....", ".........ppp....", "......ppppp.....", ".....pppppp.....", ".....oooooo.....", "..bbbbbbbbbbbb.."}, "pob", {0xFF5B3A8C, 0xFFFF8C1A, 0xFF3D2466}},
    {{"...........ww...", ".........rrww...", ".......rrrr.....", "......rrrrrr....", ".....rrrrrrr....", "...wwwwwwwwww..."}, "rw", {0xFFE53935, 0xFFF2F2F2}},
    {{".......yy.......", ".......cc.......", "......cdcc......", "......cccd......", ".....cdcccc.....", "................"}, "cdy", {0xFF3DB8FF, 0xFFFFC83D, 0xFFFFC83D}},
};
// which hat a date asks for (month 1-12). Halloween week, New Year holidays.
inline int HatForDate(int month, int day) {
    if ((month == 10 && day >= 25) || (month == 11 && day <= 1)) return HAT_WITCH;
    if ((month == 12 && day >= 20) || (month == 1 && day <= 10)) return HAT_SANTA;
    return HAT_NONE;
}

// ---------- pose: everything needed to draw one moment ----------
enum { FX_SWEAT = 1, FX_SMOKE = 2, FX_ZZZ = 4 };
struct Pose {
    int frame = F_SIL_OPEN, y = Y_REST, x = 0, fx = 0, phase = 0, hat = HAT_NONE;
    bool operator==(const Pose& o) const { return frame == o.frame && y == o.y && x == o.x && fx == o.fx && phase == o.phase && hat == o.hat; }
    bool operator!=(const Pose& o) const { return !(*this == o); }
};

// premultiplied "over"
static inline uint32_t Over(uint32_t src, uint32_t dst) {
    uint32_t a = src >> 24; if (a == 255 || !dst) return src;
    uint32_t k = 255 - a, out = 0;
    for (int sh = 0; sh < 32; sh += 8) { uint32_t v = ((src >> sh) & 255) + ((dst >> sh) & 255) * k / 255; out |= (v > 255 ? 255 : v) << sh; }
    return out;
}
// fill one cell (box coordinates) if it is inside the box
static inline void Cell(uint32_t* px, int stride, int s, int rows, int c, int r, uint32_t col) {
    if (c < 0 || c >= BOX_W || r < 0 || r >= rows || !col) return;
    for (int dy = 0; dy < s; dy++) { uint32_t* row = px + (size_t)(r * s + dy) * stride + c * s; for (int dx = 0; dx < s; dx++) row[dx] = Over(col, row[dx]); }
}

// Draw the guy into a premultiplied ARGB buffer (stride in pixels), s pixels per cell, rows = box height in cells.
inline void DrawGuy(uint32_t* px, int stride, int s, int rows, const Pose& p, const Skin& sk, uint32_t cupsRgb) {
    uint32_t cC = Argb(cupsRgb), cD = MixRgb(cupsRgb, 0.62), cL = MixRgb(cupsRgb, 1.5);
    for (int r = 0; r < 16; r++) {
        const char* line = sk.row(p.frame, r);
        for (int c = 0; c < 16; c++) {
            int cs = c - p.x; if (cs < 0 || cs >= 16) continue;
            char ch = line[cs]; uint32_t col = 0;
            if (ch == 'C') col = cC; else if (ch == 'D') col = cD; else if (ch == 'L') col = cL;
            else if (ch > 0 && ch < 127) col = sk.col[(int)ch];
            Cell(px, stride, s, rows, c, r + p.y, col);
        }
    }
    if (sk.overlay.size() == 16) {         // accessories ride along with the head
        for (int r = 0; r < 16; r++) for (int c = 0; c < 16; c++) {
            char ch = sk.overlay[r][c]; if (ch == '.') continue;
            uint32_t col = ch == 'C' ? cC : ch == 'D' ? cD : ch == 'L' ? cL : (ch > 0 && ch < 127) ? sk.col[(int)ch] : 0;
            Cell(px, stride, s, rows, c + p.x, r + p.y, col);
        }
    }
    if (p.hat > HAT_NONE && p.hat < HAT_COUNT) {
        const HatArt& h = kHats[p.hat];
        for (int r = 0; r < HAT_ROWS; r++) for (int c = 0; c < 16 && h.rows[r][c]; c++) {
            const char* k = strchr(h.keys, h.rows[r][c]);
            if (h.rows[r][c] != '.' && k) Cell(px, stride, s, rows, c + p.x, p.y - 2 + r, h.cols[k - h.keys]);
        }
    }
    if (p.fx & FX_SWEAT) {                 // drops sliding down both sides of the face
        uint32_t w = 0xFF7FD1FF, hi = 0xFFD6F1FF; int d = p.phase % 3;
        Cell(px, stride, s, rows, 13 + p.x, p.y + 2 + d, hi); Cell(px, stride, s, rows, 13 + p.x, p.y + 3 + d, w);
        int d2 = (d + 1) % 3;
        Cell(px, stride, s, rows, 2 + p.x, p.y + 2 + d2, hi); Cell(px, stride, s, rows, 2 + p.x, p.y + 3 + d2, w);
    }
    if (p.fx & FX_SMOKE) {                 // steam out of the ears (and off the top), fading as it rises
        for (int k = 0; k < 2; k++) {
            int ph = (p.phase + k * 3) % 6, a = 235 - ph * 30;
            uint32_t c = Premul(0xE6E6E6, a);
            int r = p.y + 5 - ph, wob = (ph >> 1) & 1;
            Cell(px, stride, s, rows, 0, r, c); Cell(px, stride, s, rows, 15, r, c);
            if (ph >= 3) { Cell(px, stride, s, rows, 0 + wob, r - 1, c); Cell(px, stride, s, rows, 15 - wob, r - 1, c); }
            Cell(px, stride, s, rows, 6 + 3 * k + wob, p.y - 1 - ph / 3, Premul(0xE6E6E6, a - 40));
        }
    }
    if (p.fx & FX_ZZZ) {                   // z's floating up on the right
        uint32_t z = 0xFFC8D0FF; int ph = p.phase % 3;
        static const char* big[4] = {"zzzz", "..z.", ".z..", "zzzz"};
        static const char* small[3] = {"zz.", ".z.", ".zz"};
        if (ph >= 1) for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (big[r][c] == 'z') Cell(px, stride, s, rows, 12 + c, p.y - 2 + r, z);
        if (ph != 1) for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) if (small[r][c] == 'z') Cell(px, stride, s, rows, 9 + c, p.y - 2 + r, z);
    }
}
