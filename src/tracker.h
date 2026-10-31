// Headphones Guy beat tracker
// Spectral-flux onsets (2048 FFT for tempo, ~11 ms FFT for beat phase, hop ~5 ms) -> tempo from accumulated
// autocorrelation with a prior around 170 BPM -> beat phase from a long-memory, kick-weighted beat histogram.
// On top: track-change detection, same-BPM phase jumps and seek recovery, a mid-band check for 240+ BPM,
// a kick+snare check against 3:4 polyrhythm jumps, and section analysis (loudness, drops, build-up rolls)
// from the share of the mix taken by the bass.
// Tuned on real hardcore / uptempo / breakcore / neurofunk / dubstep tracks.
// Portable C++, no OS deps. All times in seconds on one shared clock.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum GuyMode { MODE_SILENCE = 0, MODE_AWAKE = 1, MODE_LISTEN = 2, MODE_NOD = 3, MODE_PAUSE = 4 };

struct GuyStatus {
    int mode = MODE_SILENCE;
    double period = 0.5;   // seconds per beat
    double gridT0 = 0;     // time of a beat
    double bpm = 0;
    bool locked = false;
    long long beatIdx = 0; // index of the beat at gridT0 (for every-other-beat nods)
    int intensity = 1;     // 0 quiet, 1 normal, 2 loud (bass level vs recent peak)
    bool drop = false;     // inside a drop: bass slammed back after a break/build, until it gives way again
    bool dropFresh = false; // first couple of bars of that drop
    bool build = false;     // snare roll / build-up right now (bass out, hits getting dense)
};

// real FFT of size N (power of two) via N/2 complex FFT.
// Plain float math on purpose: std::complex operator* without -ffast-math goes through __mulsc3 and was most of the CPU time.
class RealFFT {
public:
    void init(int n) {
        N = n; H = n / 2;
        twr.resize(H); twi.resize(H); pr.resize(H); pi.resize(H);
        for (int k = 0; k < H; k++) { twr[k] = (float)cos(-2 * M_PI * k / H); twi[k] = (float)sin(-2 * M_PI * k / H); }
        for (int k = 0; k < H; k++) { pr[k] = (float)cos(-2 * M_PI * k / N); pi[k] = (float)sin(-2 * M_PI * k / N); }
        rev.resize(H); int bits = 0; while ((1 << bits) < H) bits++;
        for (int i = 0; i < H; i++) { int r = 0; for (int b = 0; b < bits; b++) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[i] = r; }
        re.resize(H); im.resize(H);
    }
    // in: N reals, out: magnitudes N/2+1
    void mag(const float* in, float* out) {
        float* R = re.data(); float* I = im.data();
        for (int i = 0; i < H; i++) { R[rev[i]] = in[2 * i]; I[rev[i]] = in[2 * i + 1]; }
        for (int len = 2; len <= H; len <<= 1) {
            int half = len / 2, step = H / len;
            for (int i = 0; i < H; i += len)
                for (int j = 0; j < half; j++) {
                    int a = i + j, b = a + half; float wr = twr[j * step], wi = twi[j * step];
                    float vr = R[b] * wr - I[b] * wi, vi = R[b] * wi + I[b] * wr;
                    R[b] = R[a] - vr; I[b] = I[a] - vi; R[a] += vr; I[a] += vi;
                }
        }
        out[0] = std::fabs(R[0] + I[0]);
        out[H] = std::fabs(R[0] - I[0]);
        for (int k = 1; k < H; k++) {
            float ar = R[k], ai = I[k], br = R[H - k], bi = -I[H - k];
            float er = 0.5f * (ar + br), ei = 0.5f * (ai + bi);
            float dr = ar - br, di = ai - bi;            // o = -0.5i * (a - b)
            float orr = 0.5f * di, oi = -0.5f * dr;
            float xr = er + pr[k] * orr - pi[k] * oi, xi = ei + pr[k] * oi + pi[k] * orr;
            out[k] = std::sqrt(xr * xr + xi * xi);
        }
    }
private:
    int N = 0, H = 0;
    std::vector<float> twr, twi, pr, pi, re, im; std::vector<int> rev;
};

class BeatTracker {
public:
    // tuning (values found offline on real tracks, checked by tests/)
    double minBpm = 70, maxBpm = 300, priorCenter = 170, priorSigmaOct = 0.6;
    double lockConf = 0.21, lowConfThr = 0.10, fasterRatio = 0.75;
    double winSec = 6.0, tempoMemSec = 16.0, phaseMemSec = 24.0, thetaGain = 0.3;
    int switchCount = 6; bool snapFirst = false;
    bool sharpOnsets = true;          // phase onsets from the short FFT (false = old 2048-window behaviour)
    int changeCount = 6;              // fresh-window votes (x0.25 s) needed to call a track change
    double changeSupport = 0.6;       // ...and the current tempo must fall below this share of the fresh peak

