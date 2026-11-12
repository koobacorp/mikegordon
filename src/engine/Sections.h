// MIKE GORDON — the sections: Tone (amp voices), Filter, Pitch, Motion, Texture, Space, Width + Ducking.
#pragma once
#include "Dsp.h"
#include "Params.h"
#include <array>

namespace mg {

constexpr int kSub = 32;   // control-rate sub-block

struct Ctx {
    double sr = 48000, bpm = 120, ppq = 0; bool playing = false, hasPpq = false;
    const float* p = nullptr;   // plain parameter values
    float v(int i) const { return p[i]; }
    bool on(int i) const { return p[i] >= 0.5f; }
    int ch(int i) const { return (int)std::lround(p[i]); }
    /// Hz of a synced division at the current tempo
    double divHz(int div) const { return bpm / 60.0 / kDivBeats[std::clamp(div, 0, kNumDivs - 1)]; }
    bool locked() const { return playing && hasPpq; }
};

/// Shared helper for a synced/free LFO: call once per sub-block, then step() per sample.
struct SyncLfo {
    Lfo l; double inc = 0;
    void begin(const Ctx& c, bool sync, int div, double freeHz) {
        if (sync) { double beats = kDivBeats[std::clamp(div, 0, kNumDivs - 1)]; inc = c.divHz(div) / c.sr; if (c.locked()) l.lockTo(c.ppq, beats); }
        else inc = freeHz / c.sr;
    }
    inline double step() { double p = l.ph; l.ph += inc; l.ph -= std::floor(l.ph); return p; }
};

/// Type crossfader: when the chosen type changes, the old and new types run together for a short equal-gain fade.
/// The new type first runs silently for a short "prefill" so its delay lines hold real signal, then fades in.
struct TypeFade {
    int cur = -1, prev = -1, warm = 0, pre = 0; Ramp r;
    void setTime(double sr, double s, double prefill = 0) { r.setTime(sr, s); pre = (int)(prefill * sr); }
    /// returns true if `t` became the new type (caller should reset that type's state)
    bool update(int t) { if (cur < 0) { cur = t; r.v = 1; r.up = true; return true; } if (t != cur) { prev = cur; cur = t; r.v = 0; r.up = true; warm = 0; return true; } return false; }
    bool fading() const { return prev >= 0 && r.v < 1; }
    inline void advance() { if (warm < pre) ++warm; else r.next(); }
};

// =====================================================================================================================
// Spring reverb (used inside the amp and by the Space section): delay loop + dispersive allpass chain ("drip"/chirp).
struct Spring {
    static constexpr int kAp = 12;
    double sr = 48000; Delay loop[2]; Allpass1 ap[2][kAp]; OnePole damp[2]; Biquad hp[2], lp[2]; float fb[2] = {0, 0}, g = 0.5f; float D[2] = {1, 1};
    void prepare(double s) {
        sr = s; for (int c = 0; c < 2; ++c) { loop[c].init((int)(0.06 * sr)); D[c] = (float)((c ? 0.0413 : 0.0337) * sr); hp[c].highpass(sr, 160, 0.7); lp[c].lowpass(sr, 4800, 0.7);
            for (int k = 0; k < kAp; ++k) ap[c][k].a = 0.62f - 0.015f * (float)k; }
        set(2.0, 0.5, false); reset();
    }
    void reset() { for (int c = 0; c < 2; ++c) { loop[c].reset(); for (auto& a : ap[c]) a.reset(); damp[c].reset(); hp[c].reset(); lp[c].reset(); fb[c] = 0; } }
    /// rt60 seconds, tone 0..1 (bright), freeze holds the loop
    bool frozen = false; float fz = 0;
    /// freeze01 crossfades smoothly into a lossless held loop (1 = fully frozen)
    void set(double rt60, double tone, float freeze01) {
        fz = freeze01; frozen = fz >= 0.9999f; double d = D[0] / sr; float g0 = (float)std::pow(10.0, -3.0 * d / std::max(0.05, rt60)); g = g0 + fz * (1.f - g0);
        for (int c = 0; c < 2; ++c) damp[c].setLP(sr, 1800 + tone * 9000);
    }
    /// gain the loop applies per round trip (for tests)
    float loopGain() const { return g; }
    inline void process(float inL, float inR, float& oL, float& oR) {
        float in[2] = {inL, inR}, out[2];
        for (int c = 0; c < 2; ++c) {
            float v = (frozen ? 0.f : in[c] * (1 - fz)) + g * fb[c]; v = frozen ? v : softClip(v * 0.5f) * 2.f; loop[c].push(v);
            float y = loop[c].at((int)D[c]); for (auto& a : ap[c]) y = a.process(y);
            float dmp = damp[c].lp(y); fb[c] = frozen ? y : dmp + fz * (y - dmp);
            out[c] = lp[c].process(hp[c].process(y));
        }
        oL = out[0]; oR = out[1];
    }
};

// Plate reverb: figure-of-eight tank topology from Dattorro's 1997 paper (lengths at 29761 Hz, scaled to the host rate).
struct Plate {
    double sr = 48000, sc = 1; OnePole bw; DiffuserAP inAp[4], tankAp1[2], tankAp2[2]; Delay del1[2], del2[2]; OnePole damp[2]; float tankOut[2] = {0, 0};
    float decay = 0.5f, dampF = 0.3f; Lfo lfo; bool frozen = false;
    int L(int n) const { return std::max(1, (int)std::lround(n * sc)); }
    void prepare(double s) {
        sr = s; sc = sr / 29761.0; const int in[4] = {142, 107, 379, 277};
        for (int i = 0; i < 4; ++i) { inAp[i].init(L(in[i]) + 2); inAp[i].len = (float)L(in[i]); }
        const int a1[2] = {672, 908}, d1[2] = {4453, 4217}, a2[2] = {1800, 2656}, d2[2] = {3720, 3163};
        for (int t = 0; t < 2; ++t) { tankAp1[t].init(L(a1[t] + 40)); tankAp1[t].len = (float)L(a1[t]); del1[t].init(L(d1[t]) + 2); tankAp2[t].init(L(a2[t]) + 2); tankAp2[t].len = (float)L(a2[t]); del2[t].init(L(d2[t]) + 2); }
        bw.a = 0.0005f; reset();
    }
    void reset() { for (auto& a : inAp) a.reset(); for (int t = 0; t < 2; ++t) { tankAp1[t].reset(); tankAp2[t].reset(); del1[t].reset(); del2[t].reset(); damp[t].reset(); tankOut[t] = 0; } bw.reset(); }
    float fz = 0;
    void set(double decay01, double tone, float freeze01) {
        fz = freeze01; frozen = fz >= 0.9999f; float d0 = (float)(0.2 + 0.73 * decay01); decay = d0 + fz * (1.f - d0);
        float d = (float)(0.05 + (1 - tone) * 0.65) * (1 - fz);
        for (auto& dm : damp) dm.a = d;
    }
    inline void process(float inL, float inR, float& oL, float& oR) {
        float x = frozen ? 0.f : 0.5f * (inL + inR) * (1 - fz); x = bw.lp(x);
        x = inAp[0].process(x, 0.75f); x = inAp[1].process(x, 0.75f); x = inAp[2].process(x, 0.625f); x = inAp[3].process(x, 0.625f);
        lfo.advanceFree(0.9, sr); float m = frozen ? 0.f : (float)(Lfo::sine(lfo.ph) * 12 * sc) * (1 - fz);
        for (int t = 0; t < 2; ++t) {
            float v = x + decay * tankOut[1 - t]; if (!frozen) v = softClip(v * 0.5f) * 2.f;
            v = tankAp1[t].process(v, -0.7f, t ? -m : m);
            del1[t].push(v); v = del1[t].at(L(t ? 4217 : 4453) - 1);
            v = damp[t].lp(v) * decay;
            v = tankAp2[t].process(v, 0.5f);
            del2[t].push(v); tankOut[t] = del2[t].at(L(t ? 3163 : 3720) - 1);
        }
        // output taps
        auto& A1 = del1[0]; auto& A2 = tankAp2[0].d; auto& A3 = del2[0]; auto& B1 = del1[1]; auto& B2 = tankAp2[1].d; auto& B3 = del2[1];
        oL = 0.6f * (B1.at(L(266)) + B1.at(L(2974)) - B2.at(L(1913)) + B3.at(L(1996)) - A1.at(L(1990)) - A2.at(L(187)) - A3.at(L(1066)));
        oR = 0.6f * (A1.at(L(353)) + A1.at(L(3627)) - A2.at(L(1228)) + A3.at(L(2673)) - B1.at(L(2111)) - B2.at(L(335)) - B3.at(L(121)));
    }
};

// Tape echo: saturating feedback loop with tone filter, gentle wow; optional ping-pong.
struct Echo {
    double sr = 48000; Delay d[2]; Smooth time; Biquad lp[2], hp[2]; float fb = 0.4f; bool frozen = false, ping = false; Lfo wow; float state[2] = {0, 0};
    void prepare(double s) { sr = s; for (auto& x : d) x.init((int)(2.6 * sr)); time.setTime(sr, 0.12); time.reset((float)(0.375 * sr)); reset(); }
    bool first = true;
    void reset() { for (int c = 0; c < 2; ++c) { d[c].reset(); lp[c].reset(); hp[c].reset(); state[c] = 0; } first = true; }
    float fz = 0;
    void set(double seconds, double fb01, double tone, float freeze01, bool pp) {
        float tt = (float)std::clamp(seconds * sr, 8.0, 2.5 * sr); if (first) { time.reset(tt); first = false; } time.target(tt); fz = freeze01; frozen = fz >= 0.9999f; ping = pp; float f0 = (float)(0.95 * fb01); fb = f0 + fz * (1.f - f0);
        for (int c = 0; c < 2; ++c) { lp[c].lowpass(sr, 1800 + tone * 10000, 0.6); hp[c].highpass(sr, 90, 0.6); }
    }
    inline void process(float inL, float inR, float& oL, float& oR) {
        float t = time.next(); wow.advanceFree(0.35, sr); float w = frozen ? 0.f : (float)(Lfo::sine(wow.ph) * 0.0004 * sr) * (1 - fz);
        float y[2];
        for (int c = 0; c < 2; ++c) y[c] = frozen ? d[c].at((int)std::lround(t) - 1) : d[c].read(t - 1 + w);
        float in[2] = {inL, inR};
        if (ping) { float mono = 0.5f * (inL + inR); in[0] = mono; in[1] = 0; }
        for (int c = 0; c < 2; ++c) {
            float src = ping ? y[1 - c] : y[c];
            float v;
            if (frozen) v = src; else { float f = hp[c].process(lp[c].process(src)); f = f + fz * (src - f); v = in[c] * (1 - fz) + fb * (softClip(f * 0.8f) * 1.25f * (1 - fz) + fz * src); }
            d[c].push(flush(v));
        }
        oL = y[0]; oR = y[1];
    }
};

// =====================================================================================================================
// TONE: four original clean amp voices.
struct VoiceSpec { float headroom, bias, midHz; };
inline constexpr VoiceSpec kVoiceSpec[4] = {{0.35f, 0.0f, 800}, {0.6f, 0.05f, 500}, {1.0f, 0.2f, 1000}, {0.55f, 0.08f, 650}};

struct Amp {
    double sr = 48000;
    struct Ch { Biquad bright, sparkle, voice1, voice2, bass, mid, treble; AdaaTanh sh; OnePole dc; Delay fx; void reset() { bright.reset(); sparkle.reset(); voice1.reset(); voice2.reset(); bass.reset(); mid.reset(); treble.reset(); sh.reset(); dc.reset(); fx.reset(); } };
    Ch ch[4][2]; Lfo lfo[4]; TypeFade tf; Spring spring; Smooth drive, vfx, verb, brightAmt; bool springLive = false; float last[5] = {-99, -99, -99, -99, -99};
    void prepare(double s) {
        sr = s; for (auto& v : ch) for (auto& c : v) { c.fx.init((int)(0.02 * sr)); c.dc.setLP(sr, 8); }
        spring.prepare(sr); spring.set(1.6, 0.45, false);
        for (auto* sm : {&drive, &vfx, &verb, &brightAmt}) sm->setTime(sr, 0.02);
        tf.setTime(sr, 0.03); reset();
    }
    void reset() { for (auto& v : ch) for (auto& c : v) c.reset(); spring.reset(); tf = TypeFade{}; tf.setTime(sr, 0.03, 0.012); for (auto& l : last) l = -99; drive.reset(-1); }
    void setVoiceFilters(int v, const Ctx& c) {
        for (int k = 0; k < 2; ++k) {
            auto& q = ch[v][k];
            q.bright.highShelf(sr, 2500, 6.0, 0.8);
            switch (v) {
                case 0: q.voice1.highShelf(sr, 6000, 2.0); q.voice2.peak(sr, 120, 0.7, 1.0); break;
                case 1: q.voice1.peak(sr, 500, 0.7, -3.5); q.voice2.lowShelf(sr, 100, 2.0); break;
                case 2: q.voice1.highpass(sr, 80, 0.7); q.voice2.peak(sr, 2400, 0.9, 3.5); break;
                default: q.voice1.lowpass(sr, 7500, 0.6); q.voice2.peak(sr, 250, 0.8, 1.5); break;
            }
            q.bass.lowShelf(sr, 100, c.v(P_AMP_BASS)); q.mid.peak(sr, kVoiceSpec[v].midHz, 0.8, c.v(P_AMP_MID)); q.treble.highShelf(sr, 3500, c.v(P_AMP_TREBLE));
            q.sparkle.peak(sr, 3000, 0.8, v == 2 ? 8.0 * c.v(P_AMP_VFX) * 0.01 : 0.0);
        }
    }
    inline float voiceSample(int v, int k, float x, float G, float fx, double ph) {
        auto& q = ch[v][k]; const auto& sp = kVoiceSpec[v];
        float b = brightAmt.v; if (b > 0) x = x + b * (q.bright.process(x) - x);
        if (v == 2) x = q.sparkle.process(x);
        float tb = std::tanh(sp.bias), norm = 1.f / (sp.headroom * (1 - tb * tb));
        float y = (q.sh.process(sp.headroom * G * x + sp.bias) - tb) * norm / std::sqrt(G);
        y = q.dc.hp(y);
        y = q.voice2.process(q.voice1.process(y));
        y = q.treble.process(q.mid.process(q.bass.process(y)));
        q.fx.push(y);
        switch (v) {
            case 0: { float d = (float)((0.007 + 0.0025 * fx * Lfo::sine(ph + (k ? 0.5 : 0.0))) * sr); return y * (1 - 0.3f * fx) + 0.75f * fx * q.fx.read(d); }
            case 1: { float g = 1 - 0.85f * fx * (0.5f - 0.5f * (float)std::cos(2 * kPi * ph)); return y * g; }
            case 3: { float A = 0.0025f * fx * (float)sr; float d = 0.00025f * (float)sr + A * (1 + Lfo::sine(ph + (k ? 0.25 : 0.0))); return q.fx.read(d); }
            default: return y;
        }
    }
    void process(float* L, float* R, int n, const Ctx& c) {
        int voice = std::clamp(c.ch(P_AMP_VOICE), 0, 3);
        if (tf.update(voice)) ch[voice][0].reset(), ch[voice][1].reset(), last[0] = -99;
        float cur[5] = {c.v(P_AMP_BASS), c.v(P_AMP_MID), c.v(P_AMP_TREBLE), c.v(P_AMP_VFX), (float)voice};
        if (std::memcmp(cur, last, sizeof cur) != 0) { setVoiceFilters(voice, c); if (tf.fading()) setVoiceFilters(tf.prev, c); std::memcpy(last, cur, sizeof cur); }
        float dr = c.v(P_AMP_DRIVE); bool fresh = drive.v < 0; drive.target(dr); vfx.target(c.v(P_AMP_VFX) * 0.01f); verb.target(c.v(P_AMP_VERB) * 0.01f); brightAmt.target(c.on(P_AMP_BRIGHT) ? 1.f : 0.f);
        if (fresh) { drive.reset(dr); vfx.reset(vfx.t); verb.reset(verb.t); brightAmt.reset(brightAmt.t); }
        double inc = c.v(P_AMP_VRATE) / sr;
        for (int i = 0; i < n; ++i) {
            float G = dbToGain(drive.next() * 1.8f + (voice == 2 ? 4.f * vfx.v : 0.f)); float fx = vfx.next(); brightAmt.next();
            float o[2];
            for (int k = 0; k < 2; ++k) {
                float x = k ? R[i] : L[i];
                float y = voiceSample(voice, k, x, G, fx, lfo[voice].ph);
                if (tf.fading()) { float yp = voiceSample(tf.prev, k, x, G, fx, lfo[tf.prev].ph); y = yp + tf.r.v * (y - yp); }
                o[k] = y;
            }
            lfo[voice].ph += inc; lfo[voice].ph -= std::floor(lfo[voice].ph);
            if (tf.fading()) { lfo[tf.prev].ph += inc; lfo[tf.prev].ph -= std::floor(lfo[tf.prev].ph); tf.advance(); }
            float vb = verb.next();
            if (vb == 0 && verb.settled() && springLive) { spring.reset(); springLive = false; }
            if (vb > 0 || !verb.settled()) { springLive = true; float sL, sR; spring.process(o[0], o[1], sL, sR); float dg = std::min(1.f, 2 * (1 - 0.5f * vb)), wg = vb; o[0] = o[0] * dg + sL * wg; o[1] = o[1] * dg + sR * wg; }
            L[i] = o[0]; R[i] = o[1];
        }
    }
};

// =====================================================================================================================
// FILTER: high-pass + resonant low-pass with LFO or envelope (auto-wah) movement.
struct FilterSec {
    double sr = 48000; Svf hp[2], lp[2]; EnvFollow env; SyncLfo lfo; Smooth lpOct, hpOct, q, mod; int cnt = 0;
    void prepare(double s) { sr = s; env.set(sr, 0.004, 0.15); for (auto* m : {&lpOct, &hpOct, &q}) m->setTime(sr, 0.02); mod.setTime(sr, 0.004); reset(); }
    void reset() { for (int k = 0; k < 2; ++k) { hp[k].reset(); lp[k].reset(); } env.reset(); lpOct.reset(-1000); mod.reset(0); cnt = 0; }
    static void copyCoefs(Svf& d, const Svf& s) { d.g = s.g; d.k = s.k; d.a1 = s.a1; d.a2 = s.a2; d.a3 = s.a3; }
    static float qFromRes(float res01) { return 0.7071f + res01 * res01 * 11.f; }
    void process(float* L, float* R, int n, const Ctx& c) {
        float lpo = std::log2(c.v(P_FLT_LP)), hpo = std::log2(c.v(P_FLT_HP)), qq = qFromRes(c.v(P_FLT_RES) * 0.01f);
        if (lpOct.v < -999) { lpOct.reset(lpo); hpOct.reset(hpo); q.reset(qq); cnt = 0; }
        lpOct.target(lpo); hpOct.target(hpo); q.target(qq);
        int mode = c.ch(P_FLT_MOVE); float depth = c.v(P_FLT_DEPTH) * 0.01f, sens = dbToGain(c.v(P_FLT_SENS) * 0.36f);
        lfo.begin(c, c.on(P_FLT_SYNC), c.ch(P_FLT_DIV), c.v(P_FLT_RATE));
        for (int i = 0; i < n; ++i) {
            double ph = lfo.step(); float e = env.process(0.5f * (std::abs(L[i]) + std::abs(R[i])));
            float m = mode == 1 ? depth * 3.f * Lfo::sine(ph) : mode == 2 ? depth * 4.f * std::min(1.f, e * sens) : 0.f;
            mod.target(m); float mv = mod.next(); float lo = lpOct.next(), ho = hpOct.next(), qv = q.next();
            if ((cnt++ & 3) == 0) { lp[0].set(sr, std::pow(2.0, (double)(lo + mv)), qv); hp[0].set(sr, std::pow(2.0, (double)ho), 0.7071); copyCoefs(lp[1], lp[0]); copyCoefs(hp[1], hp[0]); }
            L[i] = lp[0].lp(hp[0].hp(L[i])); R[i] = lp[1].lp(hp[1].hp(R[i]));
        }
    }
};

// =====================================================================================================================
// PITCH: octave down / up blend plus a detuned pair spread across the stereo field.
struct PitchSec {
    double sr = 48000; PitchShifter dn[2], up[2], detA, detB; Smooth gD, gU, gDet, cents, width;
    void prepare(double s) {
        sr = s; for (int k = 0; k < 2; ++k) { dn[k].init((int)(0.06 * sr)); dn[k].setWindow((float)(0.05 * sr)); up[k].init((int)(0.05 * sr)); up[k].setWindow((float)(0.04 * sr)); }
        for (auto* d : {&detA, &detB}) { d->init((int)(0.04 * sr)); d->setWindow((float)(0.03 * sr)); }
        for (auto* m : {&gD, &gU, &gDet, &cents, &width}) m->setTime(sr, 0.03); reset();
    }
    void reset() { for (int k = 0; k < 2; ++k) { dn[k].reset(); up[k].reset(); } detA.reset(); detB.reset(); detB.ph = 0.37f; gD.reset(0); gU.reset(0); gDet.reset(0); cents.reset(-1); }
    void process(float* L, float* R, int n, const Ctx& c) {
        gD.target(c.v(P_PIT_DOWN) * 0.01f); gU.target(c.v(P_PIT_UP) * 0.01f); float ct = c.v(P_PIT_DETUNE); if (cents.v < 0) { cents.reset(ct); width.reset(c.v(P_PIT_WIDTH) * 0.01f); } cents.target(ct); gDet.target(0.6f * std::min(1.f, ct / 4.f)); width.target(c.v(P_PIT_WIDTH) * 0.01f);
        for (int i = 0; i < n; ++i) {
            float d = gD.next(), u = gU.next(), g = gDet.next(), cc = cents.next(), w = width.next();
            float x[2] = {L[i], R[i]}, o[2] = {x[0], x[1]};
            for (int k = 0; k < 2; ++k) { float sd = dn[k].process(x[k], 0.5f), su = up[k].process(x[k], 2.f); o[k] += d * sd + u * su; }
            float mono = 0.5f * (x[0] + x[1]); float r = std::pow(2.f, cc / 1200.f);
            float a = detA.process(mono, r), b = detB.process(mono, 1.f / r);
            float pl = 0.5f + 0.5f * w, pr = 0.5f - 0.5f * w;
            o[0] += g * (a * pl + b * pr); o[1] += g * (a * pr + b * pl);
            L[i] = o[0]; R[i] = o[1];
        }
    }
};

// =====================================================================================================================
// MOTION: chorus, flanger, phaser, swirl (photocell-style multi-stage phase shifter), tremolo.
struct Motion {
    double sr = 48000; SyncLfo lfo; TypeFade tf; Delay dl[2]; Allpass1 ph[2][6], sw[2][4]; float fbState[2] = {0, 0}, swFb[2] = {0, 0}, flFb[2] = {0, 0};
    Smooth depth, mix, fb, spread; int ctr = 0;
    float phA[2] = {0, 0}, swA[2][4] = {};
    void prepare(double s) { sr = s; for (auto& d : dl) d.init((int)(0.05 * sr)); for (auto* m : {&depth, &mix, &fb, &spread}) m->setTime(sr, 0.02); tf.setTime(sr, 0.03, 0.03); reset(); }
    void resetType(int t) {
        for (int k = 0; k < 2; ++k) { if (t <= 1) { dl[k].reset(); flFb[k] = 0; } if (t == 2) { for (auto& a : ph[k]) a.reset(); fbState[k] = 0; } if (t == 3) { for (auto& a : sw[k]) a.reset(); swFb[k] = 0; } }
    }
    void reset() { for (int t = 0; t < 5; ++t) resetType(t); tf = TypeFade{}; tf.setTime(sr, 0.03, 0.03); depth.reset(-1); }
    inline float typeSample(int t, int k, float x, double p, float dp, float fbv) {
        switch (t) {
            case 0: { float d1 = (float)((0.012 + 0.0025 * dp * (1 + Lfo::sine(p))) * sr), d2 = (float)((0.018 + 0.0025 * dp * (1 + Lfo::sine(p + 0.33))) * sr);
                      return 0.5f * (dl[k].read(d1) + dl[k].read(d2)); }
            case 1: { float d = (float)((0.001 + 0.004 * dp * (0.5 + 0.5 * Lfo::sine(p))) * sr); float y = dl[k].read(d); flFb[k] = y * fbv; return y; }
            case 2: { float y = x + fbState[k] * std::min(fbv, 0.75f); for (auto& a : ph[k]) y = a.process(y); fbState[k] = flush(y); return y; }
            case 3: { float y = x + swFb[k] * 0.5f * fbv; for (auto& a : sw[k]) y = a.process(y); swFb[k] = flush(y); float s = Lfo::sine(p); return y * (1 - 0.1f * dp * (0.5f + 0.5f * s)); }
            default: { float g = 1 - dp * (0.5f - 0.5f * (float)std::cos(2 * kPi * p)); return x * g; }
        }
    }
    /// per-sample control: set allpass coefficients for phaser/swirl (every 4 samples)
    inline void updateCoefs(int t, int k, double p, float dp) {
        if (t == 2) { float a = Allpass1::coef(sr, 600.0 * std::pow(2.0, dp * 2.5 * Lfo::sine(p))); for (auto& ap : ph[k]) ap.a = a; }
        if (t == 3) { float s = Lfo::sine(p); float sh = s > 0 ? std::pow(s, 0.7f) : -std::pow(-s, 1.4f); static const float base[4] = {180, 420, 950, 2100};
                      for (int j = 0; j < 4; ++j) sw[k][j].a = Allpass1::coef(sr, base[j] * std::pow(2.0, dp * 2.0 * sh)); }
    }
    void process(float* L, float* R, int n, const Ctx& c) {
        int type = std::clamp(c.ch(P_MOT_TYPE), 0, 4); if (tf.update(type)) resetType(type);
        float dt = c.v(P_MOT_DEPTH) * 0.01f; bool fresh = depth.v < 0; if (fresh) { depth.reset(dt); ctr = 0; }
        depth.target(dt); mix.target(c.v(P_MOT_MIX) * 0.01f); fb.target(c.v(P_MOT_FB) * 0.01f); spread.target(c.v(P_MOT_SPREAD) * 0.01f);
        if (fresh) { mix.reset(mix.t); fb.reset(fb.t); spread.reset(spread.t); }
        lfo.begin(c, c.on(P_MOT_SYNC), c.ch(P_MOT_DIV), c.v(P_MOT_RATE));
        for (int i = 0; i < n; ++i) {
            double p = lfo.step(); float dp = depth.next(), mx = mix.next(), fbv = fb.next(), sp = spread.next();
            float x[2] = {L[i], R[i]};
            for (int k = 0; k < 2; ++k) {
                double pk = p + (k ? sp * (type == 4 ? 0.5 : 0.25) : 0.0);
                bool upd = (ctr & 3) == 0;
                if (upd) { updateCoefs(type, k, pk, dp); if (tf.fading()) updateCoefs(tf.prev, k, pk, dp); }
                bool usesDelay = type <= 1 || (tf.fading() && tf.prev <= 1);
                if (usesDelay) dl[k].push(x[k] + flFb[k] * ((type == 1 || (tf.fading() && tf.prev == 1)) ? 1.f : 0.f));
                if (!(type == 1 || (tf.fading() && tf.prev == 1))) flFb[k] = 0;
                float w = typeSample(type, k, x[k], pk, dp, fbv);
                if (tf.fading()) { float wp = typeSample(tf.prev, k, x[k], pk, dp, fbv); w = wp + tf.r.v * (w - wp); }
                float y = x[k] + mx * (w - x[k]);
                (k ? R[i] : L[i]) = y;
            }
            ++ctr; if (tf.fading()) tf.advance();
        }
    }
};

// =====================================================================================================================
// TEXTURE: tape wobble, vinyl crackle, hiss, bit crush, worn cassette tone.
struct Texture {
    double sr = 48000; Delay wob[2]; Lfo wow1, wow2, flut; OnePole wowNoise; Rng rng; Smooth wobble, crackle, hiss, crush, tape;
    Biquad tHp[2], tBump[2], tLp[2]; OnePole hissHp[2], hissLp[2]; Svf crk[2]; EnvFollow gateEnv; float held[2] = {0, 0}, acc = 1; float lastTape = -1;
    float crackleKick[2] = {0, 0};
    void prepare(double s) {
        sr = s; for (auto& d : wob) d.init((int)(0.012 * sr)); wowNoise.setLP(sr, 0.8);
        for (auto* m : {&wobble, &crackle, &hiss, &crush, &tape}) m->setTime(sr, 0.03);
        for (int k = 0; k < 2; ++k) { hissHp[k].setLP(sr, 900); hissLp[k].setLP(sr, 11000); crk[k].set(sr, 2800, 0.9); }
        gateEnv.set(sr, 0.005, 0.8); reset();
    }
    void reset() { for (int k = 0; k < 2; ++k) { wob[k].reset(); tHp[k].reset(); tBump[k].reset(); tLp[k].reset(); hissHp[k].reset(); hissLp[k].reset(); crk[k].reset(); held[k] = 0; crackleKick[k] = 0; }
                   wowNoise.reset(); gateEnv.reset(); acc = 1; lastTape = -1; wobble.reset(-1); }
    void setTape(float t) { for (int k = 0; k < 2; ++k) { tHp[k].highpass(sr, 30 + 45 * t, 0.7071); tBump[k].peak(sr, 100, 1.2, 3.0 * t); tLp[k].lowpass(sr, 18000.0 * (1 - 0.72 * t), 0.7071); } }
    void process(float* L, float* R, int n, const Ctx& c) {
        float wt = c.v(P_TEX_WOBBLE) * 0.01f; bool fresh = wobble.v < 0;
        wobble.target(wt); crackle.target(c.v(P_TEX_CRACKLE) * 0.01f); hiss.target(c.v(P_TEX_HISS) * 0.01f); crush.target(c.v(P_TEX_CRUSH) * 0.01f); tape.target(c.v(P_TEX_TAPE) * 0.01f);
        if (fresh) for (auto* m : {&wobble, &crackle, &hiss, &crush, &tape}) m->reset(m->t);
        float tp = tape.skip(n); if (std::abs(tp - lastTape) > 1e-4f) { setTape(tp); lastTape = tp; }
        bool gate = c.on(P_TEX_GATE);
        for (int i = 0; i < n; ++i) {
            float w = wobble.next(), ck = crackle.next(), hs = hiss.next(), cr = crush.next();
            float x[2] = {L[i], R[i]};
            // wobble: wow (two slow sines + slow random) and flutter, one tape for both channels
            wow1.advanceFree(0.55, sr); wow2.advanceFree(0.23, sr); flut.advanceFree(7.3, sr);
            float rnd = wowNoise.lp(rng.bi()) * 25.f;
            float wowA = 0.0035f * w * w * (float)sr, flA = 0.00012f * w * (float)sr;
            float mod = wowA * (0.6f * Lfo::sine(wow1.ph) + 0.3f * Lfo::sine(wow2.ph) + 0.1f * clampf(rnd, -1, 1)) + flA * Lfo::sine(flut.ph);
            float base = 0.0003f * (float)sr + wowA * 1.05f + flA;
            float e = gateEnv.process(0.5f * (std::abs(x[0]) + std::abs(x[1]))); float ng = 1.f; if (gate) { ng = std::min(1.f, e * 8.f); ng *= ng; }
            bool hold = false; if (cr > 0.001f) { acc += 1.f / (1.f + cr * cr * 11.f); if (acc >= 1.f) { acc -= 1.f; hold = true; } }
            float trig = ck > 0.001f ? (2 + 60 * ck * ck) / (float)sr : 0.f; bool hit = rng.uni() < trig; float hitAmp = hit ? (0.05f + 0.6f * std::pow(rng.uni(), 3.f)) * (rng.u() & 1 ? 1.f : -1.f) : 0.f; bool both = rng.uni() < 0.5f;
            for (int k = 0; k < 2; ++k) {
                float y = x[k];
                wob[k].push(y); if (w > 0 || !wobble.settled()) y = wob[k].read(base + mod);
                if (tp > 0.0005f) { float dr = 1 + 2 * tp; y = tLp[k].process(tBump[k].process(tHp[k].process(y))); y = std::tanh(y * dr) / dr; }
                if (cr > 0.001f) { if (hold) { float bits = 16 - 12 * cr; float step = 2.f / std::pow(2.f, bits); held[k] = std::round(y / step) * step; } y = held[k]; }
                float imp = (k == 0 || both) ? hitAmp : 0.f;
                float l, b, h; crk[k].tick(imp, l, b, h); y += 0.25f * ck * b * ng * 3.f;
                if (hs > 0) { float nz = hissLp[k].lp(hissHp[k].hp(rng.bi())); y += 0.04f * hs * hs * nz * ng * 2.f; }
                (k ? R[i] : L[i]) = y;
            }
            if (tp > 0.0005f) { float m = 0.15f * tp; float l = L[i], r = R[i]; L[i] = l + m * (r - l) * 0.5f; R[i] = r + m * (l - r) * 0.5f; }
        }
    }
};

// =====================================================================================================================
// SPACE: spring, plate, tape echo; shimmer (octave-up feedback) and freeze for all three.
struct Space {
    double sr = 48000; Spring spring; Plate plate; Echo echo; TypeFade tf; Delay pre[2]; Smooth preS, mix, shim, inGain, fz; PitchShifter sh[2]; float shimFb[2] = {0, 0}; float last[6] = {-1, -1, -1, -1, -1, -1};
    void prepare(double s) {
        sr = s; spring.prepare(sr); plate.prepare(sr); echo.prepare(sr); for (int k = 0; k < 2; ++k) { pre[k].init((int)(0.16 * sr)); sh[k].init((int)(0.07 * sr)); sh[k].setWindow((float)(0.06 * sr)); }
        preS.setTime(sr, 0.05); mix.setTime(sr, 0.02); inEnv.set(sr, 0.005, 0.3); igStep = (float)(1.0 / (0.06 * sr)); msA = (float)std::exp(-1.0 / (sr * 0.5)); shim.setTime(sr, 0.05); inGain.setTime(sr, 0.03); fz.setTime(sr, 0.06); tf.setTime(sr, 0.06, 0.08); reset();
    }
    void resetType(int t) { if (t == 0) spring.reset(); if (t == 1) plate.reset(); if (t == 2) echo.reset(); }
    void reset() { spring.reset(); plate.reset(); echo.reset(); for (int k = 0; k < 2; ++k) { pre[k].reset(); sh[k].reset(); shimFb[k] = 0; } tf = TypeFade{}; tf.setTime(sr, 0.06, 0.08); for (auto& l : last) l = -1; preS.reset(-1); igPh = 0; fz.reset(-1); inEnv.reset(); inMs = wetMs = 0; fresh = true; }
    double echoSeconds(const Ctx& c) const { return c.on(P_SPC_SYNC) ? kDivBeats[std::clamp(c.ch(P_SPC_DIV), 0, kNumDivs - 1)] * 60.0 / c.bpm : c.v(P_SPC_TIME) * 0.001; }
    static double rt60For(float decay01) { return 0.4 + decay01 * decay01 * 7.6; }
    void configure(int t, const Ctx& c) {
        float d = c.v(P_SPC_DECAY) * 0.01f, tone = c.v(P_SPC_TONE) * 0.01f; float fr = fz.v;
        if (t == 0) spring.set(rt60For(d), tone, fr);
        if (t == 1) plate.set(d, tone, fr);
        if (t == 2) echo.set(echoSeconds(c), d, tone, fr, c.on(P_SPC_PING));
    }
    inline void typeSample(int t, float a, float b, float& oL, float& oR) {
        if (t == 0) spring.process(a, b, oL, oR); else if (t == 1) plate.process(a, b, oL, oR); else echo.process(a, b, oL, oR);
    }
    float igPh = 0, igStep = 0.001f; EnvFollow inEnv; float inMs = 0, wetMs = 0, msA = 0.999f; bool fresh = true;
    void process(float* L, float* R, int n, const Ctx& c) {
        int type = std::clamp(c.ch(P_SPC_TYPE), 0, 2); if (tf.update(type)) { resetType(type); igPh = 0; }
        bool frz = c.on(P_SPC_FREEZE); if (fz.v < 0) fz.reset(frz ? 1.f : 0.f); fz.target(frz ? 1.f : 0.f); fz.skip(n);
        configure(type, c); if (tf.fading()) configure(tf.prev, c);
        float pd = type == 2 ? 0.f : c.v(P_SPC_PRE) * 0.001f * (float)sr; if (preS.v < 0) preS.reset(pd); preS.target(pd);
        mix.target(c.v(P_SPC_MIX) * 0.01f); shim.target(frz ? 0.f : c.v(P_SPC_SHIMMER) * 0.01f); inGain.target(1);
        if (fresh) { mix.reset(mix.t); shim.reset(shim.t); fresh = false; }
        // shimmer loop safety: lower feedback on long decays, only feed while there is input, and never let the wet build far above the input
        float shimGain = 0.4f * (1.f - 0.5f * c.v(P_SPC_DECAY) * 0.01f);
        for (int i = 0; i < n; ++i) {
            float m = mix.next(), s = shim.next(), p = preS.next(); if (igPh < 1) { igPh = std::min(1.f, igPh + igStep); } float ig = igPh * igPh * (3 - 2 * igPh);
            float in[2] = {L[i] * ig, R[i] * ig};
            float e = inEnv.process(0.5f * (std::abs(in[0]) + std::abs(in[1])));
            float guard = wetMs > 1e-12f ? clampf(std::sqrt(4.f * inMs / wetMs), 0, 1) : 1.f;
            float feed = s * shimGain * std::min(1.f, e * 20.f) * guard;
            float q[2];
            for (int k = 0; k < 2; ++k) { pre[k].push((k ? R[i] : L[i]) + softClip(shimFb[k] * feed)); q[k] = p < 1 ? pre[k].at(0) : pre[k].read(p); }
            float oL, oR; typeSample(type, q[0] * ig, q[1] * ig, oL, oR);
            if (tf.fading()) { float pL, pR; typeSample(tf.prev, q[0], q[1], pL, pR); oL = pL + tf.r.v * (oL - pL); oR = pR + tf.r.v * (oR - pR); tf.advance(); }
            if (s > 0) { float a = sh[0].process(oL, 2.f), b = sh[1].process(oR, 2.f); shimFb[0] = a; shimFb[1] = b; oL += 0.3f * s * a; oR += 0.3f * s * b; }
            else shimFb[0] = shimFb[1] = 0;
            inMs = flush(msA * inMs + (1 - msA) * 0.5f * (L[i] * L[i] + R[i] * R[i])); wetMs = flush(msA * wetMs + (1 - msA) * 0.5f * (oL * oL + oR * oR));
            float dg = std::min(1.f, 2 * (1 - m)), wg = std::min(1.f, 2 * m);
            L[i] = L[i] * dg + oL * wg; R[i] = R[i] * dg + oR * wg;
        }
    }
};

// =====================================================================================================================
// WIDTH (mid/side with mono bass via a 4th-order Linkwitz-Riley high-pass on the side signal) + DUCKING gain.
struct WidthSec {
    double sr = 48000; Biquad h1, h2; Smooth width, mono; float lastF = -1;
    void prepare(double s) { sr = s; width.setTime(sr, 0.02); mono.setTime(sr, 0.02); reset(); }
    void reset() { h1.reset(); h2.reset(); lastF = -1; width.reset(-1); }
    void process(float* L, float* R, int n, const Ctx& c) {
        float f = c.v(P_WID_FREQ); if (f != lastF) { h1.highpass(sr, f, 0.70710678); h2.highpass(sr, f, 0.70710678); lastF = f; }
        float wt = c.v(P_WID_WIDTH) * 0.01f; if (width.v < 0) { width.reset(wt); mono.reset(c.on(P_WID_MONO) ? 1.f : 0.f); }
        width.target(wt); mono.target(c.on(P_WID_MONO) ? 1.f : 0.f);
        for (int i = 0; i < n; ++i) {
            float w = width.next(), mb = mono.next();
            float M = 0.5f * (L[i] + R[i]), S = 0.5f * (L[i] - R[i]) * w;
            float Sh = h2.process(h1.process(S)); S = S + mb * (Sh - S);
            L[i] = M + S; R[i] = M - S;
        }
    }
};

struct Ducker {
    double sr = 48000; SyncLfo lfo; EnvFollow env; Smooth g; float relLast = -1;
    void prepare(double s) { sr = s; g.setTime(sr, 0.0015); env.set(sr, 0.001, 0.15); reset(); }
    void reset() { g.reset(1); env.reset(); relLast = -1; }
    /// writes per-sample gain (1 = no ducking) into out[n]
    void process(float* out, const float* scL, const float* scR, int n, const Ctx& c) {
        int src = c.ch(P_DUK_SRC); float depth = c.v(P_DUK_DEPTH) * 0.01f;
        if (src == 0) {
            lfo.begin(c, true, c.ch(P_DUK_DIV), 1);
            float pw = 1 + c.v(P_DUK_SHAPE) * 0.05f;
            for (int i = 0; i < n; ++i) { double p = lfo.step(); g.target(1 - depth * std::pow((float)(1 - p), pw)); out[i] = g.next(); }
        } else {
            float rel = c.v(P_DUK_RELEASE); if (rel != relLast) { env.set(sr, 0.001, rel * 0.001); relLast = rel; }
            float th = c.v(P_DUK_THRESH);
            for (int i = 0; i < n; ++i) {
                float x = scL ? 0.5f * (std::abs(scL[i]) + std::abs(scR ? scR[i] : scL[i])) : 0.f;
                float e = env.process(x); float over = gainToDb(e) - th; float fr = clampf(over / 18.f, 0, 1);
                g.target(1 - depth * fr); out[i] = g.next();
            }
        }
    }
};
} // namespace mg
