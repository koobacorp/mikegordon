#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace mg;

juce::AudioProcessorValueTreeState::ParameterLayout MikeGordonProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < kNumParams; ++i) {
        const auto& d = kParams[i]; juce::ParameterID id { d.id, 1 };
        juce::String label = d.unit;
        if (d.kind == K_BOOL) l.add(std::make_unique<juce::AudioParameterBool>(id, d.name, d.def >= 0.5f));
        else if (d.kind == K_CHOICE) { juce::StringArray c; for (int k = 0; k < d.nChoices; ++k) c.add(d.choices[k]); l.add(std::make_unique<juce::AudioParameterChoice>(id, d.name, c, (int)d.def)); }
        else {
            juce::NormalisableRange<float> r(d.min, d.max); if (d.centre > 0) r.setSkewForCentre(d.centre);
            auto unit = juce::String(d.unit); int dec = (d.max - d.min) <= 12 ? 2 : ((d.max - d.min) <= 200 ? 1 : 0);
            auto toText = [unit, dec](float v, int) { if (unit == "Hz" && v >= 1000) return juce::String(v / 1000.f, 2) + " kHz"; return juce::String(v, dec) + (unit.isEmpty() ? "" : " " + unit); };
            l.add(std::make_unique<juce::AudioParameterFloat>(id, d.name, r, d.def, juce::AudioParameterFloatAttributes().withLabel(label).withStringFromValueFunction(toText)));
        }
    }
    return l;
}

MikeGordonProcessor::MikeGordonProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "MIKEGORDON", layout()) {
    for (int i = 0; i < kNumParams; ++i) { raw.push_back(apvts.getRawParameterValue(kParams[i].id)); params.push_back(apvts.getParameter(kParams[i].id)); }
    for (int i = 0; i < kMovable; ++i) order[(size_t)i].store(i);
    setMacros(defaultMacros());
    for (auto& l : locks) l.store(false);
    eng.prepare(48000, 512);
    setLatencySamples(0);
}

bool MikeGordonProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    if (out == juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::mono()) return false;
    if (l.inputBuses.size() > 1) { auto sc = l.getChannelSet(true, 1); if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false; }
    return true;
}

void MikeGordonProcessor::prepareToPlay(double sr, int bs) {
    eng.prepare(sr, bs); scratch.setSize(2, std::max(bs, 32), false, false, true); scBuf.setSize(2, std::max(bs, 32), false, false, true); setLatencySamples(0);
}

void MikeGordonProcessor::run(juce::AudioBuffer<float>& buffer, bool bypassed) {
    juce::ScopedNoDenormals nd;
    int n = buffer.getNumSamples(); if (n <= 0) return;
    auto mainIn = getBusBuffer(buffer, true, 0); auto mainOut = getBusBuffer(buffer, false, 0);
    int inCh = mainIn.getNumChannels(), outCh = mainOut.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear(c, 0, n);
    if (bypassed || outCh == 0) return;
    if (scratch.getNumSamples() < n) { scratch.setSize(2, n, false, false, true); scBuf.setSize(2, n, false, false, true); } // only if the host breaks its promise
    // main signal -> stereo scratch
    for (int c = 0; c < 2; ++c) scratch.copyFrom(c, 0, mainIn, std::min(c, std::max(0, inCh - 1)), 0, n);
    if (inCh == 0) scratch.clear();
    // sidechain
    const float* scL = nullptr; const float* scR = nullptr;
    if (getBusCount(true) > 1 && getBus(true, 1)->isEnabled()) { auto sc = getBusBuffer(buffer, true, 1); if (sc.getNumChannels() > 0) { scL = sc.getReadPointer(0); scR = sc.getReadPointer(std::min(1, sc.getNumChannels() - 1)); } }
    // parameters + macros
    for (int i = 0; i < kNumParams; ++i) base[i] = raw[(size_t)i]->load();
    applyMacros(base, eff, macros());
    int o[kMovable]; for (int i = 0; i < kMovable; ++i) o[i] = order[(size_t)i].load(); eng.setOrder(o);
    HostInfo hi;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) { if (auto b = pos->getBpm()) hi.bpm = *b; if (auto q = pos->getPpqPosition()) { hi.ppq = *q; hi.hasPpq = true; } hi.playing = pos->getIsPlaying(); }
    lastBpm.store(hi.bpm);
    eng.process(scratch.getWritePointer(0), scratch.getWritePointer(1), n, scL, scR, hi, eff);
    if (outCh == 1) { auto* d = mainOut.getWritePointer(0); auto* a = scratch.getReadPointer(0); auto* b = scratch.getReadPointer(1); for (int i = 0; i < n; ++i) d[i] = 0.5f * (a[i] + b[i]); }
    else { mainOut.copyFrom(0, 0, scratch, 0, 0, n); mainOut.copyFrom(1, 0, scratch, 1, 0, n); for (int c = 2; c < outCh; ++c) mainOut.clear(c, 0, n); }
}
void MikeGordonProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { m.clear(); run(b, false); }
void MikeGordonProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { m.clear(); run(b, true); }