    void init(double sampleRate) {
        sr = sampleRate;
        hop = std::max(1, (int)lround(sr / 200.0));
        fps = sr / hop;
        N = 2048; while (N < sr * 0.04) N *= 2;   // ~43 ms window
        fft.init(N); NS = 512; while (NS < sr * 0.0107) NS *= 2; fftS.init(NS);
        winS.resize(NS); for (int i = 0; i < NS; i++) winS[i] = (float)(0.5 - 0.5 * cos(2 * M_PI * i / (NS - 1)));
        frS.resize(NS); magS.resize(NS / 2 + 1); prevS.assign(NS / 2 + 1, 0.f); wPS.assign(NS / 2 + 1, 0.f);
        for (int k = 0; k <= NS / 2; k++) { double f = k * sr / NS, w = 0;
            if (f >= 20 && f < 70) w = 0.5; else if (f < 150 && f >= 70) w = 1.5; else if (f >= 150 && f < 400) w = 0.7;
            else if (f >= 400 && f < 1000) w = 0.5; else if (f >= 1000 && f < 3000) w = 0.3;
            wPS[k] = (float)w; }
        win.resize(N); for (int i = 0; i < N; i++) win[i] = (float)(0.5 - 0.5 * cos(2 * M_PI * i / (N - 1)));
        ring.assign(N, 0.f); rpos = 0; fill = 0;
        fr.resize(N); mag.resize(N / 2 + 1); prevL.assign(N / 2 + 1, 0.f); L.resize(N / 2 + 1);
        wPhase.assign(N / 2 + 1, 0.f); wTempo.assign(N / 2 + 1, 0.f); wMid.assign(N / 2 + 1, 0.f); wKick.assign(N / 2 + 1, 0.f);
        for (int k = 0; k <= N / 2; k++) {
            double f = k * sr / N, w = 0;
            if (f >= 20 && f < 70) w = 0.5; else if (f < 150 && f >= 70) w = 1.5; else if (f >= 150 && f < 400) w = 0.7;
            else if (f >= 400 && f < 1000) w = 0.5; else if (f >= 1000 && f < 3000) w = 0.3;
            wPhase[k] = (float)w; wTempo[k] = (f >= 20 && f < 24000) ? 1.f : 0.f; wMid[k] = (f >= 150 && f < 3000) ? 1.f : 0.f; wKick[k] = (f >= 20 && f < 150) ? 1.f : 0.f;
        }
        kB0 = std::max(1, (int)ceil(20.0 * N / sr)); kB1 = std::max(kB0, (int)floor(150.0 * N / sr));
        cap = (int)(fps * (winSec + 1)) + 8;
        ot.assign(cap, 0.f); op.assign(cap, 0.f); om.assign(cap, 0.f); ok.assign(cap, 0.f); envT.assign(cap, 0.0);
        minLag = fps * 60.0 / maxBpm; maxLag = fps * 60.0 / minBpm;
        acc = 0; sA = 0; firstFrame = true;
        lastLoud = -1e9; lastFrameT = -1e9; soundStart = 0; prevMode = MODE_SILENCE;
        resetAll();
    }

    void feed(const float* x, int n, double t0) {
        for (int i = 0; i < n; i++) {
            float v = x[i];
            ring[rpos] = v; rpos = (rpos + 1) % N; if (fill < N) fill++;
            sA += (double)v * v;
            if (++acc >= hop) { frame(t0 + (i + 1) / sr); acc = 0; sA = 0; }
        }
    }

    GuyStatus status(double now) {
        GuyStatus st;
        bool ready = locked && histReady;
        st.beatIdx = (long long)floor(psi - theta);
        st.intensity = inten;
        st.build = !dropOn && now - buildT < 0.6;
        st.drop = dropOn; st.dropFresh = dropOn && now - dropT < std::max(1.6, 8 * T);
        st.locked = ready; st.period = T; st.bpm = ready ? 60.0 / T : 0;
        // last beat before the newest frame
        double ph = psi - theta; ph -= floor(ph);
        st.gridT0 = lastFrameT - ph * T;
        double quiet = now - lastLoud;
        int m;
        if (quiet > 3.0) m = MODE_SILENCE;
        else if (quiet > 0.3) m = (prevMode == MODE_SILENCE) ? MODE_SILENCE : MODE_PAUSE;
        else if (ready && (!lowConf || st.dropFresh || st.build)) m = MODE_NOD;   // a drop means the beat is back, even if confidence lags
        else if (ready && lowConf) m = (now - lowSince < 8.0) ? MODE_PAUSE : MODE_LISTEN;
        else m = (now - soundStart < 7.0) ? MODE_LISTEN : MODE_AWAKE;
        prevMode = m;
        st.mode = m;
        return st;
    }

