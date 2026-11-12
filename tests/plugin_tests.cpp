// MIKE GORDON — JUCE-level tests: identity, parameters (ranges match the engine), buses incl. sidechain and mono, bypass,
// state, presets, macros, order, randomize locks, editor build/resizing + screenshots, and CPU. Runs headless on the CI Mac.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <chrono>
#include <cstdio>
using namespace mg;

static int passed = 0, failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s | ", __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Head : juce::AudioPlayHead {
    bool playing = true; double bpm = 120, ppq = 0;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying(playing); p.setBpm(bpm); p.setPpqPosition(ppq); return p; }
};
struct Rig {
    MikeGordonProcessor p; Head head; double sr; int bs;
    Rig(double rate = 48000, int block = 256, bool sidechain = false) : sr(rate), bs(block) {
        if (sidechain) { auto l = p.getBusesLayout(); l.inputBuses.getReference(1) = juce::AudioChannelSet::stereo(); p.setBusesLayout(l); }
        p.setPlayHead(&head); p.setRateAndBufferSizeDetails(sr, bs); p.prepareToPlay(sr, bs);
    }
    /// process `in` (stereo) and return output; sc optional
    std::vector<std::vector<float>> run(const std::vector<std::vector<float>>& in, const std::vector<std::vector<float>>* sc = nullptr, double* secs = nullptr) {
        size_t n = in[0].size(); int nCh = std::max(p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()); int outCh = p.getMainBusNumOutputChannels();
        std::vector<std::vector<float>> out((size_t)outCh, std::vector<float>(n)); juce::AudioBuffer<float> b(nCh, bs); juce::MidiBuffer m; double t = 0;
        for (size_t pos = 0; pos < n; pos += (size_t)bs) {
            int k = (int)std::min<size_t>((size_t)bs, n - pos); b.setSize(nCh, k, false, false, true); b.clear();
            for (int c = 0; c < p.getMainBusNumInputChannels(); ++c) b.copyFrom(c, 0, in[(size_t)std::min<int>(c, (int)in.size() - 1)].data() + pos, k);
            if (sc && p.getBusCount(true) > 1 && p.getBus(true, 1)->isEnabled()) { int off = p.getMainBusNumInputChannels(); for (int c = 0; c < p.getBus(true, 1)->getNumberOfChannels(); ++c) b.copyFrom(off + c, 0, (*sc)[(size_t)c].data() + pos, k); }
            auto t0 = std::chrono::steady_clock::now(); p.processBlock(b, m); t += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            for (int c = 0; c < outCh; ++c) std::memcpy(out[(size_t)c].data() + pos, b.getReadPointer(c), sizeof(float) * (size_t)k);
            head.ppq += k * head.bpm / 60.0 / sr;
        }
        if (secs) *secs = t; return out;
    }
};
static std::vector<std::vector<float>> tone(double sr, double sec, float f = 220, float a = 0.3f) { size_t n = (size_t)(sr * sec); std::vector<std::vector<float>> v(2, std::vector<float>(n)); for (size_t i = 0; i < n; ++i) v[0][i] = v[1][i] = a * (float)std::sin(2 * juce::MathConstants<double>::pi * f * (double)i / sr); return v; }
static std::vector<std::vector<float>> pluck(double sr, double sec) { auto v = tone(sr, sec, 196, 0.25f); for (size_t i = 0; i < v[0].size(); ++i) { float e = std::exp(-(float)(i % (size_t)(sr / 2)) / (float)(sr * 0.12)); v[0][i] *= e; v[1][i] *= e * 0.9f; } return v; }
static double rms(const std::vector<float>& x, size_t a = 0) { double s = 0; for (size_t i = a; i < x.size(); ++i) s += (double)x[i] * x[i]; return std::sqrt(s / (double)std::max<size_t>(1, x.size() - a)); }
static bool finite(const std::vector<std::vector<float>>& o) { for (auto& c : o) for (float v : c) if (!std::isfinite(v)) return false; return true; }
static void savePng(juce::Component& c, const juce::String& name) {
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, 1.0f); auto f = juce::File::getCurrentWorkingDirectory().getChildFile(name); f.deleteFile();
    juce::FileOutputStream s(f); bool ok = s.openedOk() && juce::PNGImageFormat().writeImageToStream(img, s); CHECK(ok); std::printf("  screenshot %s %dx%d\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0); juce::ScopedJuceInitialiser_GUI gui;
    std::puts("MIKE GORDON plugin tests");

    { std::puts("- identity and parameters");
      Rig r; CHECK(r.p.getName() == "MIKE GORDON"); CHECK(!r.p.acceptsMidi() && !r.p.producesMidi() && !r.p.isMidiEffect()); CHECK(r.p.getLatencySamples() == 0); CHECK(r.p.getTailLengthSeconds() > 0);
      CHECK((int)r.p.getParameters().size() == kNumParams); CHECK(r.p.getBypassParameter() == r.p.param(P_BYPASS));
      int okRange = 0, okText = 0, okNorm = 0;
      for (int i = 0; i < kNumParams; ++i) { auto* prm = r.p.param(i); const auto& d = kParams[i]; okRange += prm->paramID == d.id && std::abs(prm->convertFrom0to1(0) - d.min) < 1e-4f && std::abs(prm->convertFrom0to1(1) - d.max) < 1e-4f && std::abs(r.p.value(i) - d.def) < 1e-4f;
        okText += prm->getCurrentValueAsText().isNotEmpty();
        bool n = true; if (d.kind == K_FLOAT) for (float x : {0.1f, 0.3f, 0.5f, 0.77f}) n &= std::abs(prm->convertFrom0to1(x) - fromNorm(i, x)) <= 1e-3f * (d.max - d.min); okNorm += n; }
      CHECK(okRange == kNumParams); CHECK(okText == kNumParams); CHECKM(okNorm == kNumParams, "engine/host normalisation differ for %d params", kNumParams - okNorm);
      juce::Random rnd(3); int rt = 0; for (int i = 0; i < kNumParams; ++i) { auto* prm = r.p.param(i); float n = rnd.nextFloat(); prm->setValueNotifyingHost(n); rt += std::abs(prm->convertTo0to1(r.p.value(i)) - prm->convertTo0to1(prm->convertFrom0to1(n))) < 1e-4f; }
      CHECK(rt == kNumParams); std::printf("  %d parameters\n", kNumParams); }

    { std::puts("- buses: stereo, mono, mono->stereo, sidechain");
      Rig r; CHECK(r.p.getBusCount(true) == 2 && r.p.getBusCount(false) == 1); CHECK(r.p.getBus(true, 1)->getName() == "Sidechain");
      auto l = r.p.getBusesLayout(); CHECK(r.p.checkBusesLayoutSupported(l));
      auto m = l; m.inputBuses.getReference(0) = juce::AudioChannelSet::mono(); m.outputBuses.getReference(0) = juce::AudioChannelSet::mono(); CHECK(r.p.checkBusesLayoutSupported(m));
      auto ms = l; ms.inputBuses.getReference(0) = juce::AudioChannelSet::mono(); CHECK(r.p.checkBusesLayoutSupported(ms));
      auto bad = l; bad.outputBuses.getReference(0) = juce::AudioChannelSet::mono(); CHECK(!r.p.checkBusesLayoutSupported(bad));
      auto q = l; q.outputBuses.getReference(0) = juce::AudioChannelSet::quadraphonic(); CHECK(!r.p.checkBusesLayoutSupported(q));
      auto sc = l; sc.inputBuses.getReference(1) = juce::AudioChannelSet::stereo(); CHECK(r.p.checkBusesLayoutSupported(sc)); sc.inputBuses.getReference(1) = juce::AudioChannelSet::mono(); CHECK(r.p.checkBusesLayoutSupported(sc));
      // mono in/out processes
      MikeGordonProcessor mp; CHECK(mp.setBusesLayout(m)); mp.prepareToPlay(48000, 256); juce::AudioBuffer<float> b(std::max(mp.getTotalNumInputChannels(), mp.getTotalNumOutputChannels()), 256); juce::MidiBuffer mid;
      for (int i = 0; i < 256; ++i) b.setSample(0, i, 0.3f * std::sin(i * 0.05f)); mp.processBlock(b, mid); bool fin = true; for (int i = 0; i < 256; ++i) fin &= std::isfinite(b.getSample(0, i)); CHECK(fin); CHECK(b.getMagnitude(0, 0, 256) > 0.05f); }

    { std::puts("- silence in = silence out with defaults; bypass and Amount 0 pass audio unchanged");
      Rig r; auto s = r.run({std::vector<float>(48000, 0.f), std::vector<float>(48000, 0.f)}); CHECK(rms(s[0]) == 0 && rms(s[1]) == 0);
      auto in = pluck(48000, 1.0);
      { Rig a; a.p.setValue(P_SPC_ON, 1); a.p.setValue(P_BYPASS, 1); auto o = a.run(in); bool same = true; for (size_t i = 0; i < in[0].size(); ++i) same &= o[0][i] == in[0][i] && o[1][i] == in[1][i]; CHECK(same); }
      { Rig a; a.p.setValue(P_SPC_ON, 1); a.p.setValue(P_AMOUNT, 0); auto o = a.run(in); bool same = true; for (size_t i = 0; i < in[0].size(); ++i) same &= o[0][i] == in[0][i]; CHECK(same); }
      { Rig a; juce::AudioBuffer<float> b(4, 256); for (int i = 0; i < 256; ++i) { b.setSample(0, i, 0.2f); b.setSample(1, i, -0.2f); } juce::MidiBuffer m; a.p.processBlockBypassed(b, m); CHECK(b.getSample(0, 100) == 0.2f && b.getSample(1, 100) == -0.2f); }
      { Rig a; a.p.loadFactory(10); auto o = a.run(in); CHECK(finite(o)); CHECK(rms(o[0]) > 0.01); double d = 0; for (size_t i = 0; i < in[0].size(); ++i) d += std::abs(o[0][i] - in[0][i]); CHECK(d / (double)in[0].size() > 1e-3); } }

    { std::puts("- sidechain ducking through the host bus");
      Rig r(48000, 256, true); CHECK(r.p.getTotalNumInputChannels() == 4); r.p.setValue(P_AMP_ON, 0); r.p.setValue(P_WID_ON, 0); r.p.setValue(P_MATCH, 0); r.p.setValue(P_DUK_ON, 1); r.p.setValue(P_DUK_SRC, 1); r.p.setValue(P_DUK_DEPTH, 80);
      std::vector<std::vector<float>> dc(2, std::vector<float>(48000, 0.5f)), sc(2, std::vector<float>(48000, 0.f)); for (size_t i = 24000; i < 26400; ++i) sc[0][i] = sc[1][i] = 0.5f * (float)std::sin(i * 0.01);
      auto o = r.run(dc, &sc); CHECKM(o[0][25000] < 0.2f, "ducked %f", o[0][25000]); CHECK(std::abs(o[0][12000] - 0.5f) < 1e-3f); }

    { std::puts("- state: params, order, macros, locks survive save/load");
      Rig a; a.p.setValue(P_AMP_VOICE, 3); a.p.setValue(P_SPC_MIX, 61); a.p.setValue(P_FLT_LP, 1234); a.p.setOrder({5, 0, 1, 2, 3, 4}); auto mm = a.p.macros(); mm.s[2][3] = {P_MOT_RATE, -0.5f}; a.p.setMacros(mm); a.p.setLocked(S_SPACE, true);
      juce::MemoryBlock mb; a.p.getStateInformation(mb); Rig b; b.p.setStateInformation(mb.getData(), (int)mb.getSize());
      CHECK((int)b.p.value(P_AMP_VOICE) == 3); CHECK(std::abs(b.p.value(P_SPC_MIX) - 61) < 0.01f); CHECK(std::abs(b.p.value(P_FLT_LP) - 1234) < 0.5f);
      auto o = b.p.getOrder(); CHECK(o[0] == 5 && o[1] == 0 && o[5] == 4); CHECK(b.p.macros().s[2][3].param == P_MOT_RATE && std::abs(b.p.macros().s[2][3].depth + 0.5f) < 1e-4f); CHECK(b.p.locked(S_SPACE) && !b.p.locked(S_TONE));
      b.p.setStateInformation("junk", 4); CHECK((int)b.p.value(P_AMP_VOICE) == 3); }

    { std::puts("- factory presets as host programs");
      Rig r; CHECK(r.p.getNumPrograms() == (int)factoryPresets().size()); CHECK(r.p.getNumPrograms() >= 40);
      for (int i = 0; i < r.p.getNumPrograms(); ++i) { r.p.setCurrentProgram(i); CHECK(r.p.getCurrentProgram() == i); CHECK(r.p.getProgramName(i) == factoryPresets()[(size_t)i].name);
        float v[kNumParams]; applyPreset(i, v); bool ok = true; for (int k = 0; k < kNumParams; ++k) if (k != P_BYPASS && k != P_COMPARE) ok &= std::abs(r.p.value(k) - v[k]) < 1e-3f * std::max(1.f, kParams[k].max - kParams[k].min); CHECKM(ok, "preset %d", i); } }

    { std::puts("- macros, order, randomize locks");
      Rig r; r.p.setValue(P_SPC_ON, 1); auto in = pluck(48000, 1.0); auto a = r.run(in); r.p.setValue(P_MACRO1, 100); Rig r2; r2.p.setValue(P_SPC_ON, 1); r2.p.setValue(P_MACRO1, 100); auto b = r2.run(in);
      double d = 0; for (size_t i = 0; i < in[0].size(); ++i) d += std::abs(a[0][i] - b[0][i]); CHECK(d / (double)in[0].size() > 1e-3); CHECK(r.p.macroName(0) == "Dream");
      auto mm = r.p.macros(); mm.s[0][0] = {P_TEX_HISS, 1}; r.p.setMacros(mm); CHECK(r.p.macroName(0) != "Dream");
      r.p.setOrder({0, 0, 1, 2, 3, 4}); auto o = r.p.getOrder(); CHECK(o[0] == 0 && o[1] == 1); // invalid order rejected
      Rig q; for (int s = S_TONE; s <= S_WIDTH; ++s) q.p.setLocked(s, s == S_TONE || s == S_SPACE); float before[kNumParams]; q.p.readValues(before);
      int changed = 0; bool lockedSame = true; for (int k = 0; k < 20; ++k) { q.p.randomize(); float now[kNumParams]; q.p.readValues(now); for (int i = 0; i < kNumParams; ++i) { int s = kParams[i].section; if (s == S_TONE || s == S_SPACE || s == S_GLOBAL) lockedSame &= now[i] == before[i]; else changed += now[i] != before[i]; } }
      CHECK(lockedSame); CHECK(changed > 50); }

    { std::puts("- sample rates and block sizes");
      for (double sr : {44100.0, 48000.0, 96000.0, 192000.0}) for (int bs : {32, 128, 512, 2048}) { Rig r(sr, bs); r.p.loadFactory(10); auto o = r.run(pluck(sr, 0.5)); CHECK(finite(o)); }
      // the host may send a block shorter than announced
      Rig r(48000, 512); juce::AudioBuffer<float> b(4, 17); b.clear(); juce::MidiBuffer m; r.p.processBlock(b, m); CHECK(true); }

    { std::puts("- editor: builds, resizes, screenshots");
      Rig r; r.p.loadFactory(0);
      std::unique_ptr<juce::AudioProcessorEditor> ed(r.p.createEditor()); auto* e = dynamic_cast<MikeGordonEditor*>(ed.get()); CHECK(e != nullptr);
      CHECK(ed->getWidth() == MikeGordonEditor::kW && ed->getHeight() == MikeGordonEditor::kH); CHECK(ed->isResizable());
      auto* c = ed->getConstrainer(); CHECK(c->getMinimumWidth() < MikeGordonEditor::kW && c->getMaximumWidth() >= 2 * MikeGordonEditor::kW - 1);
      e->refreshNow(); savePng(*ed, "MIKE-GORDON-ui-1-default.png");
      r.p.loadFactory(10); r.p.setValue(P_SPC_TYPE, 1); e->refreshNow(); savePng(*ed, "MIKE-GORDON-ui-2-dreamy-pad.png");
      r.p.loadFactory(15); r.p.setValue(P_SPC_ON, 1); r.p.setValue(P_SPC_TYPE, 2); for (int k = 0; k < e->numCards(); ++k) e->setMoreOpen(k, k % 2 == 1); e->refreshNow(); savePng(*ed, "MIKE-GORDON-ui-3-more-panels.png");
      for (int k = 0; k < e->numCards(); ++k) e->setMoreOpen(k, false);
      r.p.loadFactory(19); ed->setSize(MikeGordonEditor::kW * 6 / 10, MikeGordonEditor::kH * 6 / 10); e->refreshNow(); savePng(*ed, "MIKE-GORDON-ui-4-small-window.png");
      ed->setSize(MikeGordonEditor::kW * 3 / 2, MikeGordonEditor::kH * 3 / 2); e->refreshNow(); CHECK(ed->getWidth() == MikeGordonEditor::kW * 3 / 2);
      // every child component has a size and stays inside the window at the base size
      ed->setSize(MikeGordonEditor::kW, MikeGordonEditor::kH); e->refreshNow(); int outside = 0;
      std::function<void(juce::Component&, juce::Rectangle<int>)> walk = [&](juce::Component& comp, juce::Rectangle<int> area) { for (auto* ch : comp.getChildren()) { if (!ch->isVisible()) continue; if (!area.contains(ch->getBounds())) { ++outside; std::printf("  outside: %s %s in %s\n", ch->getName().toRawUTF8(), ch->getBounds().toString().toRawUTF8(), area.toString().toRawUTF8()); } walk(*ch, ch->getLocalBounds()); } };
      walk(*ed, ed->getLocalBounds().withSize(MikeGordonEditor::kW, MikeGordonEditor::kH)); CHECKM(outside == 0, "%d components outside their parent", outside);
      ed.reset(); CHECK(true); }

    { std::puts("- CPU (48 kHz, 256-sample blocks, this CI machine)");
      auto in = pluck(48000, 10.0);
      auto cpu = [&](const char* name, std::function<void(MikeGordonProcessor&)> set) { Rig r; set(r.p); double t = 0; r.run(in, nullptr, &t); double pct = 100.0 * t / 10.0; std::printf("  CPU %-32s %6.2f %%\n", name, pct); return pct; };
      double a = cpu("defaults", [](MikeGordonProcessor&) {});
      double b = cpu("Dreamy Pad", [](MikeGordonProcessor& p) { p.loadFactory(10); });
      double c = cpu("everything on", [](MikeGordonProcessor& p) { for (int m = 0; m < kMovable; ++m) p.setValue(sectionSwitch(m), 1); p.setValue(P_DUK_ON, 1); p.setValue(P_SPC_SHIMMER, 50); p.setValue(P_PIT_DOWN, 50); p.setValue(P_PIT_UP, 50); p.setValue(P_TEX_CRACKLE, 50); p.setValue(P_TEX_HISS, 30); });
      CHECK(a < 10 && b < 20 && c < 30); }

    std::printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
