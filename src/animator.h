// Turns tracker status into a pose at a given moment: nods on the predicted beat grid, blinks, build-up shiver,
// drop face, plus tempo easter eggs (sweat at 270+, smoke at 295+, falling asleep at night).
// Portable: used by every guy in the Windows app and by the web demo.
#pragma once
#include <cmath>
#include <algorithm>
#include "tracker.h"
#include "sprites.h"

struct AnimOptions {
    double latency = 0.010;   // nod offset, seconds
    bool moves = true;        // deep nods / drop face / build-up shiver
    bool eggs = true;         // sweat, smoke, sleeping at night
    int hat = HAT_NONE;
    bool night = false;       // local time is 0:00-6:00
};

class Animator {
public:
    // returns the pose for 'now' and writes when it will change next (seconds, same clock)
    Pose step(const GuyStatus& st, double now, const AnimOptions& o, double* wakeAt) {
        Pose p; p.hat = o.hat;
        double wake = now + 0.03;
        switch (st.mode) {
        case MODE_SILENCE: case MODE_AWAKE: {
            bool sil = st.mode == MODE_SILENCE;
            if (sil && o.eggs && o.night) {            // asleep: eyes shut, z's drifting up
                p.frame = F_SIL_BLINK; p.fx = FX_ZZZ;
                p.phase = (int)floor(now / 0.8) % 3; wake = (floor(now / 0.8) + 1) * 0.8;
                break;
            }
            if (now >= nextBlink + 0.14) { rnd = rnd * 1103515245u + 12345u; nextBlink = now + 2.5 + (rnd >> 16) % 4000 / 1000.0; }
            bool blink = now >= nextBlink;
            p.frame = sil ? (blink ? F_SIL_BLINK : F_SIL_OPEN) : (blink ? F_AWAKE_BLINK : F_AWAKE_OPEN);
            wake = blink ? nextBlink + 0.14 : nextBlink; break;
        }
        case MODE_LISTEN: p.frame = F_LISTEN; break;
        case MODE_PAUSE: p.frame = F_PAUSE; break;
        case MODE_NOD: {
            double T = st.period, t = now - (st.gridT0 + o.latency);
            double kb = floor(t / T), ph = t - kb * T;
            int inten = o.moves ? st.intensity : 1; bool drop = o.moves && st.drop, fresh = drop && st.dropFresh;
            if (drop) inten = 2;
            // loud: deeper nod held a bit longer; quiet: nod on every other beat
            bool skip = inten == 0 && ((st.beatIdx + (long long)kb) & 1);
            double down = inten == 2 ? std::min(0.35 * T, 0.13) : std::min(0.3 * T, 0.12);
            p.frame = drop ? F_DROP : inten == 2 ? F_LOUD : F_LISTEN;
            if (o.moves && st.build && !drop) {
                // build-up: stops nodding, eyes wide, shivers on the 16ths
                double q = T / 4, kq = floor(t / q);
                p.frame = F_BUILD; p.y = Y_REST; p.x = ((long long)kq & 1) ? 1 : 0;
                wake = now + (q - (t - kq * q));
                break;
            }
            // above ~220 BPM a 2-cell nod 4 times a second turns into jitter: keep it to one cell, the face carries the drop
            if (ph < down && !skip) { p.y = (inten == 2 && T >= 0.27) ? Y_LOUD : Y_NOD; wake = now + (down - ph); }
            else { p.y = fresh ? Y_JUMP : Y_REST; wake = now + (ph < down ? down - ph : T - ph); }
            if (o.eggs) {
                double bpm = 60.0 / T;
                if (bpm >= 295) p.fx |= FX_SMOKE; else if (bpm >= 270) p.fx |= FX_SWEAT;
                if (p.fx) {                            // effects tick on their own ~6 Hz clock
                    const double tick = 1.0 / 6;
                    p.phase = (int)floor(now / tick) % 6;
                    wake = std::min(wake, (floor(now / tick) + 1) * tick);
                }
            }
            break;
        }
        }
        if (wakeAt) *wakeAt = wake;
        return p;
    }
    // voice apps: mouth follows the level instead of nodding
    Pose talk(double level, double now, const AnimOptions& o, double* wakeAt) {
        Pose p; p.hat = o.hat;
        env = std::max(level, env * 0.8);
        p.frame = env > 0.02 && fmod(now, 0.16) < 0.09 ? F_TALK : F_AWAKE_OPEN;
        if (wakeAt) *wakeAt = now + 0.03;
        return p;
    }
private:
    double nextBlink = 3, env = 0; unsigned rnd = 12345;
};