    double lastConf = 0, lastCandBpm = 0;
    // the player reported a discontinuity (seek, skip): re-find the beat position from what comes next
    void markCut(double t) { if (locked) { cutT = t; cutDone = false; } }
    int trackChanges = 0, staleRefreshes = 0, phaseJumps = 0;  // debug counters

private:
    double sr = 48000, fps = 200; int hop = 240, N = 2048;
    RealFFT fft, fftS; std::vector<float> winS, frS, magS, prevS, wPS; int NS = 512; std::vector<float> win, ring, fr, mag, prevL, L, wPhase, wTempo;
    int rpos = 0, fill = 0, acc = 0; double sA = 0; bool firstFrame = true;
    double mT = -1, mP = -1;
    std::vector<float> ot, op, om, xm, ok, xk; std::vector<double> envT; std::vector<float> wMid, wKick; int cap = 0, wpos = 0, count = 0, sinceAn = 0;
    double minLag = 40, maxLag = 171;
    double lastLoud = -1e9, lastFrameT = -1e9, soundStart = 0;
    // lock / tempo
    bool locked = false, lowConf = false; double T = 0.5, Tref = 0.5, lowSince = 0;
    int agree = 0, mismatch = 0, lowCnt = 0, highCnt = 0; double candT = 0, mmT = 0;
    std::vector<double> sacc;
    // phase
    static const int NB = 64;
    double hist[NB], histS[NB]; int phJump = 0, phWeak = 0; double phJumpTgt = 0; double psi = 0, theta = 0; bool histReady = false, thetaFresh = true;
    int prevMode = MODE_SILENCE;
    // track change
    int chg = 0, stale = 0; double chgT = 0, staleT = 0;
    double quietRunS = 0, cutT = -1e9; bool cutDone = true; int combBad = 0; double combTgt = 0; std::vector<double> cs;
    // loudness (bass band, dB)
    int kB0 = 1, kB1 = 6; std::vector<char> lowRing; int lowCount = 0, lowPos = 0, inten = 1; double shF = 0, lastLowT = -1e9, highRun = 0;
    double lvF = -100, lvPk = -100, dropT = -1e9, levelStart = 0; bool lvInit = false;
    std::vector<char> rollRing; int rollPos = 0, rollCount = 0; double mThr = 0, mPrev = 0, mPrev2 = 0, densBase = 0, buildT = -1e9;
    static const int HN = 60; float hShare[HN], hBass[HN]; char hLow[HN]; double dropRef = -1e9; int hPos = 0, hFill = 0, hStep = 0; double pB = 0, pT = 0, dropEndRun = 0; bool dropOn = false;
    std::vector<float> xb, xp; std::vector<double> acb, sc, pr; int aLo = 0, aHi = 0;

    void resetAll() {
        wpos = 0; count = 0; sinceAn = 0; locked = false; lowConf = false; agree = 0; mismatch = 0; lowCnt = highCnt = 0; candT = 0;
        sacc.clear(); resetPhase(); mT = mP = -1; chg = 0; lvInit = false; lvF = lvPk = -100; inten = 1; dropOn = false; hFill = hPos = hStep = 0; pB = pT = 0;
    }
    void resetPhase() { for (int i = 0; i < NB; i++) hist[i] = histS[i] = 0; phJump = 0; psi = 0; theta = 0; histReady = false; thetaFresh = true; }

