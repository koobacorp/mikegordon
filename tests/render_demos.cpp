// MIKE GORDON — renders demo WAVs: a synthetic plucked-guitar loop through several factory presets.
#include "TestUtil.h"
#include <cstring>
using namespace mg; using namespace tu;
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "."; const double sr = 48000, bpm = 90;
    auto loop = guitarLoop(sr, bpm, 4); float pk = std::max(peak(loop.L), peak(loop.R)); for (auto* ch : {&loop.L, &loop.R}) for (auto& v : *ch) v *= 0.6f / pk;   // peak -4.4 dBFS
    Stereo src = loop; append(src, loop);                      // 8 bars
    writeWav(dir + "/MIKE-GORDON-demo-00-dry-guitar-loop.wav", src, sr);
    const char* picks[] = {"Glass Chorus Clean", "Warm Trem Room", "Smooth Butter", "Dreamy Pad", "Dusty Crate", "Underwater", "Broken Tape", "Jet Flange", "Pumping Wash", "Dub Echo", "Cathedral Shimmer", "Auto-Wah Funk"};
    int n = 1; const auto& ps = factoryPresets();
    for (const char* name : picks) {
        int idx = -1; for (size_t i = 0; i < ps.size(); ++i) if (!std::strcmp(ps[i].name, name)) idx = (int)i; if (idx < 0) continue;
        float p[kNumParams]; applyPreset(idx, p); auto e = engine(sr); Stereo tail = src; append(tail, silence(sr, 2.0));
        auto r = render(*e, p, tail, 256, {bpm, 0, true});
        std::string fn = name; for (auto& c : fn) { if (c == ' ') c = '-'; } char buf[512]; std::snprintf(buf, sizeof buf, "%s/MIKE-GORDON-demo-%02d-%s.wav", dir.c_str(), n++, fn.c_str());
        writeWav(buf, r.out, sr); std::printf("%s  peak %.3f  rms %.1f dB\n", buf, peak(r.out.L), db(rms(r.out.L)));
    }
}
