// Shared by the demo and the skin editor: the wasm module, a generated beat and a player that feeds the tracker.
export const RU = /^ru/i.test(navigator.language || '');

export async function loadWasm() {
  const r = await WebAssembly.instantiateStreaming(fetch('guy.wasm'), { wasi_snapshot_preview1: new Proxy({}, { get: () => () => 0 }) });
  const W = r.instance.exports; if (W._initialize) W._initialize(); W.hg_init(48000);
  return W;
}
export function cstr(W, ptr, len) { return new TextDecoder().decode(new Uint8Array(W.memory.buffer, ptr, len)); }

// generated hardcore-ish loop at 200 BPM: groove, a break with a snare roll building up, then the drop
export function makeBeat(ctx, len = 44) {
  const rate = ctx.sampleRate, bpm = 200, beat = 60 / bpm, n = Math.floor(len * rate);
  const buf = ctx.createBuffer(1, n, rate), d = buf.getChannelData(0);
  let seed = 7; const rnd = () => (seed = (seed * 1103515245 + 12345) >>> 0) / 4294967296 - 0.5;
  const hit = (t0, f) => { const i0 = Math.floor(t0 * rate); for (let i = 0; i < rate * 0.5 && i0 + i < n; i++) { const v = f(i / rate); if (v === null) break; d[i0 + i] += v; } };
  const kick = (t, hard) => { let ph = 0; hit(t, x => { if (x > 0.32) return null; const fr = 48 + 260 * Math.exp(-x * 38); ph += 2 * Math.PI * fr / rate; const s = Math.sin(ph) * Math.exp(-x * (hard ? 7 : 11)); return Math.tanh(s * (hard ? 5 : 2.2)) * 0.55; }); };
  const hat = (t, a) => hit(t, x => x > 0.05 ? null : rnd() * a * Math.exp(-x * 90));
  const snare = (t, a) => hit(t, x => x > 0.16 ? null : (rnd() * 0.9 + Math.sin(2 * Math.PI * 190 * x) * 0.4) * a * Math.exp(-x * 24));
  const B = Math.round(len / beat);
  for (let b = 0; b < B; b++) {
    const t = b * beat;
    if (t < 6) { kick(t, false); hat(t + beat / 2, 0.18); }
    else if (t < 18) { kick(t, false); hat(t + beat / 2, 0.25); if (b % 2) snare(t, 0.3); }
    else if (t < 26) {
      hat(t + beat / 2, 0.1);
      const k = t < 22 ? 1 : t < 24 ? 2 : 4;
      for (let j = 0; j < k; j++) snare(t + j * beat / k, 0.18 + 0.2 * (t - 18) / 8);
    } else { kick(t, true); hat(t + beat / 2, 0.3); if (b % 2) snare(t, 0.35); }
  }
  for (let i = 0; i < n; i++) { const t = i / rate; d[i] += (t > 18 && t < 26 ? 0.04 : 0) * Math.sin(2 * Math.PI * 220 * t) * Math.sin(Math.PI * (t - 18) / 8); d[i] = Math.max(-1, Math.min(1, d[i])); }
  return buf;
}

// Plays an AudioBuffer and feeds it to the tracker in step with what you hear.
// now() keeps running while paused, so the guy notices the silence like the real app does.
export class Player {
  constructor(W) { this.W = W; this.ctx = null; this.src = null; this.buf = null; this.mono = null; this.sr = 48000; this.dur = 0;
    this.playing = false; this.offset = 0; this.startCtx = 0; this.fed = 0; this.pausedV = 0; this.pausedR = performance.now() / 1000; this.loop = false; this.onstate = () => {}; }
  ensure() { if (!this.ctx) this.ctx = new (window.AudioContext || window.webkitAudioContext)(); return this.ctx; }
  pos() { return this.playing ? this.ctx.currentTime - this.startCtx + this.offset : this.offset; }
  now() { return this.playing ? this.pos() : this.pausedV + performance.now() / 1000 - this.pausedR; }
  latency() { return this.ctx ? (this.ctx.outputLatency || 0) + (this.ctx.baseLatency || 0) : 0; }
  markPaused() { this.pausedV = this.now(); this.pausedR = performance.now() / 1000; }
  stopSrc() { if (this.src) { this.src.onended = null; try { this.src.stop(); } catch (e) {} this.src.disconnect(); this.src = null; } }
  async load(buf) {
    this.ensure(); await this.ctx.resume();
    this.buf = buf; this.sr = buf.sampleRate; this.dur = buf.duration;
    this.mono = new Float32Array(buf.length);
    for (let c = 0; c < buf.numberOfChannels; c++) { const d = buf.getChannelData(c); for (let i = 0; i < d.length; i++) this.mono[i] += d[i] / buf.numberOfChannels; }
    this.startAt(0);
  }
  startAt(t) {
    this.stopSrc();
    this.offset = Math.max(0, Math.min(t, this.dur - 0.05));
    const src = this.ctx.createBufferSource(); src.buffer = this.buf; src.connect(this.ctx.destination);
    src.onended = () => {
      if (!this.playing || this.pos() < this.dur - 0.1) return;
      if (this.loop) { this.startAt(0); return; }
      this.markPaused(); this.playing = false; this.offset = 0; this.onstate(false);
    };
    this.src = src; this.startCtx = this.ctx.currentTime; src.start(0, this.offset); this.playing = true;
    this.W.hg_init(this.sr); this.fed = Math.floor(this.offset * this.sr);   // fresh tracker after a jump, like a real seek
    this.onstate(true);
  }
  pause() { if (!this.playing) return; this.markPaused(); this.offset = this.pos(); this.stopSrc(); this.playing = false; this.onstate(false); }
  toggle() { if (!this.buf) return; if (this.playing) this.pause(); else this.startAt(this.offset); }
  // feed new audio to the tracker and update it; returns the mode
  tick() {
    const W = this.W, now = this.now();
    if (this.playing && this.mono) {
      const upto = Math.min(this.mono.length, Math.floor(this.pos() * this.sr));
      while (this.fed < upto) { const k = Math.min(4800, upto - this.fed); const p = W.hg_buffer(k); new Float32Array(W.memory.buffer, p, k).set(this.mono.subarray(this.fed, this.fed + k)); W.hg_feed(k, this.fed / this.sr); this.fed += k; }
    }
    return W.hg_update(now);
  }
}

export function blit(W, g, ptr, s) {
  const w = 16 * s, h = 18 * s;
  const img = new ImageData(new Uint8ClampedArray(W.memory.buffer, ptr, w * h * 4).slice(), w, h);
  g.clearRect(0, 0, w, h); g.putImageData(img, 0, 0);
}
export function hatByDate(d = new Date()) {
  const m = d.getMonth() + 1, day = d.getDate();
  if ((m === 10 && day >= 25) || (m === 11 && day <= 1)) return 1;
  if ((m === 12 && day >= 20) || (m === 1 && day <= 10)) return 2;
  return 0;
}
