// MIKE GORDON — factory vibe presets and the Randomize roll (with per-section locks).
#pragma once
#include "Dsp.h"
#include "Params.h"
#include <utility>
#include <vector>

namespace mg {
struct Preset { const char* name; const char* category; std::vector<std::pair<const char*, float>> v; };

inline const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> p = {
        {"Init", "Clean Amps", {}},
        {"Glass Chorus Clean", "Clean Amps", {{"amp_voice", 0}, {"amp_vfx", 60}, {"amp_vrate", 0.7f}, {"wid_width", 130}}},
        {"Warm Trem Room", "Clean Amps", {{"amp_voice", 1}, {"amp_vfx", 45}, {"amp_vrate", 4.5f}, {"amp_verb", 35}}},
        {"Chime Sparkle", "Clean Amps", {{"amp_voice", 2}, {"amp_drive", 5}, {"amp_vfx", 50}, {"amp_treble", 2}}},
        {"Swim Vibrato", "Clean Amps", {{"amp_voice", 3}, {"amp_vfx", 55}, {"amp_vrate", 5.5f}}},
        {"Warm Surf Spring", "Clean Amps", {{"amp_voice", 1}, {"amp_vfx", 0}, {"amp_verb", 70}, {"amp_treble", 3}}},

        {"Smooth Butter", "Smooth", {{"amp_voice", 1}, {"amp_vfx", 0}, {"flt_on", 1}, {"flt_lp", 6000}, {"tex_on", 1}, {"tex_tape", 25}, {"tex_wobble", 10}, {"spc_on", 1}, {"spc_mix", 18}, {"spc_decay", 40}}},
        {"Silk Keys", "Smooth", {{"amp_vfx", 30}, {"mot_on", 1}, {"mot_depth", 35}, {"mot_mix", 35}, {"spc_on", 1}, {"spc_mix", 22}}},
        {"Velvet Room", "Smooth", {{"amp_voice", 1}, {"amp_vfx", 0}, {"amp_bass", 2}, {"amp_treble", -3}, {"spc_on", 1}, {"spc_type", 0}, {"spc_mix", 20}, {"spc_decay", 35}}},
        {"Soft Focus", "Smooth", {{"amp_vfx", 10}, {"flt_on", 1}, {"flt_lp", 4500}, {"flt_res", 5}, {"pit_on", 1}, {"pit_detune", 8}, {"spc_on", 1}, {"spc_mix", 25}}},

        {"Dreamy Pad", "Dreamy", {{"pit_on", 1}, {"pit_detune", 14}, {"mot_on", 1}, {"mot_depth", 60}, {"mot_mix", 50}, {"spc_on", 1}, {"spc_mix", 45}, {"spc_decay", 75}, {"spc_shimmer", 25}, {"wid_width", 150}}},
        {"Cloud Shimmer", "Dreamy", {{"spc_on", 1}, {"spc_mix", 50}, {"spc_decay", 85}, {"spc_shimmer", 60}, {"pit_on", 1}, {"pit_up", 15}, {"pit_detune", 6}}},
        {"Halo Glass", "Dreamy", {{"amp_voice", 0}, {"amp_vfx", 70}, {"spc_on", 1}, {"spc_mix", 40}, {"spc_decay", 70}, {"wid_width", 160}}},
        {"Slow Bloom", "Dreamy", {{"mot_on", 1}, {"mot_type", 2}, {"mot_rate", 0.15f}, {"mot_depth", 70}, {"mot_mix", 50}, {"spc_on", 1}, {"spc_mix", 40}, {"spc_decay", 80}}},
        {"Daydream Echo", "Dreamy", {{"spc_on", 1}, {"spc_type", 2}, {"spc_div", 5}, {"spc_mix", 35}, {"spc_decay", 45}, {"spc_ping", 1}, {"mot_on", 1}, {"mot_depth", 40}}},

        {"Dusty Crate", "Dusty", {{"tex_on", 1}, {"tex_wobble", 30}, {"tex_crackle", 45}, {"tex_hiss", 25}, {"tex_tape", 55}, {"flt_on", 1}, {"flt_hp", 120}, {"flt_lp", 7000}}},
        {"Old Record", "Dusty", {{"tex_on", 1}, {"tex_crackle", 70}, {"tex_hiss", 15}, {"tex_tape", 40}, {"tex_wobble", 15}, {"flt_on", 1}, {"flt_lp", 5000}, {"flt_hp", 200}, {"wid_width", 40}}},
        {"Attic Tape", "Dusty", {{"tex_on", 1}, {"tex_wobble", 55}, {"tex_tape", 70}, {"tex_hiss", 30}}},
        {"Sunbleached", "Dusty", {{"amp_voice", 1}, {"amp_vfx", 0}, {"tex_on", 1}, {"tex_wobble", 35}, {"tex_tape", 50}, {"tex_crush", 10}, {"spc_on", 1}, {"spc_type", 0}, {"spc_mix", 18}}},
        {"Lo-Fi Study", "Dusty", {{"tex_on", 1}, {"tex_wobble", 25}, {"tex_crackle", 30}, {"tex_hiss", 20}, {"tex_tape", 45}, {"tex_crush", 20}, {"flt_on", 1}, {"flt_lp", 6000}, {"flt_res", 15}, {"duk_on", 1}, {"duk_depth", 25}}},

        {"Underwater", "Underwater", {{"flt_on", 1}, {"flt_lp", 600}, {"flt_res", 30}, {"flt_move", 1}, {"flt_div", 2}, {"flt_depth", 35}, {"mot_on", 1}, {"mot_depth", 70}, {"mot_mix", 50}, {"spc_on", 1}, {"spc_mix", 35}}},
        {"Deep Blue", "Underwater", {{"flt_on", 1}, {"flt_lp", 900}, {"flt_res", 20}, {"pit_on", 1}, {"pit_down", 35}, {"pit_detune", 5}, {"spc_on", 1}, {"spc_mix", 40}, {"spc_decay", 70}}},
        {"Submarine Wah", "Underwater", {{"flt_on", 1}, {"flt_lp", 400}, {"flt_res", 60}, {"flt_move", 2}, {"flt_depth", 70}, {"flt_sens", 60}}},
        {"Murky Swim", "Underwater", {{"amp_voice", 3}, {"amp_vfx", 60}, {"amp_vrate", 2.5f}, {"flt_on", 1}, {"flt_lp", 1500}, {"flt_res", 20}}},

        {"Broken Tape", "Broken Tape", {{"tex_on", 1}, {"tex_wobble", 85}, {"tex_tape", 80}, {"tex_crush", 25}, {"tex_hiss", 30}, {"tex_crackle", 20}}},
        {"Chewed Cassette", "Broken Tape", {{"tex_on", 1}, {"tex_wobble", 100}, {"tex_tape", 90}, {"flt_on", 1}, {"flt_hp", 250}, {"flt_lp", 4500}}},
        {"Warped Memory", "Broken Tape", {{"tex_on", 1}, {"tex_wobble", 70}, {"tex_tape", 60}, {"mot_on", 1}, {"mot_type", 1}, {"mot_depth", 40}, {"mot_mix", 30}, {"mot_rate", 0.2f}, {"mot_fb", 30}, {"spc_on", 1}, {"spc_type", 2}, {"spc_mix", 30}, {"spc_decay", 50}}},
        {"Bit Rot", "Broken Tape", {{"tex_on", 1}, {"tex_crush", 65}, {"tex_tape", 40}, {"tex_wobble", 30}}},

        {"Jet Flange", "Motion", {{"mot_on", 1}, {"mot_type", 1}, {"mot_depth", 80}, {"mot_mix", 50}, {"mot_fb", 65}, {"mot_rate", 0.12f}}},
        {"Phase Drift", "Motion", {{"mot_on", 1}, {"mot_type", 2}, {"mot_depth", 70}, {"mot_mix", 50}, {"mot_fb", 50}, {"mot_rate", 0.3f}}},
        {"Swirl Rotor", "Motion", {{"mot_on", 1}, {"mot_type", 3}, {"mot_depth", 75}, {"mot_mix", 50}, {"mot_rate", 1.2f}}},
        {"Pulse Trem", "Motion", {{"mot_on", 1}, {"mot_type", 4}, {"mot_sync", 1}, {"mot_div", 9}, {"mot_depth", 80}, {"mot_mix", 100}, {"mot_spread", 0}}},
        {"Auto-Pan Eighths", "Motion", {{"mot_on", 1}, {"mot_type", 4}, {"mot_sync", 1}, {"mot_div", 9}, {"mot_depth", 100}, {"mot_mix", 100}, {"mot_spread", 100}}},

        {"Pumping Wash", "Rhythmic", {{"spc_on", 1}, {"spc_mix", 45}, {"spc_decay", 70}, {"duk_on", 1}, {"duk_depth", 75}}},
        {"Filter Gate Sweep", "Rhythmic", {{"flt_on", 1}, {"flt_lp", 800}, {"flt_res", 45}, {"flt_move", 1}, {"flt_div", 3}, {"flt_depth", 70}}},
        {"Dub Echo", "Rhythmic", {{"spc_on", 1}, {"spc_type", 2}, {"spc_div", 5}, {"spc_mix", 40}, {"spc_decay", 70}, {"spc_tone", 35}, {"flt_on", 1}, {"flt_hp", 150}}},
        {"Auto-Wah Funk", "Rhythmic", {{"amp_voice", 2}, {"amp_drive", 4}, {"flt_on", 1}, {"flt_lp", 500}, {"flt_res", 40}, {"flt_move", 2}, {"flt_depth", 80}, {"flt_sens", 55}}},

        {"Wide Detune", "Wide", {{"pit_on", 1}, {"pit_detune", 18}, {"wid_width", 170}, {"wid_freq", 150}}},
        {"Octave Pad", "Wide", {{"pit_on", 1}, {"pit_down", 40}, {"pit_up", 25}, {"mot_on", 1}, {"mot_depth", 40}, {"mot_mix", 40}, {"spc_on", 1}, {"spc_mix", 40}, {"spc_decay", 70}}},
        {"Mono Center", "Wide", {{"wid_width", 0}}},
        {"Cathedral Shimmer", "Space", {{"spc_on", 1}, {"spc_mix", 60}, {"spc_decay", 92}, {"spc_shimmer", 35}, {"spc_pre", 40}, {"wid_width", 140}}},
        {"Spring Splash", "Space", {{"spc_on", 1}, {"spc_type", 0}, {"spc_mix", 45}, {"spc_decay", 60}, {"spc_tone", 60}}},
    };
    return p;
}
inline void applyPreset(int idx, float* v) {
    setDefaults(v); const auto& ps = factoryPresets(); if (idx < 0 || idx >= (int)ps.size()) return;
    for (auto& kv : ps[(size_t)idx].v) { int i = paramIndex(kv.first); if (i >= 0) v[i] = kv.second; }
}