    void frame(double tEnd) {
        double t = tEnd - 1.0 / fps;   // calibrated frame time
        double eA = 10.0 * log10(sA / hop + 1e-12);
        // spectrum of the last N samples
        { int n1 = N - rpos; const float* rg = ring.data();
          for (int i = 0; i < n1; i++) fr[i] = rg[rpos + i] * win[i];
          for (int i = n1; i < N; i++) fr[i] = rg[i - n1] * win[i]; }
        fft.mag(fr.data(), mag.data());
        double dT = 0, dP = 0, dM = 0, dK = 0;
        for (int k = 0; k <= N / 2; k++) {
            float l = log1pf(100.f * mag[k]);
            float d = l - prevL[k]; prevL[k] = l;
            if (d > 0) { dT += d * wTempo[k]; dM += d * wMid[k]; dK += d * wKick[k]; if (!sharpOnsets) dP += d * wPhase[k]; }
        }
        // beat phase from a short ~11 ms window: a 43 ms window smears long distorted kicks and nods late on them
        if (sharpOnsets) {
        for (int i = 0, p = (rpos + N - NS) % N; i < NS; i++, p = (p + 1 == N ? 0 : p + 1)) frS[i] = ring[p] * winS[i];
        fftS.mag(frS.data(), magS.data());
        for (int k = 0; k <= NS / 2; k++) { float l = log1pf(100.f * magS[k]); float d = l - prevS[k]; prevS[k] = l; if (d > 0) dP += d * wPS[k]; }
        }
        if (firstFrame) { dT = dP = 0; firstFrame = false; }
        double eb = 0, et = 0;
        for (int k = 0; k <= N / 2; k++) { double p = (double)mag[k] * mag[k]; et += p; if (k >= kB0 && k <= kB1) eb += p; }
        double eB = 10.0 * log10(eb + 1e-9);
        double share = 10.0 * log10((eb + 1e-12) / (et + 1e-12));   // how much of the mix is kick/bass, dB (0 = all of it)
        const double a = 1.0 / (fps * 2.0);
        if (mT < 0 && dT > 0) { mT = dT; mP = std::max(dP, 1e-6); }
        float vT = 0, vP = 0;
        if (mT > 0) {
            mT += a * (dT - mT); mP += a * (dP - mP);
            vT = (float)std::min(10.0, dT / (mT + 1e-6)); vP = (float)std::min(10.0, dP / (mP + 1e-6));
        }

        bool loud = eA > -55;
        // a short hole in the sound (seek / skip in the player) splits the music: remember where it happened
        if (eA < -60) quietRunS += 1.0 / fps;
        else { if (quietRunS >= 0.03 && quietRunS < 0.5 && locked) { cutT = tEnd; cutDone = false; } quietRunS = 0; }
        if (tEnd - lastFrameT > 0.25) resetAll();
        else if (tEnd - lastFrameT > 0.03 + 1.0 / fps && locked) { cutT = tEnd; cutDone = false; }   // packets skipped a bit of time
        if (loud) { if (tEnd - lastLoud > 0.5) soundStart = tEnd; lastLoud = tEnd; }
        lastFrameT = t;
        if (tEnd - lastLoud > 0.5) { if (count || locked) resetAll(); return; }
        if (!lvInit) { lvF = lvPk = eB; shF = share; lvInit = true; levelStart = t; lowCount = 0; lowPos = 0; lowRing.assign((size_t)(fps * 4.0), 0); lastLowT = -1e9; highRun = 0; pB = eb; pT = et; hFill = hPos = hStep = 0;
                       rollRing.assign((size_t)fps, 0); rollPos = rollCount = 0; mThr = dM; mPrev = mPrev2 = 0; densBase = 4; buildT = -1e9; }
        // loudness for nod strength: slow enough not to flicker between kicks, with hysteresis
        lvF += (eB - lvF) * (1.0 / (fps * 0.6));
        lvPk = std::max(lvF, lvPk - 0.4 / fps);     // peak sags 0.4 dB/s
        double rel = lvF - lvPk;
        if (inten == 2) { if (rel < -6.0) inten = 1; }
        else if (inten == 0) { if (rel > -9.0) inten = 1; }
        else { if (rel > -3.0) inten = 2; else if (rel < -12.0) inten = 0; }
        // drop: kick/bass takes over the mix again right after >= 1 s (out of the last 4 s) where it was a minor part.
        // Bass share is level-independent, so quiet intros, filtered builds and stutters all look the same.
        shF += (share - shF) * (1.0 / (fps * 0.08));
        bool low = shF < -10.0;
        lowCount += (int)low - (int)lowRing[lowPos]; lowRing[lowPos] = low; lowPos = (lowPos + 1) % (int)lowRing.size();
        if (low) lastLowT = t;
        highRun = shF >= -4.0 ? highRun + 1.0 / fps : 0;
        // slow (power-averaged) share: sparse distorted kicks and wobbles keep it high, a real break pulls it down
        pB += (eb - pB) * (1.0 / (fps * 0.4)); pT += (et - pT) * (1.0 / (fps * 0.4));
        double sLin = 10.0 * log10((pB + 1e-12) / (pT + 1e-12));
        if (++hStep >= (int)lround(fps * 0.05)) {          // 20 Hz history, last 3 s
            hStep = 0; hShare[hPos] = (float)sLin; hLow[hPos] = low; hBass[hPos] = (float)(10.0 * log10(pB + 1e-12)); hPos = (hPos + 1) % HN; if (hFill < HN) hFill++;
        }
        if (highRun >= 0.1 && t - lastLowT < 2.1 && lowCount >= fps * 1.0 && t - levelStart >= 5.0 && t - dropT > 8.0 && hFill == HN) {
            // ...but only if the stretch before it really was a break/build, not a wobble or a kick pattern with gaps
            float tmp[HN]; int nS = 0, nL = 0, nF = 0;
            for (int back = 10; back < HN; back++) tmp[nS++] = hShare[(hPos - 1 - back + 2 * HN) % HN];
            for (int back = 4; back < HN; back++) { nF++; nL += hLow[(hPos - 1 - back + 2 * HN) % HN]; }
            std::nth_element(tmp, tmp + nS / 2, tmp + nS);
            if (tmp[nS / 2] <= -6.0 || nL >= nF / 2) { if (!dropOn) dropRef = -1e9; dropT = t; dropOn = true; dropEndRun = 0; }
        }
        // the drop lasts until the bass gives way again (or 32 bars at most)
        // the drop is over when, for most of the last 1.5 s, the bass sits well under the level the drop
        // came in with, or stops dominating the mix (noisy breaks with stray kicks count too)
        if (dropOn && hStep == 0) {
            double bNow = 10.0 * log10(pB + 1e-12);
            if (t - dropT < 3.0) dropRef = std::max(dropRef, bNow);
            else {
                int weak = 0, nW = 30;
                for (int back = 0; back < nW; back++) { int i = (hPos - 1 - back + 2 * HN) % HN; if (hBass[i] < dropRef - 8.0 || hShare[i] < -10.0) weak++; }
                if (weak * 10 >= nW * 6) dropOn = false;
            }
            if (t - dropT > 128 * T) dropOn = false;
        }
        // build-up: snare/body hits suddenly get much denser (a roll) while the bass is out
        {
            mThr += (dM - mThr) * (1.0 / fps);                       // ~1 s average of the snare-band flux
            bool pk = mPrev > 1.5 * mThr && mPrev > mPrev2 && mPrev >= dM; mPrev2 = mPrev; mPrev = dM;
            rollCount += (int)pk - (int)rollRing[rollPos]; rollRing[rollPos] = pk; rollPos = (rollPos + 1) % (int)rollRing.size();
            double dens = rollCount / (rollRing.size() / fps);       // hits per second over the last second
            densBase += (dens - densBase) * (1.0 / (fps * 6.0));
            int lowN = 0; for (int back = 0; back < 40 && back < hFill; back++) lowN += hShare[(hPos - 1 - back + 2 * HN) % HN] < -8.0;
            if (!dropOn && lowN >= 24 && dens >= std::max(8.0, 1.4 * densBase)) buildT = t;
        }

        ot[wpos] = vT; op[wpos] = vP; om[wpos] = (float)dM; ok[wpos] = (float)dK; envT[wpos] = t; wpos = (wpos + 1) % cap; if (count < cap) count++;
        if (locked) {
            psi += 1.0 / (fps * T);
            if (psi > 1e6) psi -= 2 * floor(psi / 2);   // keep beat parity
            double dec = exp(-1.0 / (fps * phaseMemSec));
            double decS = exp(-1.0 / (fps * 3.0));
            for (int i = 0; i < NB; i++) { hist[i] *= dec; histS[i] *= decS; }
            double fpsi = psi - floor(psi);
            int hb = std::min(NB - 1, (int)(fpsi * NB));
            hist[hb] += vP; histS[hb] += vP;
            histReady = true;
        }
        if (++sinceAn >= 50) { sinceAn = 0; analyze(); }
    }

