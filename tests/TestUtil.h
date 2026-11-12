// MIKE GORDON — shared test helpers: rendering, measurement, and a synthetic guitar loop.
#pragma once
#include "Engine.h"
#include "Presets.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tu {
using Buf = std::vector<float>;
struct Stereo { Buf L, R; size_t size() const { return L.size(); } };

inline Stereo sine(double sr, double f, double sec, float amp = 0.5f, bool antiphase = false) {
    Stereo s; size_t n = (size_t)(sr * sec); s.L.resize(n); s.R.resize(n);
    for (size_t i = 0; i < n; ++i) { float v = amp * (float)std::sin(2 * mg::kPi * f * (double)i / sr); s.L[i] = v; s.R[i] = antiphase ? -v : v; }
    return s;
}
inline Stereo noise(double sr, double sec, float amp, uint32_t seed = 1) { mg::Rng r; r.s = seed; Stereo s; size_t n = (size_t)(sr * sec); s.L.resize(n); s.R.resize(n); for (size_t i = 0; i < n; ++i) { s.L[i] = amp * r.bi(); s.R[i] = amp * r.bi(); } return s; }
inline Stereo silence(double sr, double sec) { Stereo s; s.L.assign((size_t)(sr * sec), 0.f); s.R = s.L; return s; }
inline void append(Stereo& a, const Stereo& b) { a.L.insert(a.L.end(), b.L.begin(), b.L.end()); a.R.insert(a.R.end(), b.R.begin(), b.R.end()); }

/// Plucked-string (Karplus-Strong) guitar loop: a 4-bar chord progression, strummed, at the given tempo.
inline Stereo guitarLoop(double sr, double bpm = 90, int bars = 4, uint32_t seed = 7) {
    double beat = 60.0 / bpm; size_t n = (size_t)(sr * beat * 4 * bars); Stereo s; s.L.assign(n, 0); s.R.assign(n, 0);
    // chords as MIDI notes (original voicings): Am7, Fmaj7, Cadd9, G6
    const int ch[4][5] = {{45, 52, 55, 60, 64}, {41, 48, 52, 57, 64}, {48, 55, 60, 62, 64}, {43, 50, 55, 59, 64}};
    mg::Rng r; r.s = seed;
    const double pattern[] = {0, 1.5, 2, 3, 3.5};   // strum positions (beats) within a bar
    for (int b = 0; b < bars; ++b) for (double pos : pattern) {
        bool down = pos == 0 || pos == 2 || pos == 3; double vel = pos == 0 ? 1.0 : 0.7;
        for (int k = 0; k < 5; ++k) {
            int note = ch[b % 4][down ? k : 4 - k]; double f = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            size_t start = (size_t)((b * 4 + pos) * beat * sr + k * 0.012 * sr); int P = std::max(2, (int)std::lround(sr / f));
            std::vector<float> line((size_t)P); for (auto& x : line) x = r.bi();
            float pan = 0.3f + 0.1f * (float)k; double dur = beat * (pos == 0 ? 1.4 : 0.9); size_t len = (size_t)(dur * sr); float prev = 0; size_t idx = 0;
            for (size_t i = 0; i < len && start + i < n; ++i) {
                float cur = line[idx]; float nx = 0.996f * 0.5f * (cur + prev); prev = cur; line[idx] = nx; idx = (idx + 1) % (size_t)P;
                float env = i > len - (size_t)(0.03 * sr) ? (float)(len - i) / (float)(0.03 * sr) : 1.f;
                float v = (float)(0.16 * vel) * cur * env; s.L[start + i] += v * (1 - pan) * 1.4f; s.R[start + i] += v * pan * 1.4f;
            }
        }
    }
    return s;
}

