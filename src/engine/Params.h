// MIKE GORDON — parameter table. One table drives the engine, the host parameters, presets, randomize and the UI.
#pragma once
#include <cmath>
#include <cstring>

namespace mg {

enum Section { S_GLOBAL, S_TONE, S_FILTER, S_PITCH, S_MOTION, S_TEXTURE, S_SPACE, S_WIDTH, kNumSections };
inline const char* sectionName(int s) {
    static const char* n[] = {"Output", "Tone", "Filter", "Pitch", "Motion", "Texture", "Space", "Width"};
    return n[s];
}
/// Sections that can be dragged into a different order (Width/Ducking always sits last).
enum Movable { M_TONE, M_FILTER, M_PITCH, M_MOTION, M_TEXTURE, M_SPACE, kMovable };

enum Kind { K_FLOAT, K_BOOL, K_CHOICE };

inline const char* const kVoices[] = {"Glass", "Warm", "Chime", "Swim"};
inline const char* const kMoveModes[] = {"Off", "LFO", "Envelope"};
inline const char* const kMotionTypes[] = {"Chorus", "Flanger", "Phaser", "Swirl", "Tremolo"};
inline const char* const kSpaceTypes[] = {"Spring", "Plate", "Echo"};
inline const char* const kDuckSources[] = {"Tempo pump", "Sidechain"};
inline const char* const kDuckTargets[] = {"Effect only", "Everything"};
inline const char* const kDivs[] = {"4 bars", "2 bars", "1 bar", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32"};
inline constexpr double kDivBeats[] = {16, 8, 4, 2, 4.0 / 3.0, 1.5, 1, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125};
inline constexpr int kNumDivs = 15;

struct ParamDef {
    const char* id; const char* name; int section; int kind;
    float min, max, def;
    float centre;            // skew centre for log-ish knobs (0 = linear)
    const char* unit;
    const char* const* choices; int nChoices;
    bool main;               // the big knob of its section
};

enum P {
    // global / output
    P_AMOUNT, P_OUTGAIN, P_MATCH, P_LIMIT, P_COMPARE, P_BYPASS, P_MACRO1, P_MACRO2, P_MACRO3,
    // tone
    P_AMP_ON, P_AMP_VOICE, P_AMP_DRIVE, P_AMP_BASS, P_AMP_MID, P_AMP_TREBLE, P_AMP_BRIGHT, P_AMP_VFX, P_AMP_VRATE, P_AMP_VERB,
    // filter
    P_FLT_ON, P_FLT_LP, P_FLT_HP, P_FLT_RES, P_FLT_MOVE, P_FLT_SYNC, P_FLT_DIV, P_FLT_RATE, P_FLT_DEPTH, P_FLT_SENS,
    // pitch
    P_PIT_ON, P_PIT_DOWN, P_PIT_UP, P_PIT_DETUNE, P_PIT_WIDTH,
    // motion
    P_MOT_ON, P_MOT_TYPE, P_MOT_SYNC, P_MOT_DIV, P_MOT_RATE, P_MOT_DEPTH, P_MOT_MIX, P_MOT_FB, P_MOT_SPREAD,
    // texture
    P_TEX_ON, P_TEX_WOBBLE, P_TEX_CRACKLE, P_TEX_HISS, P_TEX_CRUSH, P_TEX_TAPE, P_TEX_GATE,
    // space
    P_SPC_ON, P_SPC_TYPE, P_SPC_MIX, P_SPC_DECAY, P_SPC_TONE, P_SPC_PRE, P_SPC_SYNC, P_SPC_DIV, P_SPC_TIME, P_SPC_SHIMMER, P_SPC_FREEZE, P_SPC_PING,
    // width + ducking
    P_WID_ON, P_WID_WIDTH, P_WID_MONO, P_WID_FREQ, P_DUK_ON, P_DUK_SRC, P_DUK_DEPTH, P_DUK_DIV, P_DUK_SHAPE, P_DUK_THRESH, P_DUK_RELEASE, P_DUK_TARGET,
    kNumParams
};

#define MG_F(id, nm, sec, mn, mx, df, ctr, unit) {id, nm, sec, K_FLOAT, mn, mx, df, ctr, unit, nullptr, 0, false}
#define MG_M(id, nm, sec, mn, mx, df, ctr, unit) {id, nm, sec, K_FLOAT, mn, mx, df, ctr, unit, nullptr, 0, true}
#define MG_B(id, nm, sec, df) {id, nm, sec, K_BOOL, 0, 1, df, 0, "", nullptr, 0, false}
#define MG_C(id, nm, sec, arr, df) {id, nm, sec, K_CHOICE, 0, (float)(sizeof(arr) / sizeof(arr[0]) - 1), df, 0, "", arr, (int)(sizeof(arr) / sizeof(arr[0])), false}

inline const ParamDef kParams[kNumParams] = {
    MG_M("amount", "Amount", S_GLOBAL, 0, 100, 100, 0, "%"),
    MG_F("out_gain", "Output", S_GLOBAL, -24, 12, 0, 0, "dB"),
    MG_B("match", "Level Match", S_GLOBAL, 1),
    MG_B("limit", "Safety Limiter", S_GLOBAL, 1),
    MG_B("compare", "Before (hear original)", S_GLOBAL, 0),
    MG_B("bypass", "Bypass", S_GLOBAL, 0),
    MG_F("macro1", "Macro 1", S_GLOBAL, 0, 100, 0, 0, "%"),
    MG_F("macro2", "Macro 2", S_GLOBAL, 0, 100, 0, 0, "%"),
    MG_F("macro3", "Macro 3", S_GLOBAL, 0, 100, 0, 0, "%"),

    MG_B("amp_on", "Amp On", S_TONE, 1),
    MG_C("amp_voice", "Amp Voice", S_TONE, kVoices, 0),
    MG_M("amp_drive", "Drive", S_TONE, 0, 10, 2, 0, ""),
    MG_F("amp_bass", "Bass", S_TONE, -12, 12, 0, 0, "dB"),
    MG_F("amp_mid", "Mid", S_TONE, -12, 12, 0, 0, "dB"),
    MG_F("amp_treble", "Treble", S_TONE, -12, 12, 0, 0, "dB"),
    MG_B("amp_bright", "Bright", S_TONE, 0),
    MG_F("amp_vfx", "Voice FX", S_TONE, 0, 100, 40, 0, "%"),
    MG_F("amp_vrate", "Voice Speed", S_TONE, 0.1f, 10, 1.2f, 1.5f, "Hz"),
    MG_F("amp_verb", "Amp Spring", S_TONE, 0, 100, 0, 0, "%"),

    MG_B("flt_on", "Filter On", S_FILTER, 0),
    MG_M("flt_lp", "Low-pass", S_FILTER, 20, 20000, 20000, 1000, "Hz"),
    MG_F("flt_hp", "High-pass", S_FILTER, 20, 20000, 20, 1000, "Hz"),
    MG_F("flt_res", "Resonance", S_FILTER, 0, 100, 10, 0, "%"),
    MG_C("flt_move", "Movement", S_FILTER, kMoveModes, 0),
    MG_B("flt_sync", "Filter Sync", S_FILTER, 1),
    MG_C("flt_div", "Filter Sync Rate", S_FILTER, kDivs, 2),
    MG_F("flt_rate", "Filter Rate", S_FILTER, 0.05f, 20, 1, 1, "Hz"),
    MG_F("flt_depth", "Move Depth", S_FILTER, 0, 100, 50, 0, "%"),
    MG_F("flt_sens", "Envelope Sens", S_FILTER, 0, 100, 50, 0, "%"),

    MG_B("pit_on", "Pitch On", S_PITCH, 0),
    MG_F("pit_down", "Octave Down", S_PITCH, 0, 100, 0, 0, "%"),
    MG_F("pit_up", "Octave Up", S_PITCH, 0, 100, 0, 0, "%"),
    MG_M("pit_detune", "Detune", S_PITCH, 0, 50, 12, 0, "ct"),
    MG_F("pit_width", "Detune Width", S_PITCH, 0, 100, 100, 0, "%"),

    MG_B("mot_on", "Motion On", S_MOTION, 0),
    MG_C("mot_type", "Motion Type", S_MOTION, kMotionTypes, 0),
    MG_B("mot_sync", "Motion Sync", S_MOTION, 0),
    MG_C("mot_div", "Motion Sync Rate", S_MOTION, kDivs, 2),
    MG_F("mot_rate", "Motion Rate", S_MOTION, 0.05f, 10, 0.8f, 1, "Hz"),
    MG_M("mot_depth", "Motion Depth", S_MOTION, 0, 100, 50, 0, "%"),
    MG_F("mot_mix", "Motion Mix", S_MOTION, 0, 100, 50, 0, "%"),
    MG_F("mot_fb", "Feedback", S_MOTION, 0, 90, 0, 0, "%"),
    MG_F("mot_spread", "Stereo Spread", S_MOTION, 0, 100, 50, 0, "%"),

    MG_B("tex_on", "Texture On", S_TEXTURE, 0),
    MG_M("tex_wobble", "Tape Wobble", S_TEXTURE, 0, 100, 25, 0, "%"),
    MG_F("tex_crackle", "Crackle", S_TEXTURE, 0, 100, 0, 0, "%"),
    MG_F("tex_hiss", "Hiss", S_TEXTURE, 0, 100, 0, 0, "%"),
    MG_F("tex_crush", "Bit Crush", S_TEXTURE, 0, 100, 0, 0, "%"),
    MG_F("tex_tape", "Cassette", S_TEXTURE, 0, 100, 20, 0, "%"),
    MG_B("tex_gate", "Noise Follows Input", S_TEXTURE, 1),

    MG_B("spc_on", "Space On", S_SPACE, 0),
    MG_C("spc_type", "Space Type", S_SPACE, kSpaceTypes, 1),
    MG_M("spc_mix", "Space Mix", S_SPACE, 0, 100, 30, 0, "%"),
    MG_F("spc_decay", "Decay", S_SPACE, 0, 100, 50, 0, "%"),
    MG_F("spc_tone", "Space Tone", S_SPACE, 0, 100, 55, 0, "%"),
    MG_F("spc_pre", "Pre-delay", S_SPACE, 0, 150, 10, 0, "ms"),
    MG_B("spc_sync", "Echo Sync", S_SPACE, 1),
    MG_C("spc_div", "Echo Sync Rate", S_SPACE, kDivs, 8),
    MG_F("spc_time", "Echo Time", S_SPACE, 20, 1500, 375, 300, "ms"),
    MG_F("spc_shimmer", "Shimmer", S_SPACE, 0, 100, 0, 0, "%"),
    MG_B("spc_freeze", "Freeze", S_SPACE, 0),
    MG_B("spc_ping", "Ping-pong", S_SPACE, 0),

    MG_B("wid_on", "Width On", S_WIDTH, 1),
    MG_M("wid_width", "Width", S_WIDTH, 0, 200, 100, 0, "%"),
    MG_B("wid_mono", "Mono Bass", S_WIDTH, 1),
    MG_F("wid_freq", "Mono Below", S_WIDTH, 40, 400, 120, 120, "Hz"),
    MG_B("duk_on", "Ducking On", S_WIDTH, 0),
    MG_C("duk_src", "Duck Source", S_WIDTH, kDuckSources, 0),
    MG_F("duk_depth", "Duck Depth", S_WIDTH, 0, 100, 50, 0, "%"),
    MG_C("duk_div", "Pump Rate", S_WIDTH, kDivs, 6),
    MG_F("duk_shape", "Pump Shape", S_WIDTH, 0, 100, 50, 0, "%"),
    MG_F("duk_thresh", "Duck Threshold", S_WIDTH, -60, 0, -24, 0, "dB"),
    MG_F("duk_release", "Duck Release", S_WIDTH, 20, 1000, 150, 150, "ms"),
    MG_C("duk_target", "Duck Target", S_WIDTH, kDuckTargets, 0),
};
#undef MG_F
#undef MG_M
#undef MG_B
#undef MG_C

inline int paramIndex(const char* id) { for (int i = 0; i < kNumParams; ++i) if (std::strcmp(kParams[i].id, id) == 0) return i; return -1; }
inline void setDefaults(float* v) { for (int i = 0; i < kNumParams; ++i) v[i] = kParams[i].def; }

/// Same skew law JUCE NormalisableRange::setSkewForCentre uses, so engine-side normalisation matches the host.
inline float skewFor(const ParamDef& d) { if (d.centre <= 0) return 1.f; return (float)(std::log(0.5) / std::log((d.centre - d.min) / (d.max - d.min))); }
inline float toNorm(int i, float plain) {
    const auto& d = kParams[i]; float x = (plain - d.min) / (d.max - d.min); x = x < 0 ? 0 : (x > 1 ? 1 : x);
    float s = skewFor(d); return s == 1.f ? x : std::pow(x, s);
}
inline float fromNorm(int i, float n) {
    const auto& d = kParams[i]; n = n < 0 ? 0 : (n > 1 ? 1 : n); float s = skewFor(d);
    float x = s == 1.f ? n : std::pow(n, 1.f / s); float v = d.min + x * (d.max - d.min);
    if (d.kind != K_FLOAT) v = std::round(v);
    return v;
}
/// The on/off switch of each movable section.
inline int sectionSwitch(int movable) { static const int s[] = {P_AMP_ON, P_FLT_ON, P_PIT_ON, P_MOT_ON, P_TEX_ON, P_SPC_ON}; return s[movable]; }

/// Macro assignments: 3 macros x 4 slots. depth is in normalised units (-1..1) added at macro = 100%.
struct MacroSlot { int param = -1; float depth = 0; };
constexpr int kMacros = 3, kMacroSlots = 4;
struct MacroMap { MacroSlot s[kMacros][kMacroSlots]; };

/// Apply macros: in = plain base values, out = plain values the engine uses.
inline void applyMacros(const float* in, float* out, const MacroMap& m) {
    for (int i = 0; i < kNumParams; ++i) out[i] = in[i];
    for (int k = 0; k < kMacros; ++k) {
        float amt = in[P_MACRO1 + k] * 0.01f; if (amt <= 0) continue;
        for (auto& sl : m.s[k]) if (sl.param >= 0 && sl.param < kNumParams && kParams[sl.param].kind == K_FLOAT && sl.param > P_MACRO3)
            out[sl.param] = fromNorm(sl.param, toNorm(sl.param, out[sl.param]) + sl.depth * amt);
    }
}
inline MacroMap defaultMacros() {
    MacroMap m;
    m.s[0][0] = {P_SPC_MIX, 0.45f}; m.s[0][1] = {P_MOT_DEPTH, 0.35f}; m.s[0][2] = {P_WID_WIDTH, 0.2f};          // Dream
    m.s[1][0] = {P_AMP_TREBLE, -0.35f}; m.s[1][1] = {P_FLT_LP, -0.45f}; m.s[1][2] = {P_FLT_RES, 0.2f};                                               // Dark
    m.s[2][0] = {P_TEX_WOBBLE, 0.5f}; m.s[2][1] = {P_TEX_TAPE, 0.45f}; m.s[2][2] = {P_TEX_CRACKLE, 0.3f};          // Age
    return m;
}
inline const char* defaultMacroName(int k) { static const char* n[] = {"Dream", "Dark", "Age"}; return n[k]; }

} // namespace mg