    static double at(const std::vector<float>& x, int W, double idx) {
        if (idx < 0) return 0;
        int i = (int)idx; double f = idx - i;
        if (i + 1 >= W) return i < W ? x[i] : 0;
        return x[i] * (1 - f) + x[i + 1] * f;
    }
    double comb(const std::vector<float>& x, int W, double P, double phi) {
        double s = 0, w = 1; int K = (int)((W - 1 - phi) / P);
        for (int k = 0; k <= K; k++) { s += w * at(x, W, W - 1 - phi - k * P); w *= 0.93; }
        return s;
    }
    double refinePeak(int l) {
        if (l <= 0 || l + 1 >= (int)acb.size()) return l;
        double a = acb[l - 1], b = acb[l], c = acb[l + 1], d = a - 2 * b + c;
        if (d >= 0) return l;
        double o = 0.5 * (a - c) / d; if (o > 0.5) o = 0.5; if (o < -0.5) o = -0.5;
        return l + o;
    }

    // Phase sanity after seeks and hard cuts. The long histogram keeps the old grid for ~20 s and on dense
    // hardcore it is too flat for the slow jump test, so check the beat position directly on the last ~2 s.
    void phaseCheck(int W, double now) {
        double P = fps * T;
        if (!cutDone && now - cutT >= 1.0) {             // 1 s of audio after a hole: rebuild the phase from it alone
            cutDone = true; combBad = 0;
            backfill(std::min(W, (int)(fps * (now - cutT))));
            thetaFresh = true; phaseJumps++;
            return;
        }
        if (W < (int)(P * 6)) return;
        auto combS = [&](double phi) { double s = 0, w = 1; for (int k = 0; k < 8; k++) { s += w * at(xp, W, W - 1 - phi - k * P); w *= 0.8; } return s; };
        double ph = psi - theta; ph -= floor(ph);
        double cur = combS(ph * P), best = -1, bphi = 0; cs.clear();
        for (double q = 0; q < P; q += 1.0) { double v = combS(q); cs.push_back(v); if (v > best) { best = v; bphi = q; } }
        std::nth_element(cs.begin(), cs.begin() + cs.size() / 2, cs.end());
        double base = cs[cs.size() / 2];            // dense music has a high floor; judge the contrast above it
        cur -= base; best -= base;
        double dph = (bphi - ph * P) / P; dph -= floor(dph + 0.5);
        double dq = (bphi - combTgt) / P; dq -= floor(dq + 0.5);
        // clearly better beat position elsewhere, and not the on/off-beat tie that dense kicks produce
        if (fabs(dph) > 0.12 && best > 1e-9 && cur < 0.5 * best && (combBad == 0 || fabs(dq) < 0.06)) {
            combTgt = bphi;
            if (++combBad >= 6) { combBad = 0; backfill(std::min(W, (int)(fps * 2.5))); thetaFresh = true; phaseJumps++; }
        } else combBad = 0;
    }
    // smoothed argmax of a beat histogram; optionally its smoothed peak value and mean
    int peakBin(const double* h, double* peak, double* mean) const {
        static const double k7[7] = {0.146447, 0.5, 0.853553, 1.0, 0.853553, 0.5, 0.146447};
        double bestv = -1, sum = 0; int bb = 0;
        for (int i = 0; i < NB; i++) {
            double s = 0; for (int d = -3; d <= 3; d++) s += k7[d + 3] * h[(i + d + NB) % NB];
            sum += s; if (s > bestv) { bestv = s; bb = i; }
        }
        if (peak) *peak = bestv;
        if (mean) *mean = sum / NB;
        return bb;
    }
    // rebuild the beat histogram from the recent window with the new period
    void backfill(int W) {
        resetPhase();
        double dec = exp(-1.0 / (fps * phaseMemSec));
        int n = (int)xp.size();
        for (int k = n - W; k < n; k++) {
            double age = n - 1 - k;
            double ps = -age / (fps * T); ps -= floor(ps);
            hist[std::min(NB - 1, (int)(ps * NB))] += xp[k] * pow(dec, age);
            if (age < fps * 3.0) histS[std::min(NB - 1, (int)(ps * NB))] += xp[k] * exp(-age / (fps * 3.0));
        }
        psi = 0; histReady = true; Tref = T;
    }

