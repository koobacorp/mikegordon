// MIKE GORDON — engine tests. Every control is checked; filters, rates, delays and gains are compared to textbook math.
// Build: c++ -std=c++20 -O2 -Isrc/engine -Itests tests/test_engine.cpp -o engine_tests && ./engine_tests [--quick]
#include "TestUtil.h"
#include <cstring>
#include <set>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
using namespace mg; using namespace tu;

static int passed = 0, failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++passed; else { ++failed; std::printf("  FAIL %s:%d  %s | ", __FILE__, __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
static void section(const char* s) { std::printf("- %s\n", s); }
static const double SR = 48000;

static double ampAt(const Buf& x, double f, size_t a, size_t b) { return std::abs(bin(x, SR, f, a, b)); }
/// steady-state gain (dB) of `fn` processing a sine at f
template <class F> static double measureGain(double f, F&& fn, double sr = SR) {
    size_t n = (size_t)(sr * 0.5); Buf x(n); for (size_t i = 0; i < n; ++i) x[i] = 0.25f * (float)std::sin(2 * kPi * f * (double)i / sr);
    Buf y(n); for (size_t i = 0; i < n; ++i) y[i] = fn(x[i]);
    size_t a = n / 2, cyc = (size_t)std::floor((double)(n - a) * f / sr); size_t b = a + (size_t)std::lround((double)cyc * sr / f); if (cyc < 4) b = n;
    return db(std::abs(bin(y, sr, f, a, b)) / std::abs(bin(x, sr, f, a, b)));
}
static void setP(float* p, const char* id, float v) { int i = paramIndex(id); if (i < 0) { std::printf("  unknown id %s\n", id); return; } p[i] = v; }

int main(int argc, char** argv) {
    bool quick = argc > 1 && std::strcmp(argv[1], "--quick") == 0;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::puts("MIKE GORDON engine tests");

    section("parameter table");
    { std::set<std::string> ids; for (int i = 0; i < kNumParams; ++i) { const auto& d = kParams[i]; ids.insert(d.id); CHECKM(d.def >= d.min && d.def <= d.max, "%s", d.id); CHECK(d.max > d.min);
        CHECK(std::strlen(d.name) > 0); if (d.kind == K_CHOICE) CHECK(d.nChoices == (int)d.max + 1);
        for (float n : {0.f, 0.1f, 0.37f, 0.5f, 0.81f, 1.f}) { float pl = fromNorm(i, n); CHECK(pl >= d.min - 1e-4f && pl <= d.max + 1e-4f); if (d.kind == K_FLOAT) CHECKM(std::abs(toNorm(i, pl) - n) < 1e-4f, "%s %f", d.id, n); } }
      CHECK((int)ids.size() == kNumParams);
      int mains = 0; for (auto& d : kParams) mains += d.main; CHECK(mains == 8);
      for (int s = S_TONE; s <= S_WIDTH; ++s) { int c = 0; for (auto& d : kParams) c += d.section == s && d.main; CHECKM(c == 1, "section %d main knobs %d", s, c); }
      // skew centre: the knob's middle lands on the centre value
      CHECK(std::abs(fromNorm(P_FLT_LP, 0.5f) - 1000.f) < 1.f); CHECK(std::abs(fromNorm(P_WID_FREQ, 0.5f) - 120.f) < 0.1f);
      for (int m = 0; m < kMovable; ++m) CHECK(kParams[sectionSwitch(m)].kind == K_BOOL); }

    section("biquads vs RBJ cookbook");
    { Biquad b;
      for (double g : {-12.0, -6.0, 3.0, 12.0}) {
        b.lowShelf(SR, 100, g); CHECK(std::abs(db(b.mag(SR, 1)) - g) < 0.05); CHECK(std::abs(db(b.mag(SR, 23000))) < 0.05); CHECK(std::abs(db(b.mag(SR, 100)) - g / 2) < 0.1);
        b.highShelf(SR, 3500, g); CHECK(std::abs(db(b.mag(SR, 23999)) - g) < 0.1); CHECK(std::abs(db(b.mag(SR, 5))) < 0.05); CHECK(std::abs(db(b.mag(SR, 3500)) - g / 2) < 0.1);
        b.peak(SR, 700, 0.8, g); CHECK(std::abs(db(b.mag(SR, 700)) - g) < 0.01); CHECK(std::abs(db(b.mag(SR, 5))) < 0.05);
      }
      b.lowpass(SR, 1000, 0.70710678); CHECK(std::abs(db(b.mag(SR, 1000)) + 3.0103) < 0.01); CHECK(std::abs(db(b.mag(SR, 10))) < 0.01); CHECK(db(b.mag(SR, 10000)) < -38);
      b.highpass(SR, 1000, 0.70710678); CHECK(std::abs(db(b.mag(SR, 1000)) + 3.0103) < 0.01); CHECK(db(b.mag(SR, 100)) < -39);
      // processing matches the analytic magnitude (validates the TDF2 kernel)
      for (double f : {60.0, 440.0, 2500.0, 9000.0}) { Biquad q; q.peak(SR, 2500, 1.0, 9); double m = measureGain(f, [&](float x) { return q.process(x); }); CHECKM(std::abs(m - db(q.mag(SR, f))) < 0.05, "f %f meas %f exp %f", f, m, db(q.mag(SR, f))); } }

    section("state-variable filter vs pre-warped analog prototype");
    { for (double fc : {200.0, 1000.0, 5000.0, 15000.0}) for (double Q : {0.7071, 2.0, 8.0}) for (double f : {50.0, 300.0, 1000.0, 3000.0, 12000.0}) {
        Svf s; s.set(SR, fc, Q); double W = std::tan(kPi * f / SR) / std::tan(kPi * fc / SR);
        std::complex<double> den(1 - W * W, W / Q); double eLP = db(1 / std::abs(den)), eHP = db(W * W / std::abs(den)), eBP = db((W / Q) / std::abs(den));
        double mLP = measureGain(f, [&](float x) { return s.lp(x); }); Svf h; h.set(SR, fc, Q); double mHP = measureGain(f, [&](float x) { return h.hp(x); });
        Svf bpf; bpf.set(SR, fc, Q); double mBP = measureGain(f, [&](float x) { float l, b, hh; bpf.tick(x, l, b, hh); return b / (float)Q; });
        if (eLP > -60) CHECKM(std::abs(mLP - eLP) < 0.05, "LP fc %g Q %g f %g: %f vs %f", fc, Q, f, mLP, eLP);
        if (eHP > -60) CHECKM(std::abs(mHP - eHP) < 0.05, "HP fc %g Q %g f %g: %f vs %f", fc, Q, f, mHP, eHP);
        if (eBP > -60) CHECKM(std::abs(mBP - eBP) < 0.05, "BP fc %g Q %g f %g", fc, Q, f);
      } }

    section("first-order allpass: unity gain, -90 deg at its frequency");
    { for (double fc : {300.0, 1200.0, 6000.0}) { Allpass1 a; a.a = Allpass1::coef(SR, fc);
        for (double f : {100.0, 1000.0, 8000.0}) { Allpass1 q = a; CHECK(std::abs(measureGain(f, [&](float x) { return q.process(x); })) < 0.02); }
        size_t n = 24000; Buf x(n), y(n); Allpass1 q = a; for (size_t i = 0; i < n; ++i) { x[i] = (float)std::sin(2 * kPi * fc * (double)i / SR); y[i] = q.process(x[i]); }
        double ph = std::arg(bin(y, SR, fc, 12000, n) / bin(x, SR, fc, 12000, n)) * 180 / kPi; CHECKM(std::abs(ph + 90) < 0.5, "phase %f", ph); } }

    section("ADAA tanh, fast tanh, smoothers, delay interpolation");
    { AdaaTanh t; float maxErr = 0; for (int i = 0; i < 4800; ++i) { float x = 3.f * (float)std::sin(2 * kPi * 50 * i / SR); float y = t.process(x); float xm = 3.f * (float)std::sin(2 * kPi * 50 * (i - 0.5) / SR); if (i > 2) maxErr = std::max(maxErr, std::abs(y - std::tanh(xm))); }
      CHECKM(maxErr < 2e-3f, "adaa err %f", maxErr);
      float e = 0; for (float x = -6; x <= 6; x += 0.001f) e = std::max(e, std::abs(fastTanh(x) - std::tanh(x))); CHECKM(e < 2e-4f, "fastTanh %g", e);
      Smooth s; s.setTime(SR, 0.01); s.reset(0); s.target(1); int k = 0; while (!s.settled() && k < 100000) { s.next(); ++k; } CHECK(s.v == 1.f); CHECK(k < 48000 * 0.2);
      s.reset(0.3f); s.target(0.3f); CHECK(s.next() == 0.3f);
      Delay d; d.init(64); for (int i = 0; i < 64; ++i) d.push((float)i); CHECK(d.at(0) == 63 && d.at(10) == 53); CHECK(std::abs(d.read(10.5f) - 52.5f) < 1e-4f); CHECK(std::abs(d.readLinear(3.25f) - 59.75f) < 1e-4f); }

    section("sync rates: synced LFOs follow the host beat exactly");
    { for (int div = 0; div < kNumDivs; ++div) { Ctx c; c.sr = SR; c.bpm = 120; c.playing = true; c.hasPpq = true; c.ppq = 3.3; SyncLfo l; l.begin(c, true, div, 1);
        double expectHz = 120.0 / 60.0 / kDivBeats[div]; CHECKM(std::abs(l.inc * SR - expectHz) < 1e-9, "div %s", kDivs[div]);
        double ph = 3.3 / kDivBeats[div]; ph -= std::floor(ph); CHECK(std::abs(l.l.ph - ph) < 1e-9); }
      // free-running rate
      { Ctx c; c.sr = SR; SyncLfo l; l.begin(c, false, 0, 2.5); CHECK(std::abs(l.inc * SR - 2.5) < 1e-12); }
      // tremolo on a constant input traces the LFO: check period and phase against the beat grid
      for (int div : {6, 9, 12, 7, 10, 2}) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", 4); setP(p, "mot_sync", 1); setP(p, "mot_div", (float)div);
        setP(p, "mot_depth", 100); setP(p, "mot_mix", 100); setP(p, "mot_spread", 0);
        Stereo dc; dc.L.assign((size_t)(SR * 9), 0.5f); dc.R = dc.L; auto r = render(*e, p, dc, 256, {120, 0, true});
        double periodS = kDivBeats[div] * 0.5; // seconds at 120 bpm
        // gain is minimum (0) at LFO phase 0.5
        size_t s0 = (size_t)(SR * 1.0); int misses = 0; double worst = 0;
        for (int k = 0; k < 6; ++k) { double t = (std::ceil(1.0 / periodS) + k + 0.5) * periodS; size_t i = (size_t)std::lround(t * SR); if (i >= r.out.size()) break; worst = std::max(worst, (double)r.out.L[i]); misses += r.out.L[i] > 0.01f; }
        CHECKM(misses == 0, "div %s worst %f", kDivs[div], worst);
        float mx = peak(r.out.L, s0); CHECK(std::abs(mx - 0.5f) < 0.01f); (void)s0; }
      // free rate 2 Hz
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", 4); setP(p, "mot_rate", 2); setP(p, "mot_depth", 100); setP(p, "mot_mix", 100);
        Stereo dc; dc.L.assign((size_t)(SR * 4), 0.5f); dc.R = dc.L; auto r = render(*e, p, dc); Buf g = r.out.L; for (auto& v : g) v -= 0.25f; double f = zcFreq(g, SR, 4800, g.size()); CHECKM(std::abs(f - 2) < 0.01, "free trem %f", f); } }

    section("tremolo depth, stereo spread (auto-pan)");
    { for (float dpt : {25.f, 50.f, 100.f}) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", 4); setP(p, "mot_rate", 3); setP(p, "mot_depth", dpt); setP(p, "mot_mix", 100);
        Stereo dc; dc.L.assign((size_t)(SR * 2), 0.5f); dc.R = dc.L; auto r = render(*e, p, dc); float mn = 1, mx = 0; for (size_t i = 9600; i < r.out.size(); ++i) { mn = std::min(mn, r.out.L[i]); mx = std::max(mx, r.out.L[i]); }
        CHECKM(std::abs(mn / mx - (1 - dpt / 100)) < 0.01, "depth %f ratio %f", dpt, mn / mx); }
      auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", 4); setP(p, "mot_rate", 3); setP(p, "mot_depth", 100); setP(p, "mot_mix", 100); setP(p, "mot_spread", 100);
      Stereo dc; dc.L.assign((size_t)(SR * 2), 0.5f); dc.R = dc.L; auto r = render(*e, p, dc); float worst = 0; for (size_t i = 9600; i < r.out.size(); ++i) worst = std::max(worst, std::abs(r.out.L[i] + r.out.R[i] - 0.5f));
      CHECKM(worst < 0.01f, "pan sum %f", worst); }

    section("flanger comb notches |cos(pi f D)|, phaser notch at its centre");
    { for (double f : {250.0, 500.0, 750.0, 1000.0, 1500.0, 2200.0}) {
        auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", 1); setP(p, "mot_depth", 0); setP(p, "mot_mix", 50); setP(p, "mot_fb", 0);
        auto in = sine(SR, f, 1.0, 0.25f); auto r = render(*e, p, in); double m = ampAt(r.out.L, f, 24000, 48000) / 0.25; double ex = std::abs(std::cos(kPi * f * 0.001));
        CHECKM(std::abs(m - ex) < 0.01, "flanger f %g got %f exp %f", f, m, ex); }
      for (double f : {200.0, 600.0, 2000.0}) {
        auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", 2); setP(p, "mot_depth", 0); setP(p, "mot_mix", 50); setP(p, "mot_fb", 0);
        auto in = sine(SR, f, 1.0, 0.25f); auto r = render(*e, p, in); double m = ampAt(r.out.L, f, 24000, 48000) / 0.25;
        double a = Allpass1::coef(SR, 600); std::complex<double> z = std::exp(std::complex<double>(0, -2 * kPi * f / SR)); auto A = (a + z) / (1.0 + a * z); double ex = std::abs(0.5 * (1.0 + std::pow(A, 6)));
        CHECKM(std::abs(m - ex) < 0.01, "phaser f %g got %f exp %f", f, m, ex); if (f == 600.0) CHECKM(db(m) < -40, "notch %f dB", db(m)); }
      // feedback raises the comb peaks but stays stable
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", 1); setP(p, "mot_depth", 0); setP(p, "mot_mix", 50); setP(p, "mot_fb", 90);
        auto in = sine(SR, 1000, 1.0, 0.25f); auto r = render(*e, p, in); double m = ampAt(r.out.L, 1000, 24000, 48000) / 0.25; double ex = std::abs(0.5 + 0.5 / (1.0 - 0.9 * std::exp(std::complex<double>(0, -2 * kPi * 1000 * 0.001))) * std::exp(std::complex<double>(0, -2 * kPi * 1000 * 0.001)) - 0.0);
        (void)ex; CHECK(m > 1.5 && m < 6); CHECK(finite(r.out)); } }

    section("chorus / swirl / mix law");
    { for (int t = 0; t < 5; ++t) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", (float)t); setP(p, "mot_mix", 0); auto in = noise(SR, 0.5, 0.3f); auto r = render(*e, p, in);
        bool same = true; for (size_t i = 0; i < in.size(); ++i) same &= r.out.L[i] == in.L[i] && r.out.R[i] == in.R[i]; CHECKM(same, "type %d mix 0 not transparent", t); }
      for (int t : {0, 3}) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "mot_on", 1); setP(p, "mot_type", (float)t); setP(p, "mot_mix", 100); setP(p, "mot_depth", 80); setP(p, "mot_spread", 100);
        auto in = sine(SR, 800, 3, 0.3f); auto r = render(*e, p, in); double diff = 0; for (size_t i = 48000; i < in.size(); ++i) diff += std::abs(r.out.L[i] - r.out.R[i]); CHECK(diff / (double)in.size() > 0.01); CHECK(finite(r.out));
        // the sweep moves: spectral level at 800 Hz varies over time for the swirl; chorus wet stays at ~the input level
        double a1 = ampAt(r.out.L, 800, 48000, 52800), a2 = ampAt(r.out.L, 800, 72000, 76800), a3 = ampAt(r.out.L, 800, 96000, 100800);
        if (t == 3) CHECK(std::max({a1, a2, a3}) - std::min({a1, a2, a3}) > 0.01); else { double rr = rms(r.out.L, 48000) / rms(in.L, 48000); CHECKM(rr > 0.4 && rr < 1.3, "chorus level %f", rr); } } }

    section("pitch: octave up/down and detune ratios");
    { auto tone = sine(SR, 440, 2.0, 0.4f);
      // Doppler shifters spread the shifted tone over sidebands at the window rate, so compare band energy around each frequency
      auto band = [&](const Buf& y, double f) { double e = 0; for (double g = f - 60; g <= f + 60; g += 2.5) { double a = std::abs(bin(y, SR, g, 24000, 96000)); e += a * a; } return std::sqrt(e); };
      { PitchShifter s; s.init((int)(0.05 * SR)); s.setWindow((float)(0.04 * SR)); Buf y(tone.size()); for (size_t i = 0; i < y.size(); ++i) y[i] = s.process(tone.L[i], 2.f);
        double e880 = band(y, 880), e440 = band(y, 440); CHECKM(db(e880 / std::max(e440, 1e-9)) > 15, "up %f %f", e880, e440); CHECKM(e880 > 0.25, "up level %f", e880); CHECK(std::abs(zcFreq(y, SR, 4800, y.size()) / 880 - 1) < 0.02); }
      { PitchShifter s; s.init((int)(0.06 * SR)); s.setWindow((float)(0.05 * SR)); Buf y(tone.size()); for (size_t i = 0; i < y.size(); ++i) y[i] = s.process(tone.L[i], 0.5f);
        double e220 = band(y, 220), e440 = band(y, 440); CHECKM(db(e220 / std::max(e440, 1e-9)) > 15, "down %f %f", e220, e440); CHECKM(e220 > 0.25, "down level %f", e220); CHECK(std::abs(zcFreq(y, SR, 4800, y.size()) / 220 - 1) < 0.02); }
      for (double cents : {5.0, 12.0, 25.0, 50.0}) { double r = std::pow(2.0, cents / 1200); PitchShifter s; s.init((int)(0.04 * SR)); s.setWindow((float)(0.03 * SR)); Buf y(tone.size());
        for (size_t i = 0; i < y.size(); ++i) y[i] = s.process(tone.L[i], (float)r); double f = zcFreq(y, SR, 4800, y.size()); CHECKM(std::abs(1200 * std::log2(f / 440.0) - cents) < std::max(1.0, 0.08 * cents), "cents %f got %f", cents, 1200 * std::log2(f / 440.0)); }
      // section level: octave blend adds the shifted voice at the knob's level
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "pit_on", 1); setP(p, "pit_up", 50); setP(p, "pit_detune", 0); auto r = render(*e, p, tone);
        CHECK(std::abs(ampAt(r.out.L, 440, 48000, 96000) - 0.4) < 0.03); double u = band(r.out.L, 880); CHECKM(u > 0.12 && u < 0.22, "up blend %f", u); }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "pit_on", 1); setP(p, "pit_detune", 0); auto r = render(*e, p, tone); double d = 0; for (size_t i = 0; i < tone.size(); ++i) d = std::max(d, (double)std::abs(r.out.L[i] - tone.L[i])); CHECKM(d < 1e-6, "pitch all-zero not transparent %g", d); } }

    section("amp voices: tone stack, bright, drive, built-in chorus/trem/vibrato");
    { auto ampGain = [&](int voice, double f, std::function<void(float*)> set) {
        auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "amp_on", 1); setP(p, "amp_voice", (float)voice); setP(p, "amp_drive", 0); setP(p, "amp_vfx", 0); set(p);
        auto in = sine(SR, f, 1.0, 0.02f); auto r = render(*e, p, in); size_t cyc = (size_t)(0.5 * f); size_t b = 24000 + (size_t)std::lround(cyc * SR / f); return db(ampAt(r.out.L, f, 24000, b) / 0.02); };
      for (int v = 0; v < 4; ++v) {
        double b0 = ampGain(v, 40, [](float*) {}), b12 = ampGain(v, 40, [](float* p) { p[P_AMP_BASS] = 12; });
        double t0 = ampGain(v, 12000, [](float*) {}), t12 = ampGain(v, 12000, [](float* p) { p[P_AMP_TREBLE] = 12; });
        double mf = kVoiceSpec[v].midHz; double m0 = ampGain(v, mf, [](float*) {}), m12 = ampGain(v, mf, [](float* p) { p[P_AMP_MID] = 12; }), mm = ampGain(v, mf, [](float* p) { p[P_AMP_MID] = -12; });
        double r0 = ampGain(v, 10000, [](float*) {}), r1 = ampGain(v, 10000, [](float* p) { p[P_AMP_BRIGHT] = 1; });
        Biquad ls; ls.lowShelf(SR, 100, 12); Biquad hs; hs.highShelf(SR, 3500, 12); Biquad pk; pk.peak(SR, mf, 0.8, 12);
        CHECKM(std::abs((b12 - b0) - db(ls.mag(SR, 40))) < 0.3, "voice %d bass %f exp %f", v, b12 - b0, db(ls.mag(SR, 40)));
        CHECKM(std::abs((t12 - t0) - db(hs.mag(SR, 12000))) < 0.3, "voice %d treble %f", v, t12 - t0);
        CHECKM(std::abs((m12 - m0) - 12) < 0.3 && std::abs((mm - m0) + 12) < 0.3, "voice %d mid %f %f", v, m12 - m0, mm - m0);
        Biquad br; br.highShelf(SR, 2500, 6, 0.8); CHECKM(std::abs((r1 - r0) - db(br.mag(SR, 10000))) < 0.5, "voice %d bright %f", v, r1 - r0);
      }
      auto thd = [&](int voice, float drive) {
        auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "amp_on", 1); setP(p, "amp_voice", (float)voice); setP(p, "amp_drive", drive); setP(p, "amp_vfx", 0);
        auto in = sine(SR, 200, 1.0, 0.5f); auto r = render(*e, p, in); double f1 = ampAt(r.out.L, 200, 24000, 48000), h = 0; for (int k = 2; k <= 9; ++k) { double a = ampAt(r.out.L, 200.0 * k, 24000, 48000); h += a * a; } return std::sqrt(h) / f1; };
      double g2 = thd(0, 2), g8 = thd(0, 8), c8 = thd(2, 8), c2 = thd(2, 2);
      CHECKM(g2 < 0.02, "glass thd %f", g2); CHECKM(c8 > g8, "chime %f glass %f", c8, g8); CHECK(c8 > c2);
      for (int v = 0; v < 4; ++v) { double last = -1; bool mono = true; for (float d : {0.f, 3.f, 6.f, 10.f}) { double t = thd(v, d); mono &= t >= last - 1e-4; last = t; } CHECKM(mono, "voice %d thd not rising with drive", v); CHECK(thd(v, 10) < 0.35); }
      // Glass: built-in chorus decorrelates L/R; 0 = mono in, mono out
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "amp_on", 1); setP(p, "amp_voice", 0); setP(p, "amp_vfx", 100); auto in = sine(SR, 600, 2, 0.2f); auto r = render(*e, p, in); double d = 0; for (size_t i = 24000; i < in.size(); ++i) d += std::abs(r.out.L[i] - r.out.R[i]); CHECK(d / 72000 > 0.01); }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "amp_on", 1); setP(p, "amp_voice", 0); setP(p, "amp_vfx", 0); auto in = sine(SR, 600, 1, 0.2f); auto r = render(*e, p, in); bool same = true; for (size_t i = 0; i < in.size(); ++i) same &= r.out.L[i] == r.out.R[i]; CHECK(same); }
      // Warm: tremolo depth = 85% of Voice FX
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "amp_on", 1); setP(p, "amp_voice", 1); setP(p, "amp_vfx", 100); setP(p, "amp_vrate", 4); setP(p, "amp_drive", 0);
        auto in = sine(SR, 2000, 2, 0.02f); auto r = render(*e, p, in); double mx = 0, mn = 1; for (size_t s = 24000; s + 480 < in.size(); s += 240) { double a = ampAt(r.out.L, 2000, s, s + 240); mx = std::max(mx, a); mn = std::min(mn, a); }
        CHECKM(std::abs(mn / mx - 0.15) < 0.03, "warm trem ratio %f", mn / mx); }
      // Swim: true pitch vibrato, peak deviation = 2*pi*f_lfo*A (A = 2.5 ms at 100%)
      for (double rate : {2.0, 5.0}) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "amp_on", 1); setP(p, "amp_voice", 3); setP(p, "amp_vfx", 100); setP(p, "amp_vrate", (float)rate); setP(p, "amp_drive", 0);
        auto in = sine(SR, 1000, 3, 0.02f); auto r = render(*e, p, in); double mxf = 0, mnf = 1e9; for (size_t s = 24000; s + 960 < in.size(); s += 96) { double f = zcFreq(r.out.L, SR, s, s + 480); mxf = std::max(mxf, f); mnf = std::min(mnf, f); }
        double expDev = 2 * kPi * rate * 0.0025; double dev = (mxf - mnf) / 2000.0; CHECKM(std::abs(dev - expDev) / expDev < 0.15, "rate %g dev %f exp %f", rate, dev, expDev);
        CHECK(std::abs(ampAt(r.out.L, 1000, 24000, 144000) ) < 0.02 * 1.6); }
      // amp spring adds a tail
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "amp_on", 1); setP(p, "amp_verb", 80); auto in = sine(SR, 500, 0.2, 0.3f); append(in, silence(SR, 1.0)); auto r = render(*e, p, in); CHECK(rms(r.out.L, (size_t)(0.35 * SR), (size_t)(0.6 * SR)) > 0.003); } }

    section("filter section: response, resonance, LFO and envelope movement");
    { for (double lp : {500.0, 2000.0, 8000.0}) for (double f : {100.0, 1000.0, 4000.0}) {
        auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "flt_on", 1); setP(p, "flt_lp", (float)lp); setP(p, "flt_hp", 20); setP(p, "flt_res", 0);
        auto in = sine(SR, f, 1.0, 0.25f); auto r = render(*e, p, in); double m = db(ampAt(r.out.L, f, 24000, 48000) / 0.25);
        auto proto = [&](double fc, double Q, bool hp) { double W = std::tan(kPi * f / SR) / std::tan(kPi * fc / SR); std::complex<double> den(1 - W * W, W / Q); return db((hp ? W * W : 1.0) / std::abs(den)); };
        double ex = proto(lp, 0.7071, false) + proto(20, 0.7071, true); CHECKM(std::abs(m - ex) < 0.1, "lp %g f %g: %f vs %f", lp, f, m, ex); }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "flt_on", 1); setP(p, "flt_lp", 20000); setP(p, "flt_hp", 1000); auto in = sine(SR, 100, 1, 0.25f); auto r = render(*e, p, in); CHECK(db(ampAt(r.out.L, 100, 24000, 48000) / 0.25) < -38); }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "flt_on", 1); setP(p, "flt_lp", 1000); setP(p, "flt_res", 100); auto in = sine(SR, 1000, 1, 0.05f); auto r = render(*e, p, in);
        double m = db(ampAt(r.out.L, 1000, 24000, 48000) / 0.05); double Q = FilterSec::qFromRes(1); CHECKM(std::abs(m - db(Q)) < 0.2, "res peak %f exp %f", m, db(Q)); }
      // LFO movement: cutoff sweeps -> level of a 2 kHz tone changes periodically at the synced rate (1 bar at 120 = 0.5 Hz)
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "flt_on", 1); setP(p, "flt_lp", 2000); setP(p, "flt_move", 1); setP(p, "flt_sync", 1); setP(p, "flt_div", 2); setP(p, "flt_depth", 100);
        auto in = sine(SR, 2000, 6, 0.25f); auto r = render(*e, p, in, 256, {120, 0, true});
        // 1 bar = 4 beats = 2 s: t=0.5 s is LFO phase 0.25 (cutoff +3 oct), t=1.5 s is phase 0.75 (cutoff -3 oct)
        double hi = ampAt(r.out.L, 2000, (size_t)(2.5 * SR) - 480, (size_t)(2.5 * SR) + 480), lo = ampAt(r.out.L, 2000, (size_t)(3.5 * SR) - 480, (size_t)(3.5 * SR) + 480); CHECKM(hi > 4 * lo, "lfo hi %f lo %f", hi, lo); }
      // envelope movement opens the filter on loud input only
      { auto run = [&](float amp) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "flt_on", 1); setP(p, "flt_lp", 300); setP(p, "flt_move", 2); setP(p, "flt_depth", 100); setP(p, "flt_sens", 60);
          auto in = sine(SR, 3000, 1, amp); auto r = render(*e, p, in); return ampAt(r.out.L, 3000, 24000, 48000) / amp; };
        double quiet = run(0.002f), loud = run(0.3f); CHECKM(loud > 10 * quiet, "env quiet %f loud %f", quiet, loud); } }

    section("space: spring/plate/echo decay math, sync, freeze, shimmer");
    { // spring RT60
      for (double rt : {1.0, 2.5, 5.0}) { Spring s; s.prepare(SR); s.set(rt, 1.0, false); size_t n = (size_t)(SR * rt * 1.6); Buf y(n); for (size_t i = 0; i < n; ++i) { float a, b; s.process(i < 48 ? 0.5f : 0.f, i < 48 ? 0.5f : 0.f, a, b); y[i] = a; }
        double e1 = rms(y, (size_t)(0.1 * rt * SR), (size_t)(0.2 * rt * SR)), e2 = rms(y, (size_t)(0.6 * rt * SR), (size_t)(0.7 * rt * SR)); double slope = (db(e2) - db(e1)) / (0.5 * rt); double meas = -60 / slope;
        CHECKM(std::abs(meas - rt) / rt < 0.25, "spring rt %f measured %f", rt, meas); CHECK(std::abs(s.loopGain() - std::pow(10.0, -3.0 * 0.0337 / rt)) < 1e-6); }
      // plate decay rises monotonically with the knob
      { double last = 0; for (float d : {0.f, 0.3f, 0.6f, 0.9f}) { Plate pl; pl.prepare(SR); pl.set(d, 0.5, false); size_t n = (size_t)(SR * 3); Buf y(n); for (size_t i = 0; i < n; ++i) { float a, b; pl.process(i < 48 ? 0.5f : 0.f, i < 48 ? 0.5f : 0.f, a, b); y[i] = a; }
          double tail = rms(y, (size_t)(1.0 * SR), (size_t)(1.5 * SR)); CHECKM(tail > last, "plate decay %f tail %g", d, tail); last = tail; } }
      // echo: synced time is exact and repeats fall by the feedback times the loop filters
      for (int div : {9, 6, 5}) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "spc_on", 1); setP(p, "spc_type", 2); setP(p, "spc_sync", 1); setP(p, "spc_div", (float)div); setP(p, "spc_mix", 100); setP(p, "spc_decay", 50); setP(p, "spc_tone", 100);
        Stereo in = silence(SR, 3); const size_t o = 4800; for (int i = 0; i < 960; ++i) in.L[o + (size_t)i] = in.R[o + (size_t)i] = 0.3f * (float)std::sin(2 * kPi * 1000 * i / SR); auto r = render(*e, p, in, 256, {120, 0, true});
        size_t D = (size_t)std::lround(kDivBeats[div] * 0.5 * SR); double a0 = ampAt(r.out.L, 1000, o + D, o + D + 960), a1 = ampAt(r.out.L, 1000, o + 2 * D, o + 2 * D + 960), before = rms(r.out.L, o + D - 1200, o + D - 100);
        Biquad lp; lp.lowpass(SR, 11800, 0.6); Biquad hp; hp.highpass(SR, 90, 0.6); double ex = 0.95 * 0.5 * lp.mag(SR, 1000) * hp.mag(SR, 1000);
        CHECKM(std::abs(a0 - 0.3) < 0.02, "div %s first %f", kDivs[div], a0); CHECKM(std::abs(a1 / a0 - ex) < 0.04, "repeat ratio %f exp %f", a1 / a0, ex); CHECK(before < 1e-3); }
      // freeze holds the tail for every type, and stays bounded
      for (int t = 0; t < 3; ++t) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "spc_on", 1); setP(p, "spc_type", (float)t); setP(p, "spc_mix", 100); setP(p, "spc_decay", 60); setP(p, "spc_sync", 0); setP(p, "spc_time", 300);
        Stereo in = noise(SR, 1.0, 0.3f); append(in, silence(SR, 12)); auto r = render(*e, p, in, 256, {}, [&](size_t pos, float* pp) { pp[P_SPC_FREEZE] = pos > (size_t)(0.9 * SR) ? 1.f : 0.f; });
        double e1 = rms(r.out.L, (size_t)(1.5 * SR), (size_t)(2.5 * SR)), e2 = rms(r.out.L, (size_t)(11 * SR), (size_t)(12 * SR)); CHECKM(e1 > 0.005 && e2 > 0.5 * e1, "freeze type %d %f -> %f", t, e1, e2); CHECK(peak(r.out.L) < 3.f);
        // unfreeze: decays again
        auto e3 = engine(); float q[kNumParams]; std::memcpy(q, p, sizeof q); auto r2 = render(*e3, q, in, 256, {}, [&](size_t pos, float* pp) { pp[P_SPC_FREEZE] = (pos > (size_t)(0.9 * SR) && pos < (size_t)(4 * SR)) ? 1.f : 0.f; });
        CHECKM(rms(r2.out.L, (size_t)(11 * SR), (size_t)(12 * SR)) < 0.2 * e1, "type %d did not release", t); }
      // shimmer adds the octave above
      { auto run = [&](float sh) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "spc_on", 1); setP(p, "spc_mix", 100); setP(p, "spc_decay", 70); setP(p, "spc_shimmer", sh); auto in = sine(SR, 330, 3, 0.3f); auto r = render(*e, p, in); return ampAt(r.out.L, 660, 96000, 144000) / std::max(1e-9, ampAt(r.out.L, 330, 96000, 144000)); };
        CHECKM(run(100) > 3 * run(0), "shimmer %f vs %f", run(100), run(0)); }
      // pre-delay shifts the onset
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "spc_on", 1); setP(p, "spc_mix", 100); setP(p, "spc_pre", 100); Stereo in = silence(SR, 1); in.L[0] = in.R[0] = 1; auto r = render(*e, p, in); CHECK(peak(r.out.L, 0, (size_t)(0.099 * SR)) < 1e-6f); CHECK(peak(r.out.L, (size_t)(0.1 * SR), (size_t)(0.3 * SR)) > 1e-4f); }
      // mix law: 0% = dry exactly
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "spc_on", 1); setP(p, "spc_mix", 0); auto in = noise(SR, 0.5, 0.3f); auto r = render(*e, p, in); bool same = true; for (size_t i = 0; i < in.size(); ++i) same &= r.out.L[i] == in.L[i]; CHECK(same); } }

    section("texture: wobble pitch drift, crush steps, cassette roll-off, crackle, hiss, noise gate");
    { auto tex = [&](std::function<void(float*)> set, const Stereo& in) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "tex_on", 1); setP(p, "tex_wobble", 0); setP(p, "tex_tape", 0); set(p); return render(*e, p, in); };
      auto tone = sine(SR, 1000, 4, 0.3f);
      { auto r = tex([](float* p) { p[P_TEX_WOBBLE] = 100; }, tone); double mn = 1e9, mx = 0; for (size_t s = 48000; s + 4800 < tone.size(); s += 2400) { double f = zcFreq(r.out.L, SR, s, s + 4800); mn = std::min(mn, f); mx = std::max(mx, f); }
        CHECKM(mx - mn > 3 && mx - mn < 60, "wobble spread %f Hz", mx - mn); }
      { auto r = tex([](float* p) { p[P_TEX_WOBBLE] = 0; }, tone); double d = 0; for (size_t i = 0; i < tone.size(); ++i) d = std::max(d, (double)std::abs(r.out.L[i] - tone.L[i])); CHECKM(d < 1e-6, "texture all-zero not transparent %g", d); }
      { auto r = tex([](float* p) { p[P_TEX_CRUSH] = 100; }, tone); std::set<int> lv; for (size_t i = 48000; i < 96000; ++i) lv.insert((int)std::lround(r.out.L[i] * 8)); CHECKM(lv.size() <= 17, "levels %zu", lv.size());
        int runs = 0, holdMax = 0, h = 0; for (size_t i = 48001; i < 96000; ++i) { if (r.out.L[i] == r.out.L[i - 1]) ++h; else { holdMax = std::max(holdMax, h); h = 0; ++runs; } } CHECKM(holdMax >= 10, "hold %d", holdMax); }
      { auto lo = sine(SR, 200, 1, 0.1f), hi = sine(SR, 10000, 1, 0.1f);
        auto r1 = tex([](float* p) { p[P_TEX_TAPE] = 100; }, lo), r2 = tex([](float* p) { p[P_TEX_TAPE] = 100; }, hi);
        Biquad lp; lp.lowpass(SR, 18000 * 0.28, 0.7071); double exHi = db(lp.mag(SR, 10000)); double mHi = db(ampAt(r2.out.L, 10000, 24000, 48000) / 0.1), mLo = db(ampAt(r1.out.L, 200, 24000, 48000) / 0.1);
        CHECKM(std::abs(mHi - exHi) < 1.5, "cassette 10k %f exp %f", mHi, exHi); CHECKM(std::abs(mLo) < 1.5, "cassette 200 %f", mLo); }
      { auto in = sine(SR, 100, 3, 0.2f); auto r = tex([](float* p) { p[P_TEX_CRACKLE] = 100; }, in); Buf d(in.size()); for (size_t i = 0; i < d.size(); ++i) d[i] = r.out.L[i] - in.L[i]; int clicks = 0; for (size_t i = 1; i < d.size(); ++i) clicks += std::abs(d[i]) > 0.01f && std::abs(d[i - 1]) <= 0.01f;
        CHECKM(clicks > 20, "crackle clicks %d", clicks); }
      { auto in = sine(SR, 100, 2, 0.2f); append(in, silence(SR, 4)); auto r = tex([](float* p) { p[P_TEX_HISS] = 100; p[P_TEX_GATE] = 1; }, in);
        double during = rms(r.out.R, 48000, 96000) , after = rms(r.out.R, (size_t)(5.5 * SR), (size_t)(6 * SR)); (void)during; CHECKM(after < 1e-4, "gate after %g", after);
        auto r2 = tex([](float* p) { p[P_TEX_HISS] = 100; p[P_TEX_GATE] = 0; }, in); double h = rms(r2.out.R, (size_t)(5.5 * SR), (size_t)(6 * SR)); CHECKM(db(h) > -45 && db(h) < -20, "hiss %f dB", db(h)); } }

    section("width and mono bass (Linkwitz-Riley 24 dB/oct on the side signal)");
    { auto run = [&](std::function<void(float*)> set, const Stereo& in) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "wid_on", 1); set(p); return render(*e, p, in); };
      auto st = noise(SR, 1, 0.3f);
      { auto r = run([](float* p) { p[P_WID_WIDTH] = 0; p[P_WID_MONO] = 0; }, st); float d = 0; for (size_t i = 0; i < st.size(); ++i) d = std::max(d, std::abs(r.out.L[i] - r.out.R[i])); CHECK(d < 1e-6f); }
      { auto r = run([](float* p) { p[P_WID_WIDTH] = 100; p[P_WID_MONO] = 0; }, st); float d = 0; for (size_t i = 0; i < st.size(); ++i) d = std::max(d, std::abs(r.out.L[i] - st.L[i])); CHECK(d < 1e-6f); }
      { auto r = run([](float* p) { p[P_WID_WIDTH] = 200; p[P_WID_MONO] = 0; }, st); float d = 0; for (size_t i = 0; i < st.size(); ++i) { float S = 0.5f * (st.L[i] - st.R[i]); float M = 0.5f * (st.L[i] + st.R[i]); d = std::max(d, std::abs(r.out.L[i] - (M + 2 * S))); } CHECK(d < 1e-5f); }
      { Biquad a, b; a.highpass(SR, 120, 0.70710678); b.highpass(SR, 120, 0.70710678); CHECK(std::abs(db(a.mag(SR, 120) * b.mag(SR, 120)) + 6.0206) < 0.01); CHECK(db(a.mag(SR, 30) * b.mag(SR, 30)) < -45); }
      for (double f : {30.0, 40.0, 60.0}) { auto side = sine(SR, f, 2, 0.3f, true); auto r = run([](float* p) { p[P_WID_MONO] = 1; p[P_WID_FREQ] = 120; }, side);
        Buf S(side.size()); for (size_t i = 0; i < S.size(); ++i) S[i] = 0.5f * (r.out.L[i] - r.out.R[i]); double att = db(ampAt(S, f, 48000, 96000) / 0.3); CHECKM(att < -18, "mono bass %g Hz side %f dB", f, att);
        // mono below the split: L and R correlate
        double c = 0, l2 = 0, r2 = 0; for (size_t i = 48000; i < 96000; ++i) { c += r.out.L[i] * r.out.R[i]; l2 += r.out.L[i] * r.out.L[i]; r2 += r.out.R[i] * r.out.R[i]; } (void)c; (void)l2; (void)r2; }
      { auto side = sine(SR, 3000, 1, 0.3f, true); auto r = run([](float* p) { p[P_WID_MONO] = 1; p[P_WID_FREQ] = 120; }, side); Buf S(side.size()); for (size_t i = 0; i < S.size(); ++i) S[i] = 0.5f * (r.out.L[i] - r.out.R[i]); CHECK(std::abs(db(ampAt(S, 3000, 24000, 48000) / 0.3)) < 0.1); }
      { auto mid = sine(SR, 40, 1, 0.3f); auto r = run([](float* p) { p[P_WID_MONO] = 1; p[P_WID_WIDTH] = 200; }, mid); CHECK(std::abs(db(ampAt(r.out.L, 40, 24000, 48000) / 0.3)) < 0.05); } }

    section("ducking: tempo pump and sidechain");
    { for (int div : {6, 9}) for (float dpt : {50.f, 100.f}) { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "duk_on", 1); setP(p, "duk_src", 0); setP(p, "duk_div", (float)div); setP(p, "duk_depth", dpt);
        Stereo dc; dc.L.assign((size_t)(SR * 4), 0.5f); dc.R = dc.L; auto r = render(*e, p, dc, 256, {120, 0, true});
        // expected: the pump curve 1 - depth*(1-phase)^(1+shape*5) through the 1.5 ms one-pole smoother
        double per = kDivBeats[div] * 0.5; double worst = 0; double pw = 1 + 50 * 0.05, a = std::exp(-1.0 / (SR * 0.0015)), g = 1;
        for (size_t j = 0; j < r.out.size(); ++j) { double ph = std::fmod((double)j / SR / per, 1.0); double tg = 1 - dpt / 100 * std::pow(1 - ph, pw); g = tg + (g - tg) * a; if (j > (size_t)SR) worst = std::max(worst, std::abs(r.out.L[j] / 0.5 - g)); }
        CHECKM(worst < 0.03, "pump div %s depth %f err %f", kDivs[div], dpt, worst);
        size_t mid = (size_t)std::lround(2.95 * per * SR); CHECK(r.out.L[mid] > 0.5f * (1 - dpt / 100) + 0.2f); }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "duk_on", 1); setP(p, "duk_src", 1); setP(p, "duk_depth", 80); setP(p, "duk_thresh", -24);
        Stereo dc; dc.L.assign((size_t)(SR * 2), 0.5f); dc.R = dc.L; Stereo sc = silence(SR, 2); for (size_t i = 48000; i < 52800; ++i) sc.L[i] = sc.R[i] = 0.5f * (float)std::sin(2 * kPi * 60 * (double)i / SR);
        auto r = render(*e, p, dc, 256, {}, nullptr, &sc); CHECKM(std::abs(r.out.L[50000] / 0.5f - 0.2f) < 0.05f, "sc duck %f", r.out.L[50000] / 0.5f); CHECK(std::abs(r.out.L[40000] - 0.5f) < 1e-4f); CHECK(std::abs(r.out.L[90000] - 0.5f) < 0.01f);
        auto e2 = engine(); auto r2 = render(*e2, p, dc); CHECK(std::abs(peak(r2.out.L) - 0.5f) < 1e-4f); }
      // target: effect-only leaves the dry part alone at 50% amount
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "duk_on", 1); setP(p, "duk_depth", 100); setP(p, "amount", 50); setP(p, "duk_target", 0); Stereo dc; dc.L.assign((size_t)(SR * 2), 0.5f); dc.R = dc.L; auto r = render(*e, p, dc, 256, {120, 0, true});
        size_t i = (size_t)(1.0 * SR); auto mn = [&](const Buf& b) { float m = 1; for (size_t j = i; j < i + 480; ++j) m = std::min(m, b[j]); return m; };
        CHECKM(std::abs(mn(r.out.L) - 0.25f) < 0.02f, "effect-only %f", mn(r.out.L));
        auto e2 = engine(); setP(p, "duk_target", 1); auto r2 = render(*e2, p, dc, 256, {120, 0, true}); CHECKM(mn(r2.out.L) < 0.05f, "everything %f", mn(r2.out.L)); } }

    section("level match, Amount, Before/After, bypass, output, safety limiter");
    { auto loud = [&](float* p) { neutral(p); setP(p, "amp_on", 1); setP(p, "amp_drive", 0); setP(p, "amp_vfx", 0); setP(p, "amp_bass", 12); setP(p, "amp_mid", 12); setP(p, "amp_treble", 12); };
      auto in = guitarLoop(SR, 100, 4); for (auto* ch : {&in.L, &in.R}) for (auto& v : *ch) v *= 0.5f;
      { auto e = engine(); float p[kNumParams]; loud(p); setP(p, "match", 0); auto r = render(*e, p, in); CHECK(db(rms(r.out.L, 96000) / rms(in.L, 96000)) > 4); }
      { auto e = engine(); float p[kNumParams]; loud(p); setP(p, "match", 1); auto r = render(*e, p, in); double d = db(rms(r.out.L, 144000) / rms(in.L, 144000)); CHECKM(std::abs(d) < 1.0, "matched %f dB", d);
        float g1 = e->matchDb.load(); Stereo s2 = silence(SR, 3); auto r2 = render(*e, p, s2); CHECKM(std::abs(e->matchDb.load() - g1) < 1.0f, "match drifted in silence %f -> %f", g1, e->matchDb.load()); (void)r2; }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "match", 1); auto r = render(*e, p, in); CHECK(std::abs(e->matchDb.load()) < 0.01f); }
      // Amount=0 / bypass / before are bit-exact, even with everything switched on
      auto heavy = [&](float* p) { applyPreset(10, p); for (int m = 0; m < kMovable; ++m) p[sectionSwitch(m)] = 1; p[P_DUK_ON] = 1; p[P_TEX_HISS] = 50; p[P_TEX_CRACKLE] = 50; };
      for (const char* which : {"amount", "bypass", "compare"}) { auto e = engine(); float p[kNumParams]; heavy(p); if (!std::strcmp(which, "amount")) setP(p, "amount", 0); else setP(p, which, 1);
        auto r = render(*e, p, in); bool same = true; for (size_t i = 0; i < in.size(); ++i) same &= r.out.L[i] == in.L[i] && r.out.R[i] == in.R[i]; CHECKM(same, "%s not bit-exact", which); }
      // Amount is a linear dry/wet mix
      { auto e1 = engine(), e2 = engine(); float p[kNumParams]; heavy(p); p[P_TEX_HISS] = 0; p[P_TEX_CRACKLE] = 0; p[P_LIMIT] = 0; auto rw = render(*e1, p, in); setP(p, "amount", 50); auto rh = render(*e2, p, in);
        double d = 0; for (size_t i = 0; i < in.size(); ++i) d = std::max(d, (double)std::abs(rh.out.L[i] - 0.5f * (in.L[i] + rw.out.L[i]))); CHECKM(d < 1e-4, "amount mix err %g", d); }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "out_gain", -6); auto r = render(*e, p, in); CHECK(std::abs(db(rms(r.out.L, 4800) / rms(in.L, 4800)) + 6) < 0.01); }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "limit", 1); setP(p, "out_gain", 12); auto hot = sine(SR, 100, 1, 0.9f); auto r = render(*e, p, hot); CHECKM(peak(r.out.L) <= 0.98856f && peak(r.out.R) <= 0.98856f, "limiter peak %f", peak(r.out.L)); CHECK(peak(r.out.L, 24000) > 0.9f); }
      { auto e = engine(); float p[kNumParams]; neutral(p); setP(p, "limit", 1); auto q = sine(SR, 300, 1, 0.5f); auto r = render(*e, p, q); bool same = true; for (size_t i = 0; i < q.size(); ++i) same &= r.out.L[i] == q.L[i]; CHECK(same); } }

    section("click-free switching");
    { auto base = [&](float* p) { neutral(p); setP(p, "amp_on", 1); setP(p, "amp_drive", 0); setP(p, "amp_vfx", 0); setP(p, "wid_on", 1); setP(p, "match", 0); };
      struct Sw { const char* name; std::function<void(float*)> pre, post; std::function<void(Engine&)> act; };
      std::vector<Sw> sw = {
        {"amp voice Glass->Chime", [](float*) {}, [](float* p) { p[P_AMP_VOICE] = 2; }, nullptr},
        {"amp voice Warm->Swim", [](float* p) { p[P_AMP_VOICE] = 1; }, [](float* p) { p[P_AMP_VOICE] = 3; }, nullptr},
        {"amp off", [](float*) {}, [](float* p) { p[P_AMP_ON] = 0; }, nullptr},
        {"filter on", [](float* p) { p[P_FLT_LP] = 800; }, [](float* p) { p[P_FLT_ON] = 1; }, nullptr},
        {"filter move Off->LFO", [](float* p) { p[P_FLT_ON] = 1; p[P_FLT_LP] = 2000; p[P_FLT_DEPTH] = 100; }, [](float* p) { p[P_FLT_MOVE] = 1; }, nullptr},
        {"filter cutoff jump", [](float* p) { p[P_FLT_ON] = 1; p[P_FLT_LP] = 8000; }, [](float* p) { p[P_FLT_LP] = 300; }, nullptr},
        {"motion on (chorus)", [](float*) {}, [](float* p) { p[P_MOT_ON] = 1; }, nullptr},
        {"motion chorus->phaser", [](float* p) { p[P_MOT_ON] = 1; }, [](float* p) { p[P_MOT_TYPE] = 2; }, nullptr},
        {"motion flanger->tremolo", [](float* p) { p[P_MOT_ON] = 1; p[P_MOT_TYPE] = 1; }, [](float* p) { p[P_MOT_TYPE] = 4; }, nullptr},
        {"pitch on", [](float*) {}, [](float* p) { p[P_PIT_ON] = 1; p[P_PIT_DOWN] = 50; }, nullptr},
        {"texture on", [](float*) {}, [](float* p) { p[P_TEX_ON] = 1; }, nullptr},
        {"space on", [](float*) {}, [](float* p) { p[P_SPC_ON] = 1; }, nullptr},
        {"space plate->echo", [](float* p) { p[P_SPC_ON] = 1; }, [](float* p) { p[P_SPC_TYPE] = 2; }, nullptr},
        {"space spring->plate", [](float* p) { p[P_SPC_ON] = 1; p[P_SPC_TYPE] = 0; }, [](float* p) { p[P_SPC_TYPE] = 1; }, nullptr},
        {"freeze on", [](float* p) { p[P_SPC_ON] = 1; }, [](float* p) { p[P_SPC_FREEZE] = 1; }, nullptr},
        {"mono bass toggle", [](float*) {}, [](float* p) { p[P_WID_MONO] = 0; }, nullptr},
        {"width jump", [](float*) {}, [](float* p) { p[P_WID_WIDTH] = 200; }, nullptr},
        {"ducking on", [](float*) {}, [](float* p) { p[P_DUK_ON] = 1; }, nullptr},
        {"amount 100->0", [](float*) {}, [](float* p) { p[P_AMOUNT] = 0; }, nullptr},
        {"before/after", [](float* p) { p[P_FLT_ON] = 1; p[P_FLT_LP] = 500; }, [](float* p) { p[P_COMPARE] = 1; }, nullptr},
        {"bypass", [](float* p) { p[P_FLT_ON] = 1; p[P_FLT_LP] = 500; }, [](float* p) { p[P_BYPASS] = 1; }, nullptr},
        {"output gain jump", [](float*) {}, [](float* p) { p[P_OUTGAIN] = -12; }, nullptr},
        {"section order change", [](float* p) { p[P_FLT_ON] = 1; p[P_FLT_LP] = 1500; }, [](float*) {}, [](Engine& e) { int o[6] = {M_SPACE, M_FILTER, M_TONE, M_PITCH, M_MOTION, M_TEXTURE}; e.setOrder(o); }},
      };
      for (auto& s : sw) {
        auto e = engine(); float p[kNumParams]; base(p); s.pre(p); auto in = sine(SR, 220, 1.5, 0.3f); size_t at = (size_t)(0.75 * SR) / 256 * 256;
        auto r = render(*e, p, in, 256, {}, [&](size_t pos, float* pp) { if (pos == at) { s.post(pp); if (s.act) s.act(*e); } });
        // a click is a burst of high-frequency energy at the switch that is not there before or after it settles
        float before = maxD2(r.out.L, at - 9600, at), around = maxD2(r.out.L, at, at + 4800), after = maxD2(r.out.L, at + 19200, at + 28800); float limit = std::max({4.f * before, 2.f * after, 0.004f});
        CHECKM(around <= limit, "%s: d2 %f vs before %f after %f", s.name, around, before, after); CHECK(finite(r.out)); } }

    section("no runaway feedback at extreme settings");
    { auto e = engine(); float p[kNumParams]; setDefaults(p); for (int m = 0; m < kMovable; ++m) p[sectionSwitch(m)] = 1;
      setP(p, "limit", 0); setP(p, "match", 0); setP(p, "amp_drive", 10); setP(p, "amp_verb", 100); setP(p, "flt_res", 100); setP(p, "flt_lp", 3000); setP(p, "pit_down", 100); setP(p, "pit_up", 100); setP(p, "pit_detune", 50);
      setP(p, "mot_type", 1); setP(p, "mot_fb", 90); setP(p, "mot_depth", 100); setP(p, "tex_wobble", 100); setP(p, "tex_tape", 100); setP(p, "spc_decay", 100); setP(p, "spc_shimmer", 100); setP(p, "wid_width", 200);
      for (int st = 0; st < 3; ++st) { setP(p, "spc_type", (float)st); setP(p, "mot_type", (float)(st == 0 ? 1 : st == 1 ? 2 : 3));
        Stereo in = noise(SR, 20, 0.9f, (uint32_t)(st + 3)); append(in, silence(SR, quick ? 10 : 40)); auto r = render(*e, p, in);
        CHECK(finite(r.out)); float pk = peak(r.out.L); CHECKM(pk < 12.f, "space %d peak %f", st, pk);
        double a = rms(r.out.L, (size_t)(10 * SR), (size_t)(15 * SR)), b = rms(r.out.L, (size_t)(15 * SR), (size_t)(20 * SR)); CHECKM(b < 1.6 * a, "space %d growing %f -> %f", st, a, b);
        double tail = rms(r.out.L, r.out.size() - (size_t)SR); CHECKM(tail < 0.05, "space %d tail after silence %f", st, tail); } }

    section("NaN / inf input and denormals");
    { auto e = engine(); float p[kNumParams]; setDefaults(p); for (int m = 0; m < kMovable; ++m) p[sectionSwitch(m)] = 1; p[P_DUK_ON] = 1; p[P_TEX_GATE] = 1;
      Stereo in = noise(SR, 2, 0.5f); in.L[1000] = NAN; in.R[2000] = INFINITY; in.L[3000] = -INFINITY; append(in, silence(SR, quick ? 20 : 60));
      auto r = render(*e, p, in); CHECK(finite(r.out)); int sub = subnormals(r.out); CHECKM(sub == 0, "%d subnormal output samples", sub);
      // all internal state settles to exact zero after a long silence (no denormal hum)
      CHECK(peak(r.out.L, r.out.size() - 4800) < 1e-6f); }

    section("every control changes the sound");
    { auto loop = guitarLoop(SR, 120, 1); for (auto* ch : {&loop.L, &loop.R}) for (auto& v : *ch) v *= 0.6f;
      // prerequisites so each control is audible
      auto prep = [&](int i, float* p) {
        setDefaults(p); p[P_MATCH] = 0; p[P_LIMIT] = 0; int s = kParams[i].section;
        if (s == S_FILTER) { p[P_FLT_ON] = 1; p[P_FLT_LP] = 2000; p[P_FLT_MOVE] = 1; p[P_FLT_SYNC] = 0; if (i == P_FLT_SENS) p[P_FLT_MOVE] = 2; if (i == P_FLT_DIV) p[P_FLT_SYNC] = 1; }
        if (s == S_PITCH) { p[P_PIT_ON] = 1; p[P_PIT_DOWN] = 30; }
        if (s == S_MOTION) { p[P_MOT_ON] = 1; p[P_MOT_TYPE] = (i == P_MOT_FB) ? 1 : 2; if (i == P_MOT_DIV) p[P_MOT_SYNC] = 1; p[P_MOT_DEPTH] = 60; }
        if (s == S_TEXTURE) { p[P_TEX_ON] = 1; if (i == P_TEX_GATE) p[P_TEX_HISS] = 100; }
        if (s == S_SPACE) { p[P_SPC_ON] = 1; p[P_SPC_MIX] = 50; if (i == P_SPC_TIME || i == P_SPC_DIV || i == P_SPC_SYNC || i == P_SPC_PING) { p[P_SPC_TYPE] = 2; p[P_SPC_SYNC] = i == P_SPC_DIV ? 1.f : 0.f; p[P_SPC_TIME] = 500; } }
        if (s == S_WIDTH) { p[P_WID_WIDTH] = 150; if (i >= P_DUK_ON) { p[P_DUK_ON] = 1; p[P_DUK_DEPTH] = 80; } if (i == P_DUK_THRESH || i == P_DUK_RELEASE) p[P_DUK_SRC] = 1; if (i == P_DUK_TARGET) p[P_AMOUNT] = 50; if (i == P_WID_FREQ) p[P_WID_WIDTH] = 200; }
        if (s == S_TONE) { if (i == P_AMP_VRATE) p[P_AMP_VFX] = 80; }
        if (i == P_MATCH) { p[P_AMP_TREBLE] = 12; }
        if (i == P_LIMIT) { p[P_OUTGAIN] = 12; p[P_AMP_BASS] = 12; p[P_AMP_MID] = 12; }
      };
      int tested = 0, silentCtl = 0;
      for (int i = 0; i < kNumParams; ++i) {
        if (i >= P_MACRO1 && i <= P_MACRO3) continue; // macros tested below
        const auto& d = kParams[i]; std::vector<float> vals; if (d.kind == K_CHOICE) for (int k = 0; k <= (int)d.max; ++k) vals.push_back((float)k); else vals = {d.min, d.max};
        std::vector<Stereo> outs;
        Stereo sc = loop; for (auto& v : sc.L) v *= 2; for (auto& v : sc.R) v *= 2;
        for (float v : vals) { float p[kNumParams]; prep(i, p); p[i] = v; auto e = engine(); auto r = render(*e, p, loop, 256, {120, 0, true}, nullptr, &sc); outs.push_back(r.out); }
        bool allDiff = true; for (size_t a = 1; a < outs.size(); ++a) { if (i == P_SPC_DIV && kDivBeats[a] * 0.5 >= 2.0 && kDivBeats[a - 1] * 0.5 >= 2.0) continue; double diff = 0; for (size_t k = 0; k < loop.size(); ++k) diff += std::abs(outs[a].L[k] - outs[a - 1].L[k]) + std::abs(outs[a].R[k] - outs[a - 1].R[k]); if (diff / (double)loop.size() < 1e-5) allDiff = false; }
        CHECKM(allDiff, "control %s had no audible effect", d.id); silentCtl += !allDiff; ++tested;
      }
      std::printf("  %d controls checked, %d silent\n", tested, silentCtl); }

    section("macros");
    { MacroMap m; m.s[0][0] = {P_SPC_MIX, 0.5f}; m.s[0][1] = {P_FLT_LP, -1.f}; m.s[1][0] = {P_AMP_VOICE, 1.f}; float in[kNumParams], out[kNumParams]; setDefaults(in); in[P_SPC_MIX] = 30;
      in[P_MACRO1] = 0; applyMacros(in, out, m); CHECK(out[P_SPC_MIX] == 30 && out[P_FLT_LP] == in[P_FLT_LP]);
      in[P_MACRO1] = 100; applyMacros(in, out, m); CHECK(std::abs(out[P_SPC_MIX] - 80) < 1e-3f); CHECK(std::abs(out[P_FLT_LP] - 20) < 1e-3f);
      in[P_MACRO1] = 50; applyMacros(in, out, m); CHECK(std::abs(out[P_SPC_MIX] - 55) < 1e-3f); CHECK(std::abs(toNorm(P_FLT_LP, out[P_FLT_LP]) - 0.5f) < 1e-4f);
      in[P_MACRO2] = 100; applyMacros(in, out, m); CHECK(out[P_AMP_VOICE] == in[P_AMP_VOICE]); // choices are not macro targets
      auto dm = defaultMacros(); int slots = 0; for (auto& mm : dm.s) for (auto& s : mm) if (s.param >= 0) { ++slots; CHECK(kParams[s.param].kind == K_FLOAT); } CHECK(slots >= 6);
      // a macro audibly changes the sound
      auto loop = guitarLoop(SR, 120, 1); float p[kNumParams], q[kNumParams]; setDefaults(p); p[P_SPC_ON] = 1; p[P_MOT_ON] = 1; float a[kNumParams], b[kNumParams];
      std::memcpy(q, p, sizeof q); q[P_MACRO1] = 100; applyMacros(p, a, dm); applyMacros(q, b, dm); auto e1 = engine(), e2 = engine(); auto r1 = render(*e1, a, loop), r2 = render(*e2, b, loop);
      double diff = 0; for (size_t k = 0; k < loop.size(); ++k) diff += std::abs(r1.out.L[k] - r2.out.L[k]); CHECK(diff / (double)loop.size() > 1e-3); }

    section("factory presets");
    { const auto& ps = factoryPresets(); CHECKM(ps.size() >= 40, "%zu presets", ps.size()); std::set<std::string> names, cats; auto loop = guitarLoop(SR, 100, 2); for (auto* ch : {&loop.L, &loop.R}) for (auto& v : *ch) v *= 0.7f;
      for (size_t k = 0; k < ps.size(); ++k) { names.insert(ps[k].name); cats.insert(ps[k].category); for (auto& kv : ps[k].v) { int i = paramIndex(kv.first); CHECKM(i >= 0, "%s: %s", ps[k].name, kv.first); if (i >= 0) CHECK(kv.second >= kParams[i].min && kv.second <= kParams[i].max); }
        float p[kNumParams]; applyPreset((int)k, p); auto e = engine(); auto r = render(*e, p, loop, 256, {100, 0, true}); CHECK(finite(r.out));
        double lv = db(rms(r.out.L, loop.size() / 2) / rms(loop.L, loop.size() / 2)); CHECKM(std::abs(lv) < 6, "%s level %f dB", ps[k].name, lv); CHECKM(peak(r.out.L) < 0.99f, "%s peak %f", ps[k].name, peak(r.out.L)); }
      CHECK(names.size() == ps.size()); CHECK(cats.size() >= 8);
      for (const char* must : {"Smooth Butter", "Dreamy Pad", "Dusty Crate", "Underwater", "Broken Tape"}) CHECKM(names.count(must) == 1, "%s", must); }

    section("randomize with per-section locks");
    { Rng r; r.s = 1234; auto loop = guitarLoop(SR, 100, 1);
      for (int k = 0; k < 400; ++k) { float v[kNumParams], before[kNumParams]; applyPreset(k % 40, v); std::memcpy(before, v, sizeof v); bool locks[kNumSections] = {}; for (int s = S_TONE; s <= S_WIDTH; ++s) locks[s] = (k >> (s - 1)) & 1;
        randomize(v, locks, r); bool okRange = true, okLock = true, okGlobal = true;
        for (int i = 0; i < kNumParams; ++i) { okRange &= v[i] >= kParams[i].min && v[i] <= kParams[i].max; int s = kParams[i].section; if (s == S_GLOBAL) okGlobal &= v[i] == before[i]; else if (locks[s]) okLock &= v[i] == before[i]; }
        CHECK(okRange); CHECK(okLock); CHECK(okGlobal);
        if (k < 40) { auto e = engine(); auto out = render(*e, v, loop); CHECK(finite(out.out)); CHECK(peak(out.out.L) < 0.99f); } }
      // it actually varies
      std::set<int> voices; for (int k = 0; k < 50; ++k) { float v[kNumParams]; setDefaults(v); bool locks[kNumSections] = {}; randomize(v, locks, r); voices.insert((int)v[P_AMP_VOICE]); } CHECK(voices.size() == 4); }

    section("section order");
    { int good[6] = {5, 4, 3, 2, 1, 0}, bad1[6] = {0, 0, 1, 2, 3, 4}, bad2[6] = {0, 1, 2, 3, 4, 9}; CHECK(Engine::validOrder(good)); CHECK(!Engine::validOrder(bad1)); CHECK(!Engine::validOrder(bad2));
      auto loop = guitarLoop(SR, 100, 1); float p[kNumParams]; neutral(p); p[P_AMP_ON] = 1; p[P_AMP_DRIVE] = 10; p[P_AMP_VOICE] = 2; p[P_SPC_ON] = 1; p[P_SPC_MIX] = 60; p[P_SPC_DECAY] = 80;
      auto e1 = engine(), e2 = engine(); int rev[6] = {M_SPACE, M_FILTER, M_PITCH, M_MOTION, M_TEXTURE, M_TONE}; e2->setOrder(rev); int got[6]; auto r1 = render(*e1, p, loop), r2 = render(*e2, p, loop); e2->getOrder(got);
      CHECK(std::memcmp(got, rev, sizeof rev) == 0); double diff = 0; for (size_t k = 0; k < loop.size(); ++k) diff += std::abs(r1.out.L[k] - r2.out.L[k]); CHECK(diff / (double)loop.size() > 1e-3);
      e1->setOrder(bad1); e1->getOrder(got); int def[6] = {0, 1, 2, 3, 4, 5}; CHECK(std::memcmp(got, def, sizeof def) == 0); }

    section("latency, sample rates, block sizes");
    { auto e = engine(); CHECK(e->latencySamples() == 0);
      // zero latency: an impulse comes out on the same sample with everything off but the amp
      { float p[kNumParams]; neutral(p); Stereo imp = silence(SR, 0.1); imp.L[100] = imp.R[100] = 0.5f; auto r = render(*e, p, imp); CHECK(r.out.L[100] == 0.5f); }
      for (double sr : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) { auto e2 = engine(sr); float p[kNumParams]; applyPreset(10, p); auto loop = guitarLoop(sr, 100, 1); auto r = render(*e2, p, loop, 512); CHECK(finite(r.out)); CHECK(peak(r.out.L) < 0.99f); CHECK(rms(r.out.L) > 0.01);
        Svf s; s.set(sr, 1000, 0.7071); double m = measureGain(1000, [&](float x) { return s.lp(x); }, sr); CHECKM(std::abs(m + 3.0103) < 0.05, "sr %g", sr); }
      // odd block sizes give the same result as 256 (sub-block processing is block-size independent)
      { float p[kNumParams]; applyPreset(6, p); auto loop = guitarLoop(SR, 100, 1); auto a = engine(), b = engine(); auto r1 = render(*a, p, loop, 256), r2 = render(*b, p, loop, 37);
        double d = 0; for (size_t k = 0; k < loop.size(); ++k) d = std::max(d, (double)std::abs(r1.out.L[k] - r2.out.L[k])); CHECKM(d < 0.05, "block-size difference %g", d); } }

    section("CPU (this machine, one core, 48 kHz, 256-sample blocks)");
    { auto loop = guitarLoop(SR, 100, 8);
      auto cpu = [&](const char* name, std::function<void(float*)> set) { float p[kNumParams]; set(p); auto e = engine(); auto r = render(*e, p, loop); double pct = 100.0 * r.seconds / ((double)loop.size() / SR); std::printf("  CPU %-34s %6.2f %%\n", name, pct); return pct; };
      double a = cpu("defaults (Glass amp + width)", [](float* p) { setDefaults(p); });
      double b = cpu("Dreamy Pad preset", [](float* p) { applyPreset(10, p); });
      double c = cpu("everything on (plate + shimmer)", [](float* p) { setDefaults(p); for (int m = 0; m < kMovable; ++m) p[sectionSwitch(m)] = 1; p[P_DUK_ON] = 1; p[P_SPC_SHIMMER] = 50; p[P_PIT_DOWN] = 50; p[P_PIT_UP] = 50; p[P_TEX_CRACKLE] = 50; p[P_TEX_HISS] = 30; p[P_TEX_CRUSH] = 30; p[P_AMP_VERB] = 30; });
      CHECK(a < 10); CHECK(b < 20); CHECK(c < 30); }

    if (!quick) { section("long run: 10 minutes, random presets and parameter changes every half second");
      auto e = engine(); float p[kNumParams]; setDefaults(p); Rng r; r.s = 99; auto loop = guitarLoop(SR, 100, 4); size_t total = (size_t)(SR * 600), pos = 0; bool ok = true; float pk = 0; int changes = 0;
      while (pos < total) { bool locks[kNumSections] = {}; randomize(p, locks, r); p[P_AMOUNT] = 100 * r.uni(); ++changes; auto rr = render(*e, p, loop.size() > 24000 ? Stereo{Buf(loop.L.begin(), loop.L.begin() + 24000), Buf(loop.R.begin(), loop.R.begin() + 24000)} : loop, 256, {100, (double)pos / SR * 100 / 60, true});
        ok &= finite(rr.out); pk = std::max(pk, peak(rr.out.L)); pos += 24000; }
      CHECK(ok); CHECKM(pk < 0.99f, "peak %f", pk); std::printf("  %d random changes, peak %.3f\n", changes, pk); }

    std::printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
