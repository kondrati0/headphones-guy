// Synthetic transitions: back to back, 4 s crossfades, same BPM shifted half a beat, breakdown then drop.
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

Song make(const std::string& name, double bpm, double len, double off = 0, double bd0 = -1, double bd1 = -1) {
    Song g{name, bpm, len, std::vector<float>((size_t)(len * SR), 0.f), {}};
    double B = 60 / bpm; int nb = (int)(len / B);
    std::uniform_real_distribution<double> P(0, 1);
    for (int b = 0; b < nb; b++) {
        double t = off + b * B; if (t + 0.4 > len) break; int pos = b % 4;
        if (t >= bd0 && t < bd1) { addNoise(g.s, t, 0.04, 60, 0); continue; }
        g.beats.push_back(t);
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


struct Res { int good = 0, tot = 0; double tOk = -1; };
// play songs one after another (xfade seconds of overlap), score beats >= skip s after each start
void run(const char* title, std::vector<Song> songs, double xfade, double skip) {
    std::vector<float> mix; std::vector<std::vector<double>> beats; std::vector<double> starts, bpms;
    double pos = 1.0; mix.assign((size_t)(SR * 1.0), 0.f);
    for (auto& g : songs) {
        size_t i0 = (size_t)(pos * SR); if (mix.size() < i0 + g.s.size()) mix.resize(i0 + g.s.size(), 0.f);
        for (size_t i = 0; i < g.s.size(); i++) {
            double t = i / SR, gain = 1;
            if (xfade > 0 && t < xfade && starts.size()) gain = t / xfade;
            if (xfade > 0 && t > g.len - xfade && &g != &songs.back()) gain = (g.len - t) / xfade;
            mix[i0 + i] += (float)(g.s[i] * gain);
        }
        starts.push_back(pos); bpms.push_back(g.bpm); beats.push_back(g.beats);
        pos += g.len - xfade;
    }
    BeatTracker tr; tr.init(SR); double clock = 0;
    std::vector<Res> R(songs.size());
    for (size_t i = 0; i + 480 <= mix.size(); i += 480) {
        tr.feed(&mix[i], 480, clock); clock += 0.01; GuyStatus st = tr.status(clock);
        for (size_t s = 0; s < songs.size(); s++) {
            double rs = clock - starts[s]; double end = s + 1 < songs.size() ? starts[s + 1] + xfade * 0.5 : 1e9;
            if (rs < 0 || clock > end) continue;
            bool ok = st.mode == MODE_NOD && fabs(st.bpm / bpms[s] - 1) < 0.02;
            if (ok && R[s].tOk < 0) R[s].tOk = rs;
        }
    }
    // second pass scoring beats: replay deterministically
    BeatTracker t2; t2.init(SR); clock = 0; std::vector<size_t> bi(songs.size(), 0);
    for (size_t i = 0; i + 480 <= mix.size(); i += 480) {
        t2.feed(&mix[i], 480, clock); clock += 0.01; GuyStatus st = t2.status(clock);
        for (size_t s = 0; s < songs.size(); s++) {
            double end = s + 1 < songs.size() ? starts[s + 1] + xfade * 0.5 : 1e9;
            while (bi[s] < beats[s].size() && starts[s] + beats[s][bi[s]] <= clock - 0.05) {
                double tb = starts[s] + beats[s][bi[s]++];
                if (tb - starts[s] < skip || tb > end) continue;
                R[s].tot++;
                if (st.mode != MODE_NOD) continue;
                double k = floor((tb - st.gridT0) / st.period + 0.5), e = tb - (st.gridT0 + k * st.period);
                if (fabs(e) < 0.035 && fabs(st.bpm / bpms[s] - 1) < 0.02) R[s].good++;
            }
        }
    }
    printf("== %s\n", title); int G = 0, T = 0;
    for (size_t s = 0; s < songs.size(); s++) { printf("  %-10s %5.1f  onbeat %3d%%  ok@%5.2fs\n", songs[s].name.c_str(), songs[s].bpm, R[s].tot ? 100 * R[s].good / R[s].tot : 0, R[s].tOk); G += R[s].good; T += R[s].tot; }
    printf("  TOTAL %d%%\n", T ? 100 * G / T : 0);
}
int main() {
    auto set = [] { return std::vector<Song>{ make("ukg", 132, 25), make("trance", 140, 25), make("dnb", 174, 25), make("breakcore", 180, 25),
                                make("hardcore", 200, 25), make("uptempo", 230, 25), make("terror", 260, 25) }; };
    run("back to back, score from 4s", set(), 0, 4);
    run("crossfade 4s, score from 4s", set(), 4, 4);
    run("same tempo, shifted half beat", { make("dnb", 174, 25), make("dnb", 174, 25, 0.1724), make("hardcore", 200, 25), make("hardcore", 200, 25, 0.15) }, 0, 4);
    run("breakdown 15-25s then drop", { make("dnb", 174, 45, 0, 15, 25), make("hardcore", 200, 45, 0, 15, 25), make("breakcore", 180, 45, 0, 15, 25) }, 0, 4);
}