    // period (s) from a lag peak: sub-frame refine, harmonics, then prefer the faster pulse
    double estimate(int bl, int Lm, int W) {
        double P = refinePeak(bl), n = 1;
        for (int m = 2; m <= 3; m++) {
            if (m * bl + m > Lm) continue;
            int k = m * bl - m; for (int i = m * bl - m; i <= m * bl + m; i++) if (acb[i] > acb[k]) k = i;
            if (acb[k] > 0) { P += refinePeak(k) / m; n++; }
        }
        P /= n;
        // prefer the faster pulse when in-between hits are nearly as strong
        while (P / 2 >= minLag - 0.5) {
            double best = -1, bphi = 0; std::vector<double> S;
            for (double ph = 0; ph < P; ph += 1.0) { double s = comb(xp, W, P, ph); S.push_back(s); if (s > best) { best = s; bphi = ph; } }
            std::vector<double> srt = S; std::nth_element(srt.begin(), srt.begin() + srt.size() / 2, srt.end());
            double base = srt[srt.size() / 2];
            double pm = bphi + P / 2; if (pm >= P) pm -= P;
            double r = (comb(xp, W, P, pm) - base) / (best - base + 1e-9);
            double fb = fps * 60.0 / (P / 2);
            if (fb > 240) {
                // very fast candidates (terror, speedcore): halftime hats fool the comb, so ask the snare/body band instead
                // averaged over several seconds: in a single 6 s window halftime hats can look like the real pulse
                double aS = midAC(P), aF = midAC(P / 2), ratio = aS > 0.05 ? aF / aS : 0;
                if (fabs(P / fvLag - 1) > 0.04) { fvLag = P; fvEma = ratio; } else fvEma += 0.05 * (ratio - fvEma);
                if (!(fvEma >= 0.75) && r < 0.92) break;
            } else if (r < fasterRatio) break;
            P /= 2;
        }
        return P / fps;
    }
    // restart the tempo memory from the current window, keeping its overall scale
    void refreshMemory(int nl, int jf, double sF) {
        double mx = 0; for (double v : sacc) mx = std::max(mx, v);
        double fm = std::max(1e-9, sF * pr[jf]);
        for (int i = 0; i < nl; i++) sacc[i] = std::max(0.0, sc[i]) * pr[i] * (mx / fm);
    }
    // normalised autocorrelation of the 150-3000 Hz onset curve at a fractional lag
    double mMean = 0, mE = 0, fvLag = 1, fvEma = 0;
    void midStats(int W) { mMean = 0; for (int i = 0; i < W; i++) mMean += xm[i]; mMean /= std::max(1, W); mE = 0; for (int i = 0; i < W; i++) mE += (xm[i] - mMean) * (xm[i] - mMean); }
    double kMean = 0, kE = 0;
    static void stats(const std::vector<float>& x, double& m, double& e) { int W = (int)x.size(); m = 0; for (float v : x) m += v; m /= std::max(1, W); e = 0; for (float v : x) e += (v - m) * (v - m); }
    static double acAt(const std::vector<float>& x, double m, double e, double lag) {
        int W = (int)x.size(), l = (int)lag; double f = lag - l; if (l + 1 >= W || e <= 1e-12) return 0;
        double a = 0, b = 0;
        for (int i = l + 1; i < W; i++) { double y = x[i] - m; a += y * (x[i - l] - m); b += y * (x[i - l - 1] - m); }
        return ((1 - f) * a + f * b) / e * (double)W / (W - l);
    }
    // 3:2 / 4:3 neighbours of the current tempo (breakcore triplet rolls, polyrhythmic fills) only win if the kick agrees.
    // The kick's periodicity counts both the pulse itself and its double (kick on every other beat).
    bool polyOk(double Tfrom, double Tto) const {
        double r = Tto / Tfrom; bool poly = false;
        for (double q : {2.0 / 3, 3.0 / 2, 4.0 / 3, 3.0 / 4}) if (fabs(r / q - 1) < 0.03) poly = true;
        if (!poly) return true;
        auto kick = [&](double Tq) { double l = fps * Tq; return std::max(acAt(xk, kMean, kE, l), acAt(xk, kMean, kE, 2 * l)); };
        auto mid = [&](double Tq) { double l = fps * Tq; return std::max(midAC(l), midAC(2 * l)); };
        double kt = kick(Tto);   // no kick at all (a break) is no evidence; a real 3:4 section moves kick and snares together
        return kt >= 0.15 && kt >= 1.3 * kick(Tfrom) && mid(Tto) >= mid(Tfrom);
    }
    double midAC(double lag) const {
        int W = (int)xm.size(), l = (int)lag; double f = lag - l; if (l + 1 >= W || mE <= 1e-12) return 0;
        double a = 0, b = 0;
        for (int i = l + 1; i < W; i++) { double y = xm[i] - mMean; a += y * (xm[i - l] - mMean); b += y * (xm[i - l - 1] - mMean); }
        return ((1 - f) * a + f * b) / mE * (double)W / (W - l);
    }
    // how well the autocorrelation still supports tempo T (best raw score near its lag)
    double support(double Tq) const {
        double cl = Tq * fps; while (cl < aLo) cl *= 2;
        if (cl > aHi) return 0;
        int c0 = (int)lround(cl); double b = -1e9;
        for (int k = std::max(aLo, c0 - 2); k <= std::min(aHi, c0 + 2); k++) b = std::max(b, sc[k - aLo]);
        return b;
    }

