// MIKE GORDON — JUCE processor: host parameters from the engine's table, sidechain bus, presets, macros, order, locks, state.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Presets.h"

class MikeGordonProcessor : public juce::AudioProcessor {
public:
    MikeGordonProcessor();
    ~MikeGordonProcessor() override = default;

    void prepareToPlay(double sr, int bs) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter("bypass"); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MIKE GORDON"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return (int)mg::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int i) override;
    const juce::String getProgramName(int i) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    // ---- helpers used by the editor and the tests
    juce::RangedAudioParameter* param(int i) const { return params[(size_t)i]; }
    float value(int i) const { return raw[(size_t)i]->load(); }
    void setValue(int i, float plain) { auto* p = params[(size_t)i]; p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(plain)); p->endChangeGesture(); }
    void readValues(float* out) const { for (int i = 0; i < mg::kNumParams; ++i) out[i] = raw[(size_t)i]->load(); }
    void applyValues(const float* v, bool includeGlobal);

    // presets
    juce::String presetName() const { return currentName; }
    void loadFactory(int i);
    static juce::File userPresetDir();
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::File& f);
    juce::Array<juce::File> userPresets() const;
    void randomize();

    // section order (draggable)
    void setOrder(const std::array<int, mg::kMovable>& o);
    std::array<int, mg::kMovable> getOrder() const { std::array<int, mg::kMovable> o; for (int i = 0; i < mg::kMovable; ++i) o[(size_t)i] = order[(size_t)i].load(); return o; }

    // macros
    mg::MacroMap macros() const;
    void setMacros(const mg::MacroMap& m);
    juce::String macroName(int k) const;

    // randomize locks
    bool locked(int section) const { return locks[(size_t)section].load(); }
    void setLocked(int section, bool b) { locks[(size_t)section].store(b); }

    mg::Engine& engine() { return eng; }
    std::atomic<double> lastBpm {120};
    juce::Random rng;

private:
    mg::Engine eng; int currentProgram = 0; juce::String currentName = "Init";
    std::vector<std::atomic<float>*> raw; std::vector<juce::RangedAudioParameter*> params;
    std::array<std::atomic<int>, mg::kMovable> order;
    std::array<std::atomic<int>, mg::kMacros * mg::kMacroSlots> macroParam; std::array<std::atomic<float>, mg::kMacros * mg::kMacroSlots> macroDepth;
    std::array<std::atomic<bool>, mg::kNumSections> locks;
    juce::AudioBuffer<float> scratch, scBuf;
    float base[mg::kNumParams] = {}, eff[mg::kNumParams] = {};
    mg::Rng rr;
    juce::ValueTree extraState() const; void restoreExtra(const juce::ValueTree&);
    void run(juce::AudioBuffer<float>& buf, bool bypassed);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MikeGordonProcessor)
};