/// Which Randomize lock section a parameter belongs to (S_GLOBAL params are never randomised).
inline int lockSection(int param) { return kParams[param].section; }

/// Randomize: tasteful ranges, never touches Amount/Output/macros or locked sections.
inline void randomize(float* v, const bool* locked /*kNumSections*/, Rng& r) {
    auto U = [&](float a, float b) { return a + (b - a) * r.uni(); };
    auto LG = [&](float a, float b) { return a * std::pow(b / a, r.uni()); };
    auto chance = [&](float p) { return r.uni() < p; };
    auto pick = [&](std::initializer_list<int> l) { int k = (int)(r.uni() * (float)l.size()); return (float)*(l.begin() + std::min(k, (int)l.size() - 1)); };
    if (!locked[S_TONE]) {
        v[P_AMP_ON] = chance(0.9f); v[P_AMP_VOICE] = pick({0, 1, 2, 3}); v[P_AMP_DRIVE] = U(0, 5); v[P_AMP_BASS] = U(-4, 4); v[P_AMP_MID] = U(-4, 4); v[P_AMP_TREBLE] = U(-4, 4);
        v[P_AMP_BRIGHT] = chance(0.25f); v[P_AMP_VFX] = U(10, 80); v[P_AMP_VRATE] = LG(0.3f, 6); v[P_AMP_VERB] = chance(0.5f) ? U(0, 40) : 0;
    }
    if (!locked[S_FILTER]) {
        v[P_FLT_ON] = chance(0.4f); v[P_FLT_LP] = LG(600, 16000); v[P_FLT_HP] = LG(20, 300); v[P_FLT_RES] = U(0, 50);
        float m = r.uni(); v[P_FLT_MOVE] = m < 0.5f ? 0 : (m < 0.8f ? 1 : 2); v[P_FLT_SYNC] = 1; v[P_FLT_DIV] = pick({2, 3, 6, 9}); v[P_FLT_RATE] = LG(0.1f, 3); v[P_FLT_DEPTH] = U(20, 70); v[P_FLT_SENS] = U(30, 70);
    }
    if (!locked[S_PITCH]) {
        v[P_PIT_ON] = chance(0.3f); v[P_PIT_DOWN] = chance(0.4f) ? U(0, 40) : 0; v[P_PIT_UP] = chance(0.3f) ? U(0, 30) : 0; v[P_PIT_DETUNE] = U(4, 20); v[P_PIT_WIDTH] = U(60, 100);
    }
    if (!locked[S_MOTION]) {
        v[P_MOT_ON] = chance(0.55f); v[P_MOT_TYPE] = pick({0, 1, 2, 3, 4}); v[P_MOT_SYNC] = chance(0.5f); v[P_MOT_DIV] = pick({2, 3, 6, 9}); v[P_MOT_RATE] = LG(0.1f, 3);
        v[P_MOT_DEPTH] = U(20, 80); v[P_MOT_MIX] = v[P_MOT_TYPE] == 4 ? 100 : U(30, 60); v[P_MOT_FB] = U(0, 50); v[P_MOT_SPREAD] = U(20, 100);
    }
    if (!locked[S_TEXTURE]) {
        v[P_TEX_ON] = chance(0.5f); v[P_TEX_WOBBLE] = U(0, 50); v[P_TEX_CRACKLE] = chance(0.5f) ? U(0, 50) : 0; v[P_TEX_HISS] = chance(0.5f) ? U(0, 30) : 0;
        v[P_TEX_CRUSH] = chance(0.25f) ? U(0, 35) : 0; v[P_TEX_TAPE] = U(10, 60); v[P_TEX_GATE] = 1;
    }
    if (!locked[S_SPACE]) {
        v[P_SPC_ON] = chance(0.7f); v[P_SPC_TYPE] = pick({0, 1, 2}); v[P_SPC_MIX] = U(15, 45); v[P_SPC_DECAY] = U(30, 80); v[P_SPC_TONE] = U(30, 70); v[P_SPC_PRE] = U(0, 40);
        v[P_SPC_SYNC] = 1; v[P_SPC_DIV] = pick({5, 6, 8, 9}); v[P_SPC_SHIMMER] = chance(0.3f) ? U(0, 40) : 0; v[P_SPC_FREEZE] = 0; v[P_SPC_PING] = chance(0.4f);
    }
    if (!locked[S_WIDTH]) {
        v[P_WID_ON] = 1; v[P_WID_WIDTH] = U(80, 170); v[P_WID_MONO] = 1; v[P_WID_FREQ] = U(80, 200); v[P_DUK_ON] = chance(0.25f); v[P_DUK_SRC] = 0;
        v[P_DUK_DEPTH] = U(20, 60); v[P_DUK_DIV] = pick({6, 9}); v[P_DUK_SHAPE] = U(30, 70);
    }
    for (int i = 0; i < kNumParams; ++i) { const auto& d = kParams[i]; v[i] = clampf(v[i], d.min, d.max); if (d.kind != K_FLOAT) v[i] = std::round(v[i]); }
}
} // namespace mg
