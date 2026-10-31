// Synthetic genres 132-260 BPM: each track alone after silence, then back to back.
#include "tracker.h"
#include <cstdio>
#include <random>
#include <string>
#include <functional>

static const double SR = 48000;
std::mt19937 rng(7);
std::uniform_real_distribution<float> U(-1, 1);

struct Song { std::string name; double bpm; double len; std::vector<float> s; std::vector<double> beats; };

void addKick(std::vector<float>& s, double t, double amp, bool hard, double maxLen = 0.35) {
    int i0 = (int)(t * SR); double ph = 0;
    for (int i = 0; i < (int)(maxLen * SR) && i0 + i < (int)s.size(); i++) {
        double tt = i / SR, f = 45 + 110 * exp(-tt * 30);
        ph += 2 * M_PI * f / SR;
        double v = sin(ph) * exp(-tt * (hard ? 6 : 12));
        if (hard) v = tanh(v * 6) * 0.8;
        if (i < 96) v += U(rng) * 0.5 * (1 - i / 96.0);
        s[i0 + i] += (float)(amp * v);
    }
}
void addNoise(std::vector<float>& s, double t, double amp, double dec, double tone) {
    int i0 = (int)(t * SR); float prev = 0;
    for (int i = 0; i < (int)(0.25 * SR) && i0 + i < (int)s.size(); i++) {
        double tt = i / SR; float n = U(rng); float hp = n - prev; prev = n;
        double v = (tone > 0 ? 0.5 * sin(2 * M_PI * tone * tt) : 0) + hp * 0.7;
        s[i0 + i] += (float)(amp * v * exp(-tt * dec));
    }
}
void addBass(std::vector<float>& s, double t, double amp, double dur) {
    int i0 = (int)(t * SR);
    for (int i = 0; i < (int)(dur * SR) && i0 + i < (int)s.size(); i++) {
        double tt = i / SR; double e = std::min(1.0, tt / 0.01) * exp(-tt * 8);
        s[i0 + i] += (float)(amp * sin(2 * M_PI * 55 * tt) * e);
    }
}
void addPad(std::vector<float>& s, double t0, double t1, double amp) {
    for (int i = (int)(t0 * SR); i < (int)(t1 * SR) && i < (int)s.size(); i++) {
        double t = i / SR; s[i] += (float)(amp * (sin(2 * M_PI * 220 * t) + sin(2 * M_PI * 277 * t) + sin(2 * M_PI * 330 * t)) / 3);
    }
}

Song make(const std::string& name, double bpm, double len) {
    Song g{name, bpm, len, std::vector<float>((size_t)(len * SR), 0.f), {}};
    double B = 60 / bpm; int nb = (int)(len / B);
    std::uniform_real_distribution<double> P(0, 1);
    for (int b = 0; b < nb; b++) {
        double t = b * B; g.beats.push_back(t); int pos = b % 4;
        if (name == "trance") { addKick(g.s, t, 0.8, false); addBass(g.s, t + B / 2, 0.35, B / 2); addNoise(g.s, t + B / 2, 0.15, 25, 0); addNoise(g.s, t + B / 4, 0.05, 60, 0); addNoise(g.s, t + 3 * B / 4, 0.05, 60, 0); }
        else if (name == "ukg") { if (pos == 0) addKick(g.s, t, 0.8, false); if (pos == 1) addKick(g.s, t + B * 0.75, 0.6, false); if (pos == 2) addKick(g.s, t + B * 0.5, 0.6, false); if (pos == 1 || pos == 3) addNoise(g.s, t, 0.4, 18, 190); for (int k = 0; k < 4; k++) addNoise(g.s, t + k * B / 4 + (k % 2 ? B * 0.06 : 0), 0.07, 70, 0); addBass(g.s, t + B * 0.5, 0.25, B * 0.4); }
        else if (name == "dnb") { if (pos == 0) addKick(g.s, t, 0.8, false); if (pos == 2) addKick(g.s, t + B * 0.5, 0.7, false); if (pos == 1 || pos == 3) addNoise(g.s, t, 0.6, 15, 200); for (int k = 0; k < 2; k++) addNoise(g.s, t + k * B / 2, 0.06, 70, 0); }
        else if (name == "breakcore") { for (int k = 0; k < 4; k++) { double tt = t + k * B / 4; double r = P(rng); if (k == 0 && pos % 2 == 0) addKick(g.s, tt, 0.8, true); else if (r < 0.25) addKick(g.s, tt, 0.6, true); else if (r < 0.55) addNoise(g.s, tt, 0.5, 20, 210); else if (r < 0.8) addNoise(g.s, tt, 0.12, 60, 0); } }
        else { addKick(g.s, t, 0.85, true, std::min(0.35, B * 0.9)); addNoise(g.s, t + B / 2, 0.08, 60, 0); if (pos == 1 || pos == 3) addNoise(g.s, t, 0.3, 20, 200); }
    }
    addPad(g.s, 0, len, 0.08);
    for (auto& v : g.s) v += U(rng) * 0.003f;
    return g;
}

