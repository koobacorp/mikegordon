// MIKE GORDON — small DSP building blocks (textbook forms, original code).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mg {
constexpr double kPi = 3.14159265358979323846;
constexpr float kPif = 3.14159265f;
inline float dbToGain(float db) { return std::pow(10.f, db * 0.05f); }
inline float gainToDb(float g) { return 20.f * std::log10(std::max(g, 1e-12f)); }
inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float flush(float x) { return std::abs(x) < 1e-18f ? 0.f : x; }
/// Rational tanh approximation (Pade 7/6 clamp), accurate to ~1e-4 inside +-3, monotonic, exact +-1 limits.
inline float fastTanh(float x) {
    if (x > 4.97f) return 1.f; if (x < -4.97f) return -1.f;
    float x2 = x * x; float a = x * (135135.f + x2 * (17325.f + x2 * (378.f + x2))); float b = 135135.f + x2 * (62370.f + x2 * (3150.f + x2 * 28.f));
    return clampf(a / b, -1.f, 1.f);
}
/// Smooth soft clipper with unity slope at 0, never above |1|.
inline float softClip(float x) { return fastTanh(x); }

/// One-pole smoother; snaps exactly to its target so "0" and "1" are bit-exact.
struct Smooth {
    float v = 0, t = 0, a = 0.f;
    void setTime(double sr, double sec) { a = sec <= 0 ? 0.f : (float)std::exp(-1.0 / (sr * sec)); }
    void reset(float x) { v = t = x; }
    void target(float x) { t = x; }
    inline float next() { if (v != t) { float nv = t + (v - t) * a; if (nv == v || std::abs(nv - t) < 1e-6f) nv = t; v = nv; } return v; }
    bool settled() const { return v == t; }
    /// advance n samples at once (control-rate use)
    inline float skip(int n) { if (v != t) { float nv = t + (v - t) * std::pow(a, (float)n); if (nv == v || std::abs(nv - t) < 1e-6f) nv = t; v = nv; } return v; }
};

/// Linear ramp 0..1 used for on/off and crossfades (equal-gain, click-free).
struct Ramp {
    float v = 0, step = 0.001f; bool up = false;
    void setTime(double sr, double sec) { step = (float)(1.0 / std::max(1.0, sr * sec)); }
    inline float next() { if (up) { v += step; if (v > 1) v = 1; } else { v -= step; if (v < 0) v = 0; } return v; }
};

struct Rng {
    uint32_t s = 0x9E3779B9u;
    inline uint32_t u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (u() >> 8) * (1.f / 16777216.f); }       // 0..1
    inline float bi() { return uni() * 2.f - 1.f; }                       // -1..1
};