struct Run { Stereo out; double seconds = 0; };
struct Host { double bpm = 120, ppq = 0; bool playing = true; };
/// Render through an engine in blocks; `onBlock(sampleIndex, params)` may change parameters between blocks.
inline Run render(mg::Engine& e, float* p, const Stereo& in, int block = 256, Host h = {}, std::function<void(size_t, float*)> onBlock = nullptr, const Stereo* sc = nullptr) {
    Run r; r.out = in; size_t n = in.size(); double sr = e.sampleRate();
    for (size_t pos = 0; pos < n; pos += (size_t)block) {
        int m = (int)std::min<size_t>((size_t)block, n - pos);
        if (onBlock) onBlock(pos, p);
        mg::HostInfo hi; hi.bpm = h.bpm; hi.playing = h.playing; hi.hasPpq = true; hi.ppq = h.ppq + (double)pos * h.bpm / 60.0 / sr;
        auto t0 = std::chrono::steady_clock::now();
        e.process(r.out.L.data() + pos, r.out.R.data() + pos, m, sc ? sc->L.data() + pos : nullptr, sc ? sc->R.data() + pos : nullptr, hi, p);
        r.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }
    return r;
}
inline std::unique_ptr<mg::Engine> engine(double sr = 48000) { auto e = std::make_unique<mg::Engine>(); e->prepare(sr, 512); return e; }
/// All sections off, width off: a neutral starting point for isolating one section.
inline void neutral(float* p) { mg::setDefaults(p); p[mg::P_AMP_ON] = 0; p[mg::P_WID_ON] = 0; p[mg::P_MATCH] = 0; p[mg::P_LIMIT] = 0; }

inline double rms(const Buf& x, size_t a = 0, size_t b = SIZE_MAX) { b = std::min(b, x.size()); double s = 0; for (size_t i = a; i < b; ++i) s += (double)x[i] * x[i]; return b > a ? std::sqrt(s / (double)(b - a)) : 0; }
inline float peak(const Buf& x, size_t a = 0, size_t b = SIZE_MAX) { b = std::min(b, x.size()); float p = 0; for (size_t i = a; i < b; ++i) p = std::max(p, std::abs(x[i])); return p; }
inline bool finite(const Stereo& s) { for (float v : s.L) if (!std::isfinite(v)) return false; for (float v : s.R) if (!std::isfinite(v)) return false; return true; }
inline int subnormals(const Stereo& s) { int c = 0; for (float v : s.L) c += std::fpclassify(v) == FP_SUBNORMAL; for (float v : s.R) c += std::fpclassify(v) == FP_SUBNORMAL; return c; }
/// complex amplitude of frequency f in x[a..b) (single-bin DFT), relative to a unit-amplitude sine
inline std::complex<double> bin(const Buf& x, double sr, double f, size_t a, size_t b) {
    std::complex<double> acc = 0; for (size_t i = a; i < b; ++i) acc += (double)x[i] * std::exp(std::complex<double>(0, -2 * mg::kPi * f * (double)i / sr));
    return acc * (2.0 / (double)(b - a));
}
inline double db(double g) { return 20 * std::log10(std::max(g, 1e-12)); }
/// mean frequency from zero crossings in [a,b)
inline double zcFreq(const Buf& x, double sr, size_t a, size_t b) {
    double first = -1, last = -1; int cnt = 0;
    for (size_t i = a + 1; i < b; ++i) if (x[i - 1] <= 0 && x[i] > 0) { double t = (double)(i - 1) + (double)(-x[i - 1] / (x[i] - x[i - 1])); if (first < 0) first = t; last = t; ++cnt; }
    return cnt > 1 ? (cnt - 1) * sr / (last - first) : 0;
}
/// max |second difference| in [a,b): spikes here are clicks
inline float maxD2(const Buf& x, size_t a, size_t b) { float m = 0; for (size_t i = std::max<size_t>(a, 2); i < std::min(b, x.size()); ++i) m = std::max(m, std::abs(x[i] - 2 * x[i - 1] + x[i - 2])); return m; }

inline void writeWav(const std::string& path, const Stereo& s, double sr) {
    FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return;
    uint32_t n = (uint32_t)s.size(), data = n * 2 * 3, riff = 36 + data, sr32 = (uint32_t)sr, br = sr32 * 6; uint16_t fmt = 1, ch = 2, ba = 6, bits = 24; uint32_t fs = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f); std::fwrite(&fs, 4, 1, f); std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f);
    std::fwrite(&sr32, 4, 1, f); std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f); std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) { float v = mg::clampf(c ? s.R[i] : s.L[i], -1, 1); int32_t q = (int32_t)std::lround(v * 8388607.0); uint8_t b[3] = {(uint8_t)(q & 255), (uint8_t)((q >> 8) & 255), (uint8_t)((q >> 16) & 255)}; std::fwrite(b, 1, 3, f); }
    std::fclose(f);
}
} // namespace tu
