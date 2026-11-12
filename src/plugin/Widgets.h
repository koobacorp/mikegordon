// MIKE GORDON — look and feel and parameter-bound widgets. Emerald skin, honey/blonde accent, cream labels.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace ui {
namespace col {
const juce::Colour bg{0xff0a3a2b}, bgDeep{0xff072a1f}, card{0xff0f4d3a}, cardHi{0xff146046}, edge{0xff22785b}, cream{0xfff6efdd}, creamDim{0xffb9d0c2},
    honey{0xffdcb57e}, honeyHi{0xfff0d3a2}, honeyDark{0xffa97d4f}, ink{0xff1b140b}, knob{0xff0b2e23}, off{0xff3e6a5a}, track{0xff062219};
}
inline juce::Font font(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }

class Look : public juce::LookAndFeel_V4 {
public:
    Look() {
        setColour(juce::ResizableWindow::backgroundColourId, col::bg);
        setColour(juce::TextButton::buttonColourId, col::cardHi); setColour(juce::TextButton::buttonOnColourId, col::honey);
        setColour(juce::TextButton::textColourOffId, col::cream); setColour(juce::TextButton::textColourOnId, col::ink);
        setColour(juce::ComboBox::backgroundColourId, col::bgDeep); setColour(juce::ComboBox::textColourId, col::cream); setColour(juce::ComboBox::outlineColourId, col::edge); setColour(juce::ComboBox::arrowColourId, col::honey);
        setColour(juce::PopupMenu::backgroundColourId, col::card); setColour(juce::PopupMenu::textColourId, col::cream);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, col::honey); setColour(juce::PopupMenu::highlightedTextColourId, col::ink); setColour(juce::PopupMenu::headerTextColourId, col::honey);
        setColour(juce::Label::textColourId, col::cream);
        setColour(juce::TooltipWindow::backgroundColourId, col::cream); setColour(juce::TooltipWindow::textColourId, col::ink); setColour(juce::TooltipWindow::outlineColourId, col::honey);
        setColour(juce::TextEditor::backgroundColourId, col::bgDeep); setColour(juce::TextEditor::textColourId, col::cream); setColour(juce::TextEditor::outlineColourId, col::edge); setColour(juce::TextEditor::focusedOutlineColourId, col::honey);
        setColour(juce::AlertWindow::backgroundColourId, col::card); setColour(juce::AlertWindow::textColourId, col::cream); setColour(juce::AlertWindow::outlineColourId, col::honey);
        setColour(juce::CaretComponent::caretColourId, col::honey);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override {
        auto r = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(2); float d = std::min(r.getWidth(), r.getHeight()); auto c = r.withSizeKeepingCentre(d, d);
        float thick = juce::jlimit(3.f, 7.f, d * 0.075f); float rad = d * 0.5f - thick * 0.5f; float ang = a0 + pos * (a1 - a0); bool bip = s.getProperties()["bipolar"];
        juce::Path track; track.addCentredArc(c.getCentreX(), c.getCentreY(), rad, rad, 0, a0, a1, true);
        g.setColour(col::track); g.strokePath(track, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        float from = bip ? (a0 + a1) * 0.5f : a0; juce::Path arc; arc.addCentredArc(c.getCentreX(), c.getCentreY(), rad, rad, 0, std::min(from, ang), std::max(from, ang), true);
        g.setColour(s.isEnabled() ? col::honey : col::off); g.strokePath(arc, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        auto cap = c.reduced(thick * 1.9f);
        g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillEllipse(cap.translated(0, 2));
        juce::ColourGradient grad(juce::Colour(0xff1a5a45), cap.getCentreX(), cap.getY(), col::knob, cap.getCentreX(), cap.getBottom(), false); g.setGradientFill(grad); g.fillEllipse(cap);
        g.setColour(col::edge); g.drawEllipse(cap, 1.f);
        float pr = cap.getWidth() * 0.5f; juce::Path ptr; ptr.addRoundedRectangle(-1.8f, -pr + 3, 3.6f, pr * 0.55f, 1.8f);
        g.setColour(col::cream); g.fillPath(ptr, juce::AffineTransform::rotation(ang).translated(cap.getCentreX(), cap.getCentreY()));
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override {
        auto r = b.getLocalBounds().toFloat().reduced(1); bool on = b.getToggleState(); float rad = std::min(8.f, r.getHeight() * 0.5f);
        auto fill = on ? col::honey : col::cardHi; if (over) fill = fill.brighter(0.08f); if (down) fill = fill.darker(0.1f);
        g.setColour(fill); g.fillRoundedRectangle(r, rad); g.setColour(on ? col::honeyHi : col::edge); g.drawRoundedRectangle(r, rad, 1.f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override {
        g.setFont(font(juce::jlimit(10.f, 16.f, b.getHeight() * 0.5f), true)); g.setColour(b.getToggleState() ? col::ink : col::cream);
        g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(4, 1), juce::Justification::centred, 1);
    }
    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override {
        auto r = juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f); g.setColour(col::bgDeep); g.fillRoundedRectangle(r, 6); g.setColour(col::edge); g.drawRoundedRectangle(r, 6, 1);
        juce::Path p; float cx = (float)w - 11, cy = (float)h * 0.5f; p.addTriangle(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3); g.setColour(col::honey); g.fillPath(p);
    }
    juce::Font getComboBoxFont(juce::ComboBox& c) override { return font(juce::jlimit(10.f, 15.f, c.getHeight() * 0.55f)); }
    void positionComboBoxText(juce::ComboBox& c, juce::Label& l) override { l.setBounds(4, 0, c.getWidth() - 20, c.getHeight()); l.setFont(getComboBoxFont(c)); }
    juce::Font getPopupMenuFont() override { return font(15); }
};

/// Knob with its name above the value, both always visible. Size classes: big (main), medium, small.
class Knob : public juce::Component {
public:
    Knob(MikeGordonProcessor& p, int index, const juce::String& label = {}) : idx(index), name(label.isEmpty() ? juce::String(mg::kParams[index].name) : label) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setPopupDisplayEnabled(true, true, nullptr); s.setDoubleClickReturnValue(true, mg::kParams[index].def);
        const auto& d = mg::kParams[index]; if (d.min < 0 && d.max > 0) s.getProperties().set("bipolar", true);
        addAndMakeVisible(s); att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, d.id, s);
        s.onValueChange = [this] { repaint(); };
        s.setTooltip(juce::String(d.name) + " - drag up/down, double-click to reset");
    }
    void setLabel(const juce::String& l) { if (l != name) { name = l; repaint(); } }
    void resized() override { auto r = getLocalBounds(); int lab = labelH(); r.removeFromTop(lab); r.removeFromBottom(valH()); s.setBounds(r); }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds(); g.setColour(isEnabled() ? col::cream : col::creamDim); g.setFont(font((float)labelH() * 0.82f, true));
        g.drawFittedText(name, r.removeFromTop(labelH()), juce::Justification::centred, 1);
        g.setColour(col::honey); g.setFont(font((float)valH() * 0.82f)); g.drawFittedText(s.getTextFromValue(s.getValue()), r.removeFromBottom(valH()), juce::Justification::centred, 1);
    }
    juce::Slider s; int idx;
private:
    int labelH() const { return juce::jlimit(12, 20, getHeight() / 7); }
    int valH() const { return juce::jlimit(11, 17, getHeight() / 8); }
    juce::String name; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

/// On/off pill bound to a bool parameter.
class Toggle : public juce::TextButton {
public:
    Toggle(MikeGordonProcessor& p, int index, const juce::String& label = {}) : juce::TextButton(label.isEmpty() ? juce::String(mg::kParams[index].name) : label) {
        setClickingTogglesState(true); att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, mg::kParams[index].id, *this); setTooltip(mg::kParams[index].name);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

/// Row of buttons for a choice parameter (amp types, motion/space types, filter movement).
class Segmented : public juce::Component {
public:
    Segmented(MikeGordonProcessor& p, int index) : param(p.param(index)), n(mg::kParams[index].nChoices) {
        for (int k = 0; k < n; ++k) { auto* b = btn.add(new juce::TextButton(mg::kParams[index].choices[k])); b->setRadioGroupId(1000 + index); b->setClickingTogglesState(false); addAndMakeVisible(b);
            b->onClick = [this, k] { if (pa) pa->setValueAsCompleteGesture((float)k); }; }
        pa = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) { int c = (int)std::lround(v); for (int k = 0; k < btn.size(); ++k) btn[k]->setToggleState(k == c, juce::dontSendNotification); });
        pa->sendInitialUpdate();
    }
    void resized() override { auto r = getLocalBounds(); int w = r.getWidth() / std::max(1, n); for (int k = 0; k < n; ++k) btn[k]->setBounds(k == n - 1 ? r : r.removeFromLeft(w).withTrimmedRight(3)); }
    juce::OwnedArray<juce::TextButton> btn;
private:
    juce::RangedAudioParameter* param; int n; std::unique_ptr<juce::ParameterAttachment> pa;
};

/// Small combo for a choice parameter (sync divisions, duck source...).
class Choice : public juce::Component {
public:
    Choice(MikeGordonProcessor& p, int index, const juce::String& label = {}) : name(label.isEmpty() ? juce::String(mg::kParams[index].name) : label) {
        const auto& d = mg::kParams[index]; for (int k = 0; k < d.nChoices; ++k) box.addItem(d.choices[k], k + 1);
        addAndMakeVisible(box); att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, d.id, box); box.setTooltip(d.name);
    }
    void resized() override { auto r = getLocalBounds(); r.removeFromTop(labelH()); box.setBounds(r.withSizeKeepingCentre(r.getWidth(), std::min(r.getHeight(), 26))); }
    void paint(juce::Graphics& g) override { g.setColour(col::cream); g.setFont(font((float)labelH() * 0.82f, true)); g.drawFittedText(name, getLocalBounds().removeFromTop(labelH()), juce::Justification::centred, 1); }
    juce::ComboBox box;
private:
    int labelH() const { return juce::jlimit(12, 18, getHeight() / 3); }
    juce::String name; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};
} // namespace ui