// ---------------------------------------------------------------------------------------------------------------------
void MikeGordonProcessor::applyValues(const float* v, bool includeGlobal) {
    for (int i = 0; i < kNumParams; ++i) {
        if (!includeGlobal && kParams[i].section == S_GLOBAL) continue;
        if (i == P_BYPASS || i == P_COMPARE) continue;
        auto* p = params[(size_t)i]; float n = p->convertTo0to1(v[i]); if (std::abs(p->getValue() - n) > 1e-7f) { p->beginChangeGesture(); p->setValueNotifyingHost(n); p->endChangeGesture(); }
    }
}
void MikeGordonProcessor::loadFactory(int i) {
    const auto& ps = factoryPresets(); if (i < 0 || i >= (int)ps.size()) return;
    float v[kNumParams]; applyPreset(i, v);
    applyValues(v, true); setMacros(defaultMacros()); setOrder({0, 1, 2, 3, 4, 5});
    currentProgram = i; currentName = ps[(size_t)i].name; updateHostDisplay(ChangeDetails().withProgramChanged(true));
}
void MikeGordonProcessor::setCurrentProgram(int i) { loadFactory(i); }
const juce::String MikeGordonProcessor::getProgramName(int i) { const auto& ps = factoryPresets(); return i >= 0 && i < (int)ps.size() ? juce::String(ps[(size_t)i].name) : juce::String(); }

void MikeGordonProcessor::randomize() {
    float v[kNumParams]; readValues(v); bool lk[kNumSections]; for (int s = 0; s < kNumSections; ++s) lk[s] = locked(s);
    rr.s = (uint32_t)rng.nextInt() | 1u; mg::randomize(v, lk, rr); applyValues(v, false); currentName = "Random roll";
}

void MikeGordonProcessor::setOrder(const std::array<int, kMovable>& o) { int t[kMovable]; for (int i = 0; i < kMovable; ++i) t[i] = o[(size_t)i]; if (!Engine::validOrder(t)) return; for (int i = 0; i < kMovable; ++i) order[(size_t)i].store(t[i]); }

mg::MacroMap MikeGordonProcessor::macros() const { MacroMap m; for (int k = 0; k < kMacros; ++k) for (int s = 0; s < kMacroSlots; ++s) { size_t j = (size_t)(k * kMacroSlots + s); m.s[k][s].param = macroParam[j].load(); m.s[k][s].depth = macroDepth[j].load(); } return m; }
void MikeGordonProcessor::setMacros(const mg::MacroMap& m) { for (int k = 0; k < kMacros; ++k) for (int s = 0; s < kMacroSlots; ++s) { size_t j = (size_t)(k * kMacroSlots + s); macroParam[j].store(m.s[k][s].param); macroDepth[j].store(m.s[k][s].depth); } }
juce::String MikeGordonProcessor::macroName(int k) const {
    auto m = macros(), d = defaultMacros(); bool same = true; for (int s = 0; s < kMacroSlots; ++s) same &= m.s[k][s].param == d.s[k][s].param && std::abs(m.s[k][s].depth - d.s[k][s].depth) < 1e-6f;
    if (same) return defaultMacroName(k);
    int first = -1; for (auto& s : m.s[k]) if (s.param >= 0) { first = s.param; break; }
    return first >= 0 ? juce::String(kParams[first].name) : "Macro " + juce::String(k + 1);
}