/// Biquad, transposed direct form II. Coefficients from the RBJ Audio EQ Cookbook.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process(float x) { float y = b0 * x + z1; z1 = flush(b1 * x - a1 * y + z2); z2 = flush(b2 * x - a2 * y); return y; }
    void setNorm(double B0, double B1, double B2, double A0, double A1, double A2) { b0 = (float)(B0 / A0); b1 = (float)(B1 / A0); b2 = (float)(B2 / A0); a1 = (float)(A1 / A0); a2 = (float)(A2 / A0); }
    void lowpass(double sr, double f, double q) { double w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q); setNorm((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass(double sr, double f, double q) { double w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q); setNorm((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void peak(double sr, double f, double q, double db) { double A = std::pow(10, db / 40), w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q); setNorm(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
    void lowShelf(double sr, double f, double db, double S = 1) {
        double A = std::pow(10, db / 40), w = 2 * kPi * f / sr, c = std::cos(w), s = std::sin(w), al = s / 2 * std::sqrt((A + 1 / A) * (1 / S - 1) + 2), sq = 2 * std::sqrt(A) * al;
        setNorm(A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq), (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf(double sr, double f, double db, double S = 1) {
        double A = std::pow(10, db / 40), w = 2 * kPi * f / sr, c = std::cos(w), s = std::sin(w), al = s / 2 * std::sqrt((A + 1 / A) * (1 / S - 1) + 2), sq = 2 * std::sqrt(A) * al;
        setNorm(A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq), (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    /// |H(e^jw)| from the current coefficients (used by tests).
    double mag(double sr, double f) const {
        double w = 2 * kPi * f / sr; double nr = b0 + b1 * std::cos(w) + b2 * std::cos(2 * w), ni = -(b1 * std::sin(w) + b2 * std::sin(2 * w));
        double dr = 1 + a1 * std::cos(w) + a2 * std::cos(2 * w), di = -(a1 * std::sin(w) + a2 * std::sin(2 * w));
        return std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};

/// State-variable filter, topology-preserving transform (bilinear with pre-warping): exact analog prototype response.
struct Svf {
    float g = 0.1f, k = 1.4142f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    void set(double sr, double f, double q) {
        f = std::clamp(f, 5.0, sr * 0.49); g = (float)std::tan(kPi * f / sr); k = (float)(1.0 / q);
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    /// returns low-pass, writes band and high
    inline void tick(float x, float& lp, float& bp, float& hp) {
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flush(2 * v1 - ic1); ic2 = flush(2 * v2 - ic2); lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    inline float lp(float x) { float l, b, h; tick(x, l, b, h); return l; }
    inline float hp(float x) { float l, b, h; tick(x, l, b, h); return h; }
};

/// One-pole low/high pass (matched-z, -3 dB at f).
struct OnePole {
    float a = 0, z = 0;
    void setLP(double sr, double f) { a = (float)std::exp(-2 * kPi * std::clamp(f, 1.0, sr * 0.49) / sr); }
    void reset() { z = 0; }
    inline float lp(float x) { z = flush(x + a * (z - x)); return z; }
    inline float hp(float x) { return x - lp(x); }
};

/// First-order allpass H = (a + z^-1)/(1 + a z^-1); phase is exactly -90 deg at f when set with setFreq.
struct Allpass1 {
    float a = 0, z = 0;
    static float coef(double sr, double f) { double t = std::tan(kPi * std::clamp(f, 5.0, sr * 0.49) / sr); return (float)((t - 1) / (t + 1)); }
    void reset() { z = 0; }
    inline float process(float x) { float y = a * x + z; z = flush(x - a * y); return y; }
};

/// Fractional delay line with cubic (Hermite) interpolation. Delay in samples.
struct Delay {
    std::vector<float> b; int mask = 0, w = 0;
    void init(int maxSamples) { int n = 1; while (n < maxSamples + 4) n <<= 1; b.assign((size_t)n, 0.f); mask = n - 1; w = 0; }
    void reset() { std::fill(b.begin(), b.end(), 0.f); w = 0; }
    inline void push(float x) { b[(size_t)w] = x; w = (w + 1) & mask; }
    /// read d samples behind the most recently pushed sample (d >= 0)
    inline float at(int d) const { return b[(size_t)((w - 1 - d) & mask)]; }
    inline float read(float d) const {
        d = std::max(d, 0.f); int i = (int)d; float f = d - (float)i;
        float xm1 = at(i - 1 < 0 ? 0 : i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
        float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    inline float readLinear(float d) const { d = std::max(d, 0.f); int i = (int)d; float f = d - (float)i; return at(i) + f * (at(i + 1) - at(i)); }
};

/// Schroeder allpass on a delay line (used by the reverbs).
struct DiffuserAP {
    Delay d; float len = 1;
    void init(int maxS) { d.init(maxS); }
    void reset() { d.reset(); }
    inline float process(float x, float g, float mod = 0) { float delayed = d.readLinear(len + mod - 1); float v = x + g * delayed; d.push(flush(v)); return delayed - g * v; }
};

/// LFO phase that can lock to the host beat position.
struct Lfo {
    double ph = 0;
    /// advance by n samples; if beats > 0 and ppq valid, phase follows the host exactly.
    void advanceFree(double hz, double sr, int n = 1) { ph += hz * n / sr; ph -= std::floor(ph); }
    void lockTo(double ppq, double beats, double offset = 0) { double p = ppq / beats + offset; ph = p - std::floor(p); }
    static inline float sine(double p) { return (float)std::sin(2 * kPi * p); }
    static inline float tri(double p) { double q = p - std::floor(p); return (float)(q < 0.5 ? 4 * q - 1 : 3 - 4 * q); }
};

/// Two-tap rotating-delay pitch shifter (Doppler shifter). ratio = output/input frequency.
/// Taps are half a window apart with sin^2/cos^2 windows that sum to exactly 1. Dry path is never delayed.
struct PitchShifter {
    Delay d; float window = 2048, ph = 0;
    void init(int maxS) { d.init(maxS + 8); }
    void setWindow(float samples) { window = samples; }
    void reset() { d.reset(); ph = 0; }
    inline float process(float x, float ratio) {
        d.push(x);
        ph += (1.f - ratio) / window; ph -= std::floor(ph);
        float p2 = ph + 0.5f; p2 -= std::floor(p2);
        float s1 = std::sin(kPif * ph), s2 = std::sin(kPif * p2);
        return d.read(1.f + ph * window) * s1 * s1 + d.read(1.f + p2 * window) * s2 * s2;
    }
};

/// Envelope follower (peak, separate attack/release).
struct EnvFollow {
    float aA = 0, aR = 0, e = 0;
    void set(double sr, double attS, double relS) { aA = (float)std::exp(-1.0 / (sr * attS)); aR = (float)std::exp(-1.0 / (sr * relS)); }
    void reset() { e = 0; }
    inline float process(float x) { x = std::abs(x); float a = x > e ? aA : aR; e = flush(x + a * (e - x)); return e; }
};

/// Tanh with first-order antiderivative anti-aliasing (ADAA1). F(x) = log(cosh(x)).
struct AdaaTanh {
    float x1 = 0; double F1 = 0;
    static inline double logcosh(double x) { double a = std::abs(x); return a + std::log1p(std::exp(-2 * a)) - 0.69314718055994531; }
    void reset() { x1 = 0; F1 = 0; }
    inline float process(float x) {
        double F = logcosh(x); float dx = x - x1; float y;
        if (std::abs(dx) < 1e-4f) y = std::tanh(0.5f * (x + x1)); else y = (float)((F - F1) / dx);
        x1 = x; F1 = F; return y;
    }
};
} // namespace mg
