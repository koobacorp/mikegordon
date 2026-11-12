// MIKE GORDON — the whole chain: movable sections in the user's order, width/ducking, level match, Amount, Before/After, limiter.
#pragma once
#include "Sections.h"
#include <atomic>
#include <functional>

namespace mg {

struct HostInfo { double bpm = 120, ppq = 0; bool playing = false, hasPpq = false; };

class Engine {
public:
    Amp amp; FilterSec flt; PitchSec pit; Motion mot; Texture tex; Space spc; WidthSec wid; Ducker duk;
    std::atomic<float> matchDb {0}, limitDb {0}, outPeak {0}, inPeak {0};

    void prepare(double s, int /*maxBlock*/) {
        sr = s; amp.prepare(sr); flt.prepare(sr); pit.prepare(sr); mot.prepare(sr); tex.prepare(sr); spc.prepare(sr); wid.prepare(sr); duk.prepare(sr);
        for (auto& r : secRamp) r.setTime(sr, 0.02); widRamp.setTime(sr, 0.02); dukRamp.setTime(sr, 0.02); orderGain.setTime(sr, 0.008);
        amount.setTime(sr, 0.02); compare.setTime(sr, 0.01); bypass.setTime(sr, 0.01); outGain.setTime(sr, 0.02); lmGain.setTime(sr, 0.25);
        msCoef = (float)std::exp(-1.0 / (sr * 0.4)); relCoef = (float)(1.0 - std::exp(-1.0 / (sr * 0.08)));
        reset();
    }
    void reset() {
        amp.reset(); flt.reset(); pit.reset(); mot.reset(); tex.reset(); spc.reset(); wid.reset(); duk.reset();
        for (int m = 0; m < kMovable; ++m) { secLive[m] = false; warmS[m] = 0; } primed = false; dms = wms = 0; limG = 1; lmGain.reset(1);
        orderGain.v = 1; orderGain.up = true;
    }
    /// order[i] = which movable section runs at slot i (a permutation of 0..5). Safe to call from any thread between blocks.
    void setOrder(const int* o) { if (!validOrder(o)) return; for (int i = 0; i < kMovable; ++i) pending[i].store(o[i]); }
    void getOrder(int* o) const { for (int i = 0; i < kMovable; ++i) o[i] = order[i]; }
    static bool validOrder(const int* o) { int seen = 0; for (int i = 0; i < kMovable; ++i) { if (o[i] < 0 || o[i] >= kMovable) return false; seen |= 1 << o[i]; } return seen == (1 << kMovable) - 1; }
    int latencySamples() const { return 0; }
    double sampleRate() const { return sr; }

    void process(float* L, float* R, int n, const float* scL, const float* scR, const HostInfo& hiIn, const float* p) {
        HostInfo hi = hiIn; if (!(hi.bpm >= 20 && hi.bpm <= 999)) hi.bpm = 120;
        if (!primed) prime(p);
        float ip = 0;
        for (int i = 0; i < n; ++i) { if (!std::isfinite(L[i])) L[i] = 0; if (!std::isfinite(R[i])) R[i] = 0; ip = std::max(ip, std::max(std::abs(L[i]), std::abs(R[i]))); }
        inPeak.store(ip);
        float op = 0;
        for (int off = 0; off < n; off += kSub) {
            int m = std::min(kSub, n - off);
            Ctx c; c.sr = sr; c.bpm = hi.bpm; c.playing = hi.playing; c.hasPpq = hi.hasPpq; c.ppq = hi.ppq + off * hi.bpm / 60.0 / sr; c.p = p;
            op = std::max(op, block(L + off, R + off, m, scL ? scL + off : nullptr, scR ? scR + off : nullptr, c));
        }
        outPeak.store(op);
    }

private:
    double sr = 48000; Ramp secRamp[kMovable], widRamp, dukRamp, orderGain; bool secLive[kMovable] = {}, widLive = false, primed = false; int warmS[kMovable] = {}, warmW = 0;
    int order[kMovable] = {M_TONE, M_FILTER, M_PITCH, M_MOTION, M_TEXTURE, M_SPACE}; std::atomic<int> pending[kMovable] = {{0}, {1}, {2}, {3}, {4}, {5}};
    Smooth amount, compare, bypass, outGain, lmGain; float dms = 0, wms = 0, msCoef = 0.999f, limG = 1, relCoef = 0.001f;
    float dryL[kSub], dryR[kSub], tL[kSub], tR[kSub], dg[kSub];