    void analyze() {
        if (count < fps * 2.5) return;
        int W = std::min(count, (int)(fps * winSec));
        xb.resize(W); xp.resize(W); xm.resize(W); xk.resize(W);
        for (int i = 0; i < W; i++) { int idx = (wpos - W + i + cap) % cap; xb[i] = ot[idx]; xp[i] = op[idx]; xm[i] = om[idx]; xk[i] = ok[idx]; }
        midStats(W); stats(xk, kMean, kE);
        double mean = 0; for (int i = 0; i < W; i++) mean += xb[i]; mean /= W;
        int Lm = std::min(W - 1, (int)(3 * maxLag) + 3);
        std::vector<double> y(W); for (int i = 0; i < W; i++) y[i] = xb[i] - mean;
        double e0 = 0; for (int i = 0; i < W; i++) e0 += y[i] * y[i];
        if (e0 < 1e-9) return;
        acb.assign(Lm + 1, 0.0);
        for (int l = 0; l <= Lm; l++) { double s = 0; const double* p = y.data(); for (int i = l; i < W; i++) s += p[i] * p[i - l]; acb[l] = s / e0 * (double)W / (W - l); }

        int lo = (int)ceil(minLag), hi = std::min((int)floor(maxLag), Lm / 3);
        if (hi <= lo) return;
        int nl = hi - lo + 1; aLo = lo; aHi = hi;
        sc.assign(nl, 0.0); pr.assign(nl, 0.0);
        if ((int)sacc.size() != nl) sacc.assign(nl, 0.0);
        double dec = exp(-0.25 / tempoMemSec);
        for (int l = lo; l <= hi; l++) {
            double s = acb[l] + 0.5 * acb[2 * l] + 0.25 * acb[3 * l];
            sc[l - lo] = s;
            double bpm = fps * 60.0 / l, o = log2(bpm / priorCenter) / priorSigmaOct;
            pr[l - lo] = exp(-0.5 * o * o);
            sacc[l - lo] = sacc[l - lo] * dec + std::max(0.0, s) * pr[l - lo];
        }
        int j = 0; for (int i = 1; i < nl; i++) if (sacc[i] > sacc[j]) j = i;
        double conf = std::min(1.0, std::max(0.0, sc[j]) / 1.75);
        double Ts = estimate(lo + j, Lm, W);
        // fresh view of the current window only (no long memory)
        int jf = 0; for (int i = 1; i < nl; i++) if (std::max(0.0, sc[i]) * pr[i] > std::max(0.0, sc[jf]) * pr[jf]) jf = i;
        double confF = std::min(1.0, std::max(0.0, sc[jf]) / 1.75);

        double TsF = (jf == j) ? Ts : estimate(lo + jf, Lm, W);
        double sF = std::max(0.0, sc[jf]);
        // the long memory can lag behind a new track: if the fresh window keeps a steady, different answer, restart it
        bool freshOnT = locked && confF >= lockConf && fabs(TsF / T - 1) < 0.04;
        if (locked && confF >= lockConf && fabs(Ts / TsF - 1) > 0.02 && (freshOnT || support(Ts) < changeSupport * sF) && (stale == 0 || fabs(TsF / staleT - 1) < 0.01)) {
            if (stale == 0) staleT = TsF;
            if (++stale >= 10 && polyOk(T, TsF)) { staleRefreshes++; refreshMemory(nl, jf, sF); j = jf; Ts = TsF; conf = confF; stale = 0; }
        } else stale = 0;
        if (freshOnT) conf = std::max(conf, confF);
        lastConf = conf; lastCandBpm = 60.0 / Ts;
        double now = envT[(wpos - 1 + cap) % cap];

        // breakdown detection (only affects the face)
        if (conf < lowConfThr) { highCnt = 0; if (++lowCnt >= 6 && !lowConf) { lowConf = true; lowSince = now; } }
        else { lowCnt = 0; if (lowConf && ++highCnt >= 2) lowConf = false; }

        if (!locked) {
            if (conf >= lockConf && candT > 0 && fabs(Ts / candT - 1) < 0.04) agree++;
            else agree = conf >= lockConf ? 1 : 0;
            candT = Ts;
            if (agree >= 3) { locked = true; T = Ts; mismatch = 0; backfill(W); }
        } else {
            // track change: the fresh window keeps voting for another tempo and the current one has lost its support
            bool vote = confF >= lockConf && fabs(TsF / T - 1) > 0.04 && support(T) < changeSupport * sF;
            if (vote) { if (chg > 0 && fabs(TsF / chgT - 1) < 0.04) chg++; else chg = 1; chgT = TsF; } else chg = 0;
            if (chg >= changeCount && polyOk(T, TsF)) {
                T = TsF; chg = 0; mismatch = 0; trackChanges++;
                // forget the old track: restart the tempo memory from the fresh window, phase from the last 2.5 s
                refreshMemory(nl, jf, sF);
                backfill(std::min(W, (int)(fps * 2.5)));
            } else if (conf >= lockConf * 0.8) {
                if (fabs(Ts / T - 1) < 0.05) { mismatch = 0; T += 0.25 * (Ts - T); }
                else {
                    if (mismatch > 0 && fabs(Ts / mmT - 1) < 0.04) mismatch++; else mismatch = 1;
                    mmT = Ts;
                    if (mismatch >= switchCount && polyOk(T, Ts)) { T = Ts; mismatch = 0; backfill(W); }
                }
            }
            // small tempo moves (174 -> 180) never trip a change: follow the fresh estimate gently
            if (chg == 0 && confF >= lockConf && fabs(TsF / T - 1) < 0.05) T += 0.15 * (TsF - T);
            // tempo drifted away from what the beat histogram was built with -> rebuild it from recent audio
            if (fabs(T / Tref - 1) > 0.02) backfill(std::min(W, (int)(fps * 3.0)));
        }
        if (locked && histReady) phaseCheck(W, now);
        if (locked && histReady) {
            double peakS, meanS;
            int bb = peakBin(hist, nullptr, nullptr), bs = peakBin(histS, &peakS, &meanS);
            double tgt = (bb + 0.5) / NB, tgtS = (bs + 0.5) / NB;
            // same tempo, new beat position (next track at the same BPM): the last 3 s disagree with the long memory
            double dj = tgtS - theta; dj -= floor(dj + 0.5);
            double dq = tgtS - phJumpTgt; dq -= floor(dq + 0.5);
            // a clear peak switches fast (2 s); a flat one (dense hardcore) has to hold steady for 3.5 s
            bool clear = peakS > 1.8 * meanS, weak = peakS > 1.05 * meanS;
            if (fabs(dj) > 0.12 && (clear || weak) && (phJump == 0 || fabs(dq) < 0.05)) {
                if (phJump == 0) phJumpTgt = tgtS;
                if (!clear) phWeak++;
                if (++phJump >= (phWeak ? 14 : 8)) { phaseJumps++; phWeak = 0;
                    double a = 0, b = 0; for (int i = 0; i < NB; i++) { a += hist[i]; b += histS[i]; }
                    for (int i = 0; i < NB; i++) hist[i] = histS[i] * (a / std::max(1e-9, b));
                    theta = tgtS; tgt = tgtS; phJump = 0;
                }
            } else { phJump = 0; phWeak = 0; }
            double d = tgt - theta; d -= floor(d + 0.5);
            if (thetaFresh) { theta = tgt; thetaFresh = false; }
            else { theta += thetaGain * d; theta -= floor(theta); }
        }
    }
};
