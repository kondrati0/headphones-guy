// Synthetic breakcore with triplet rolls and half-time stretches: the tracker must not jump to a wrong tempo.
#ifndef TRACKER
#define TRACKER "tracker.h"
#endif
#include TRACKER
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



// nasty breakcore: triplet rolls every 8 s for 2 s, and a half-time stretch
Song chaos(double bpm, double len) {
    Song g{"chaos", bpm, len, std::vector<float>((size_t)(len * SR), 0.f), {}};
    double B = 60 / bpm; std::uniform_real_distribution<double> P(0, 1);
    for (int b = 0; b * B + 0.4 < len; b++) {
        double t = b * B; g.beats.push_back(t); double sec = fmod(t, 8.0);
        if (sec > 5.5 && sec < 7.5) { for (int k = 0; k < 3; k++) addNoise(g.s, t + k * B / 3, 0.5, 20, 210); if (b % 2 == 0) addKick(g.s, t + B / 3, 0.6, true); continue; }
        if (t > 20 && t < 28) { if (b % 4 == 0) addKick(g.s, t, 0.8, true); if (b % 4 == 2) addNoise(g.s, t, 0.6, 15, 200); addNoise(g.s, t, 0.08, 60, 0); continue; }
        for (int k = 0; k < 4; k++) { double tt = t + k * B / 4; double r = P(rng); if (k == 0 && b % 2 == 0) addKick(g.s, tt, 0.8, true); else if (r < 0.25) addKick(g.s, tt, 0.6, true); else if (r < 0.55) addNoise(g.s, tt, 0.5, 20, 210); else if (r < 0.8) addNoise(g.s, tt, 0.12, 60, 0); }
    }
    addPad(g.s, 0, len, 0.08); return g;
}
int main() {
    int fail = 0;
    for (double bpm : {170.0, 180.0, 200.0}) {
        Song g = chaos(bpm, 45); BeatTracker tr; tr.init(SR); double clock = 0; size_t bi = 0; int good = 0, tot = 0, wrongT = 0, frames = 0;
        for (size_t i = 0; i + 480 <= g.s.size(); i += 480) {
            tr.feed(&g.s[i], 480, clock); clock += 0.01; GuyStatus st = tr.status(clock);
            if (clock > 8) { frames++; if (st.mode == MODE_NOD && fabs(st.bpm / bpm - 1) > 0.03) wrongT++; }
            while (bi < g.beats.size() && g.beats[bi] <= clock - 0.05) {
                double tb = g.beats[bi++];
                if (tb < 8) continue;
                tot++;
                if (st.mode != MODE_NOD) continue;
                double k = floor((tb - st.gridT0) / st.period + 0.5), e = tb - (st.gridT0 + k * st.period);
                if (fabs(e) < 0.035 && fabs(st.bpm / bpm - 1) < 0.02) good++; }
        }
        int onbeat = 100 * good / tot, wrong = 100 * wrongT / frames;
        printf("chaos %.0f: onbeat %d%%  wrong-tempo nod %d%% of time\n", bpm, onbeat, wrong);
        // triplet rolls must not drag the tempo away
        if (onbeat < 90 || wrong > 2) { printf("  FAIL chaos %.0f: onbeat < 90%% or wrong tempo > 2%%\n", bpm); fail++; }
    }
    return fail ? 1 : 0;
}