// ---------------------------------------------------------------------------------------------------------------------
juce::ValueTree MikeGordonProcessor::extraState() const {
    juce::ValueTree x("EXTRA");
    juce::StringArray o; for (auto& v : order) o.add(juce::String(v.load())); x.setProperty("order", o.joinIntoString(","), nullptr);
    juce::StringArray mm; auto m = macros(); for (auto& k : m.s) for (auto& s : k) mm.add(juce::String(s.param >= 0 ? kParams[s.param].id : "-") + ":" + juce::String(s.depth, 4)); x.setProperty("macros", mm.joinIntoString(","), nullptr);
    juce::StringArray lk; for (int s = 0; s < kNumSections; ++s) lk.add(locked(s) ? "1" : "0"); x.setProperty("locks", lk.joinIntoString(","), nullptr);
    x.setProperty("preset", currentName, nullptr); x.setProperty("program", currentProgram, nullptr);
    return x;
}
void MikeGordonProcessor::restoreExtra(const juce::ValueTree& x) {
    if (!x.isValid()) return;
    auto o = juce::StringArray::fromTokens(x.getProperty("order").toString(), ",", ""); if (o.size() == kMovable) { std::array<int, kMovable> a; for (int i = 0; i < kMovable; ++i) a[(size_t)i] = o[i].getIntValue(); setOrder(a); }
    auto mm = juce::StringArray::fromTokens(x.getProperty("macros").toString(), ",", "");
    if (mm.size() == kMacros * kMacroSlots) { MacroMap m; for (int j = 0; j < mm.size(); ++j) { auto id = mm[j].upToFirstOccurrenceOf(":", false, false); float dpt = mm[j].fromFirstOccurrenceOf(":", false, false).getFloatValue();
        int pi = id == "-" ? -1 : paramIndex(id.toRawUTF8()); m.s[j / kMacroSlots][j % kMacroSlots] = {pi, juce::jlimit(-1.f, 1.f, dpt)}; } setMacros(m); }
    auto lk = juce::StringArray::fromTokens(x.getProperty("locks").toString(), ",", ""); if (lk.size() == kNumSections) for (int s = 0; s < kNumSections; ++s) setLocked(s, lk[s] == "1");
    if (x.hasProperty("preset")) currentName = x.getProperty("preset").toString(); currentProgram = (int)x.getProperty("program", 0);
}
void MikeGordonProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState(); auto old = st.getChildWithName("EXTRA"); if (old.isValid()) st.removeChild(old, nullptr); st.appendChild(extraState(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
void MikeGordonProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size); if (!xml) return; auto st = juce::ValueTree::fromXml(*xml); if (!st.hasType(apvts.state.getType())) return;
    auto extra = st.getChildWithName("EXTRA").createCopy(); if (extra.isValid()) st.removeChild(st.getChildWithName("EXTRA"), nullptr);
    apvts.replaceState(st); restoreExtra(extra);
}

// ---------------------------------------------------------------------------------------------------------------------
juce::File MikeGordonProcessor::userPresetDir() { return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("MIKE GORDON Presets"); }
bool MikeGordonProcessor::saveUserPreset(const juce::String& name) {
    auto clean = juce::File::createLegalFileName(name.trim()); if (clean.isEmpty()) return false;
    auto dir = userPresetDir(); if (!dir.createDirectory()) return false;
    currentName = clean; juce::MemoryBlock mb; getStateInformation(mb); auto xml = getXmlFromBinary(mb.getData(), (int)mb.getSize()); if (!xml) return false;
    return xml->writeTo(dir.getChildFile(clean + ".mgpreset"));
}
bool MikeGordonProcessor::loadUserPreset(const juce::File& f) {
    auto xml = juce::XmlDocument::parse(f); if (!xml) return false; juce::MemoryBlock mb; copyXmlToBinary(*xml, mb); setStateInformation(mb.getData(), (int)mb.getSize());
    currentName = f.getFileNameWithoutExtension(); return true;
}
juce::Array<juce::File> MikeGordonProcessor::userPresets() const { auto a = userPresetDir().findChildFiles(juce::File::findFiles, false, "*.mgpreset"); a.sort(); return a; }

juce::AudioProcessorEditor* MikeGordonProcessor::createEditor() { return new MikeGordonEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MikeGordonProcessor(); }
