// MIKE GORDON — editor. Sized by use: amp picker, Amount, presets and each section's main knob are big;
// order, macro assignment, ducking source and randomize locks are small or tucked behind "more".
#pragma once
#include "Widgets.h"

class MikeGordonEditor;

/// A section card: header (on/off, name, lock, more), main view (type row, big knob, medium row, toggles) and a tucked "more" view.
class Card : public juce::Component {
public:
    /// one medium position; when it has several options, pick() chooses which one shows
    struct Slot { std::vector<juce::Component*> opts; std::function<int()> pick; };
    Card(MikeGordonProcessor& p, const juce::String& title, int sectionSwitchParam, int lockSection);
    juce::String title; MikeGordonProcessor& proc; int lockSec;
    std::unique_ptr<ui::Toggle> onBtn; juce::TextButton lockBtn { "lock" }, moreBtn { "more" };
    juce::OwnedArray<juce::Component> owned;
    juce::Component* typeRow = nullptr; juce::Component* big = nullptr; std::vector<Slot> mediums; std::vector<juce::Component*> toggles, more;
    bool moreOpen = false;
    template <class T> T* own(T* c) { owned.add(c); addAndMakeVisible(c); return c; }
    void setMore(bool b);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
};

class AmpTile : public juce::Button {
public:
    AmpTile(const juce::String& n, const juce::String& d) : juce::Button(n), desc(d) { setClickingTogglesState(false); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
    juce::String desc;
};

class ToneCard : public Card {
public:
    explicit ToneCard(MikeGordonProcessor& p);
    juce::OwnedArray<AmpTile> tiles; std::unique_ptr<juce::ParameterAttachment> voiceAtt; ui::Knob* vfx = nullptr; ui::Knob* speed = nullptr; std::vector<juce::Component*> smalls;
    void refreshTone();
    void resized() override;
};

class OrderStrip : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit OrderStrip(MikeGordonProcessor& p) : proc(p) { setTooltip("Drag a section left or right to change the order the sound goes through"); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override; void mouseDrag(const juce::MouseEvent&) override; void mouseUp(const juce::MouseEvent&) override;
    juce::Rectangle<int> chip(int slot) const;
    MikeGordonProcessor& proc; int dragging = -1; float dragX = 0;
};

class MacroBox : public juce::Component {
public:
    MacroBox(MikeGordonProcessor& p, int k);
    void resized() override; void paint(juce::Graphics&) override; void showAssign();
    MikeGordonProcessor& proc; int k; std::unique_ptr<ui::Knob> knob; juce::TextButton assign { "assign" };
};

class MikeGordonEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    static constexpr int kW = 1240, kH = 720;
    explicit MikeGordonEditor(MikeGordonProcessor&);
    ~MikeGordonEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void setMoreOpen(int card, bool open) { if (card >= 0 && card < (int)cards.size()) cards[(size_t)card]->setMore(open); }
    void refreshNow() { timerCallback(); }
    int numCards() const { return (int)cards.size(); }
private:
    void timerCallback() override;
    void showPresetMenu(); void stepPreset(int d); void savePreset();
    MikeGordonProcessor& proc; ui::Look look; juce::Component content; juce::TooltipWindow tips { this, 600 };
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Init" }, saveBtn { "Save" }, randomBtn { "Randomize" };
    std::unique_ptr<ToneCard> tone; std::vector<std::unique_ptr<Card>> cards;
    std::unique_ptr<ui::Knob> amount, outGain; std::unique_ptr<ui::Toggle> before, match, limit;
    std::unique_ptr<OrderStrip> orderStrip; std::vector<std::unique_ptr<MacroBox>> macroBoxes;
    juce::String readout;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MikeGordonEditor)
};