// regression floors: a bit under the current numbers, so CI fails when the tracker gets worse
static int g_fail = 0;
static void expect(bool ok, const char* what, const std::string& name) { if (!ok) { printf("  FAIL %s: %s\n", name.c_str(), what); g_fail++; } }

int main() {
    std::vector<Song> songs = { make("ukg", 132, 25), make("trance", 140, 25), make("dnb", 174, 25), make("breakcore", 180, 25),
                                make("hardcore", 200, 25), make("uptempo", 230, 25), make("terror", 260, 25) };
    for (int mode = 0; mode < 2; mode++) {
        printf(mode == 0 ? "== each track alone (with silence before)\n" : "== back to back, no gaps\n");
        BeatTracker tr; tr.init(SR);
        double clock = 1000.0;
        for (auto& g : songs) {
            if (mode == 0) { std::vector<float> z(480, 0.f); for (int i = 0; i < 400; i++) { tr.feed(z.data(), 480, clock); clock += 0.01; tr.status(clock); } }
            double start = clock; double tLock = -1; int good = 0, tot = 0; double errSum = 0; double lastBpm = 0;
            int modeCount[5] = {0};
            size_t bi = 0; double serr = 0; double tOk = -1;
            for (size_t i = 0; i + 480 <= g.s.size(); i += 480) {
                tr.feed(&g.s[i], 480, clock); clock += 0.01;
                GuyStatus st = tr.status(clock); modeCount[st.mode]++;
                if (st.mode == MODE_NOD && tLock < 0) tLock = clock - start;
                bool ok = st.locked && fabs(st.bpm / g.bpm - 1) < 0.02;
                if (ok && tOk < 0) tOk = clock - start;
                if (!ok) tOk = -1;
                while (bi < g.beats.size() && start + g.beats[bi] <= clock - 0.05) {
                    double tb = start + g.beats[bi++]; if (tb - start < 8) continue;
                    tot++; if (!st.locked) continue;
                    double k = floor((tb - st.gridT0) / st.period + 0.5), e = tb - (st.gridT0 + k * st.period);
                    errSum += fabs(e); serr += e; if (fabs(e) < 0.03) good++;
                }
            }
            GuyStatus st = tr.status(clock);
            printf("  signedErr %.1fms tempoOK@%.2fs\n", tot? 1000*serr/tot:0, tOk);
            lastBpm = st.bpm;
            printf("%-10s true %5.1f  got %6.1f  lock@%5.2fs  onbeat %3d%%  meanErr %5.1fms  conf %.2f  modes S%d A%d L%d N%d P%d\n",
                   g.name.c_str(), g.bpm, lastBpm, tLock, tot ? 100 * good / tot : 0, tot ? 1000 * errSum / tot : 0, tr.lastConf,
                   modeCount[0], modeCount[1], modeCount[2], modeCount[3], modeCount[4]);
            expect(fabs(lastBpm / g.bpm - 1) < 0.02, "wrong tempo at the end", g.name);
            expect(tot && 100 * good / tot >= 85, "onbeat < 85%", g.name);
        }
    }
    if (g_fail) printf("%d check(s) failed\n", g_fail);
    return g_fail ? 1 : 0;
}