    void prime(const float* p) {
        for (int m = 0; m < kMovable; ++m) { bool on = p[sectionSwitch(m)] >= 0.5f; secRamp[m].v = on ? 1.f : 0.f; secRamp[m].up = on; }
        widRamp.v = p[P_WID_ON] >= 0.5f ? 1.f : 0.f; dukRamp.v = p[P_DUK_ON] >= 0.5f ? 1.f : 0.f;
        amount.reset(p[P_AMOUNT] * 0.01f); compare.reset(p[P_COMPARE] >= 0.5f ? 1.f : 0.f); bypass.reset(p[P_BYPASS] >= 0.5f ? 1.f : 0.f); outGain.reset(dbToGain(p[P_OUTGAIN]));
        for (int i = 0; i < kMovable; ++i) order[i] = pending[i].load();
        primed = true;
    }
    void runSection(int m, float* L, float* R, int n, const Ctx& c) {
        switch (m) { case M_TONE: amp.process(L, R, n, c); break; case M_FILTER: flt.process(L, R, n, c); break; case M_PITCH: pit.process(L, R, n, c); break;
                     case M_MOTION: mot.process(L, R, n, c); break; case M_TEXTURE: tex.process(L, R, n, c); break; default: spc.process(L, R, n, c); break; }
    }
    void resetSection(int m) { switch (m) { case M_TONE: amp.reset(); break; case M_FILTER: flt.reset(); break; case M_PITCH: pit.reset(); break; case M_MOTION: mot.reset(); break; case M_TEXTURE: tex.reset(); break; default: spc.reset(); break; } }
    /// Run a section with a click-free on/off ramp. When fully off it costs nothing and its state is cleared.
    /// A section that just switched on first runs silently for `prefill` seconds so its delay lines are full, then fades in.
    template <class F> void gated(Ramp& r, bool& live, int& warm, double prefill, bool on, float* L, float* R, int n, F&& fn) {
        r.up = on;
        if (!on && r.v == 0) { warm = 0; return; }
        live = true;
        if (on && r.v == 1) { fn(); return; }
        std::memcpy(tL, L, sizeof(float) * (size_t)n); std::memcpy(tR, R, sizeof(float) * (size_t)n); fn();
        if (on && r.v == 0 && warm < (int)(prefill * sr)) { warm += n; std::memcpy(L, tL, sizeof(float) * (size_t)n); std::memcpy(R, tR, sizeof(float) * (size_t)n); return; }
        for (int i = 0; i < n; ++i) { float g = r.next(); L[i] = tL[i] + g * (L[i] - tL[i]); R[i] = tR[i] + g * (R[i] - tR[i]); }
    }
    float block(float* L, float* R, int n, const float* scL, const float* scR, const Ctx& c) {
        const float* p = c.p;
        std::memcpy(dryL, L, sizeof(float) * (size_t)n); std::memcpy(dryR, R, sizeof(float) * (size_t)n);
        // reorder request: dip the effect path, swap, come back
        bool want = false; int po[kMovable]; for (int i = 0; i < kMovable; ++i) { po[i] = pending[i].load(); want |= po[i] != order[i]; }
        if (want && !validOrder(po)) want = false;
        if (want) orderGain.up = false;
        if (want && orderGain.v == 0) { std::memcpy(order, po, sizeof order); orderGain.up = true; }
        // the movable chain
        for (int s = 0; s < kMovable; ++s) {
            int m = order[s]; bool on = p[sectionSwitch(m)] >= 0.5f;
            if (!on && secRamp[m].v == 0) { if (secLive[m]) { resetSection(m); secLive[m] = false; } continue; }
            static const double pre[kMovable] = {0.012, 0.0, 0.06, 0.035, 0.02, 0.0};
            gated(secRamp[m], secLive[m], warmS[m], pre[m], on, L, R, n, [&] { runSection(m, L, R, n, c); });
        }
        bool won = p[P_WID_ON] >= 0.5f;
        if (!won && widRamp.v == 0) { if (widLive) { wid.reset(); widLive = false; } }
        else gated(widRamp, widLive, warmW, 0.0, won, L, R, n, [&] { wid.process(L, R, n, c); });
        // ducking gain
        bool don = p[P_DUK_ON] >= 0.5f; dukRamp.up = don; bool duckAll = c.ch(P_DUK_TARGET) == 1;
        if (don || dukRamp.v > 0) { duk.process(dg, scL, scR, n, c); for (int i = 0; i < n; ++i) { float r = dukRamp.next(); dg[i] = 1 - r * (1 - dg[i]); } }
        else { for (int i = 0; i < n; ++i) dg[i] = 1; duk.reset(); }
        amount.target(p[P_AMOUNT] * 0.01f); compare.target(p[P_COMPARE] >= 0.5f ? 1.f : 0.f); bypass.target(p[P_BYPASS] >= 0.5f ? 1.f : 0.f); outGain.target(dbToGain(p[P_OUTGAIN]));
        bool match = p[P_MATCH] >= 0.5f, limit = p[P_LIMIT] >= 0.5f;
        const float T0 = 0.89125094f, C = 0.98855309f; float peakOut = 0, minG = 1;
        for (int i = 0; i < n; ++i) {
            float wl = L[i], wr = R[i]; if (!std::isfinite(wl) || !std::isfinite(wr)) { wl = wr = 0; }
            float og = orderGain.next(); wl *= og; wr *= og;
            if (!duckAll) { wl *= dg[i]; wr *= dg[i]; }
            float dl = dryL[i], dr = dryR[i];
            dms = flush(msCoef * dms + (1 - msCoef) * 0.5f * (dl * dl + dr * dr)); wms = flush(msCoef * wms + (1 - msCoef) * 0.5f * (wl * wl + wr * wr));
            if ((i & 15) == 0) { if (!match) lmGain.target(1); else if (dms > 1e-6f && wms > 1e-10f) lmGain.target(clampf(std::sqrt(dms / wms), 0.125f, 4.f)); }
            float lg = lmGain.next(); wl *= lg; wr *= lg;
            float a = amount.next(); float ol = dl + a * (wl - dl), orr = dr + a * (wr - dr);
            if (duckAll) { ol *= dg[i]; orr *= dg[i]; }
            float cp = std::max(compare.next(), bypass.next()); if (cp >= 1.f) { ol = dl; orr = dr; } else if (cp > 0) { ol += cp * (dl - ol); orr += cp * (dr - orr); }
            float g = outGain.next(); ol *= g; orr *= g;
            if (limit) {
                float pk = std::max(std::abs(ol), std::abs(orr)); float want1 = 1.f;
                if (pk > T0) { float y = T0 + (C - T0) * std::tanh((pk - T0) / (C - T0)); want1 = y / pk; }
                float rel = limG + (1 - limG) * relCoef; if (rel > 0.9999999f) rel = 1.f;
                limG = std::min(want1, rel); ol *= limG; orr *= limG; minG = std::min(minG, limG);
            } else limG = 1;
            L[i] = ol; R[i] = orr; peakOut = std::max(peakOut, std::max(std::abs(ol), std::abs(orr)));
        }
        matchDb.store(gainToDb(lmGain.v)); limitDb.store(gainToDb(minG));
        return peakOut;
    }
};
} // namespace mg
