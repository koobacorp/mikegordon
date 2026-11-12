#include "PluginEditor.h"
using namespace mg; using namespace ui;

// ===================================================================================================================== Card
Card::Card(MikeGordonProcessor& p, const juce::String& t, int sw, int ls) : title(t), proc(p), lockSec(ls) {
    if (sw >= 0) { onBtn = std::make_unique<Toggle>(p, sw, "ON"); addAndMakeVisible(*onBtn); onBtn->setTooltip("Switch the " + t + " section on or off"); }
    lockBtn.setClickingTogglesState(true); lockBtn.setTooltip("Lock: Randomize leaves this section alone"); addAndMakeVisible(lockBtn);
    lockBtn.setToggleState(p.locked(ls), juce::dontSendNotification); lockBtn.onClick = [this] { proc.setLocked(lockSec, lockBtn.getToggleState()); };
    moreBtn.setTooltip("Show the less-used controls of this section"); moreBtn.onClick = [this] { setMore(!moreOpen); }; addAndMakeVisible(moreBtn);
}
void Card::setMore(bool b) { moreOpen = b; moreBtn.setButtonText(b ? "back" : "more"); refresh(); resized(); repaint(); }
void Card::refresh() {
    moreBtn.setVisible(!more.empty());
    if (typeRow) typeRow->setVisible(!moreOpen); if (big) big->setVisible(!moreOpen);
    for (auto& s : mediums) { int k = s.pick ? s.pick() : 0; for (size_t j = 0; j < s.opts.size(); ++j) s.opts[j]->setVisible(!moreOpen && (int)j == k); }
    for (auto* c : toggles) c->setVisible(!moreOpen); for (auto* c : more) c->setVisible(moreOpen);
    if (lockBtn.getToggleState() != proc.locked(lockSec)) lockBtn.setToggleState(proc.locked(lockSec), juce::dontSendNotification);
    float alpha = (onBtn && !onBtn->getToggleState()) ? 0.55f : 1.f; if (big) big->setAlpha(alpha); for (auto& s : mediums) for (auto* o : s.opts) o->setAlpha(alpha);
}
void Card::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colours::black.withAlpha(0.25f)); g.fillRoundedRectangle(r.translated(0, 3), 12);
    juce::ColourGradient gr(col::cardHi, 0, r.getY(), col::card, 0, r.getBottom(), false); g.setGradientFill(gr); g.fillRoundedRectangle(r, 12);
    g.setColour(col::edge); g.drawRoundedRectangle(r, 12, 1.2f);
    auto h = getLocalBounds().removeFromTop(30).reduced(10, 0);
    if (onBtn) h.removeFromLeft(44);
    if (lockBtn.isVisible() && lockBtn.getX() > h.getX()) h.setRight(lockBtn.getX() - 4); // never run under the header buttons
    g.setColour(col::cream); g.setFont(font(16, true)); g.drawFittedText(title.toUpperCase(), h, juce::Justification::centredLeft, 1, 0.8f);
    if (moreOpen) { g.setColour(col::honey); g.setFont(font(12)); g.drawText("less-used controls", getLocalBounds().removeFromBottom(18).reduced(10, 0), juce::Justification::centredLeft); }
}
static void gridLayout(std::vector<juce::Component*>& v, juce::Rectangle<int> r, int cols, int rowH) {
    int w = r.getWidth() / cols; for (size_t i = 0; i < v.size(); ++i) { int c = (int)i % cols, row = (int)i / cols; v[i]->setBounds(juce::Rectangle<int>(r.getX() + c * w, r.getY() + row * rowH, w, rowH).reduced(3, 2)); }
}
void Card::resized() {
    auto r = getLocalBounds().reduced(8, 0); auto h = r.removeFromTop(30).reduced(0, 5);
    if (onBtn) onBtn->setBounds(h.removeFromLeft(40));
    moreBtn.setBounds(h.removeFromRight(46)); h.removeFromRight(4); lockBtn.setBounds(h.removeFromRight(40));
    r.removeFromBottom(6);
    if (moreOpen) { auto m = r.withTrimmedBottom(16); gridLayout(more, m, 3, std::min(76, m.getHeight() / std::max(1, ((int)more.size() + 2) / 3))); return; }
    if (typeRow) { typeRow->setBounds(r.removeFromTop(24)); r.removeFromTop(4); }
    if (!toggles.empty()) { auto t = r.removeFromBottom(24); int w = t.getWidth() / (int)toggles.size(); for (auto* c : toggles) c->setBounds(t.removeFromLeft(w).reduced(3, 0)); r.removeFromBottom(4); }
    int medH = mediums.empty() ? 0 : 74; auto m = r.removeFromBottom(medH);
    if (big) big->setBounds(r.withSizeKeepingCentre(std::min(r.getWidth(), r.getHeight() + 6), r.getHeight()));
    if (!mediums.empty()) { int w = m.getWidth() / (int)mediums.size(); for (auto& s : mediums) { auto b = m.removeFromLeft(w).reduced(2, 0); for (auto* o : s.opts) o->setBounds(o == s.opts.front() || dynamic_cast<ui::Knob*>(o) ? b : b.withSizeKeepingCentre(b.getWidth(), 46)); } }
}

// ===================================================================================================================== Tone
void AmpTile::paintButton(juce::Graphics& g, bool over, bool) {
    auto r = getLocalBounds().toFloat().reduced(2); bool on = getToggleState();
    g.setColour(on ? col::honey : (over ? col::cardHi.brighter(0.1f) : col::bgDeep)); g.fillRoundedRectangle(r, 10);
    g.setColour(on ? col::honeyHi : col::edge); g.drawRoundedRectangle(r, 10, on ? 2.f : 1.f);
    auto t = r.reduced(10, 6).toNearestInt();
    g.setColour(on ? col::ink : col::cream); g.setFont(font(20, true)); g.drawText(getButtonText(), t.removeFromTop(t.getHeight() / 2 + 4), juce::Justification::bottomLeft);
    g.setColour(on ? col::ink.withAlpha(0.8f) : col::creamDim); g.setFont(font(12)); g.drawFittedText(desc, t, juce::Justification::topLeft, 2);
}
ToneCard::ToneCard(MikeGordonProcessor& p) : Card(p, "Tone", P_AMP_ON, S_TONE) {
    const char* d[4] = {"Crystal clean, stereo chorus", "Round clean, spring + trem", "Bright, breaks up a little", "Pitch vibrato wobble"};
    for (int k = 0; k < 4; ++k) { auto* t = tiles.add(new AmpTile(kVoices[k], d[k])); addAndMakeVisible(t); t->setTooltip(juce::String(kVoices[k]) + ": " + d[k]);
        t->onClick = [this, k] { if (voiceAtt) voiceAtt->setValueAsCompleteGesture((float)k); }; }
    voiceAtt = std::make_unique<juce::ParameterAttachment>(*p.param(P_AMP_VOICE), [this](float v) { int c = (int)std::lround(v); for (int k = 0; k < 4; ++k) tiles[k]->setToggleState(k == c, juce::dontSendNotification); refreshTone(); });
    voiceAtt->sendInitialUpdate();
    big = own(new Knob(p, P_AMP_DRIVE));
    vfx = own(new Knob(p, P_AMP_VFX, "Chorus")); speed = own(new Knob(p, P_AMP_VRATE, "Speed"));
    mediums = {{{vfx}, nullptr}, {{speed}, nullptr}};
    for (int i : {P_AMP_BASS, P_AMP_MID, P_AMP_TREBLE, P_AMP_VERB}) smalls.push_back(own(new Knob(p, i, i == P_AMP_VERB ? "Spring" : juce::String())));
    smalls.push_back(own(new Toggle(p, P_AMP_BRIGHT)));
    refreshTone();
}
void ToneCard::refreshTone() {
    if (!vfx) return; int v = (int)std::lround(proc.value(P_AMP_VOICE)); const char* n[4] = {"Chorus", "Trem", "Sparkle", "Vibrato"}; vfx->setLabel(n[juce::jlimit(0, 3, v)]); speed->setEnabled(v != 2);
}
void ToneCard::resized() {
    auto r = getLocalBounds().reduced(8, 0); auto h = r.removeFromTop(30).reduced(0, 5);
    onBtn->setBounds(h.removeFromLeft(40)); h.removeFromRight(50); lockBtn.setBounds(h.removeFromRight(40)); moreBtn.setVisible(false);
    r.removeFromBottom(8);
    auto tilesR = r.removeFromTop(172); int tw = tilesR.getWidth() / 2, th = tilesR.getHeight() / 2;
    for (int k = 0; k < 4; ++k) tiles[k]->setBounds(tilesR.getX() + (k % 2) * tw, tilesR.getY() + (k / 2) * th, tw, th);
    r.removeFromTop(8);
    auto sm = r.removeFromBottom(78); auto bright = sm.removeFromRight(56);
    int w = sm.getWidth() / 4; for (int i = 0; i < 4; ++i) smalls[(size_t)i]->setBounds(sm.removeFromLeft(w).reduced(2, 0));
    smalls[4]->setBounds(bright.withSizeKeepingCentre(52, 26));
    r.removeFromBottom(6);
    auto bigR = r.removeFromLeft(r.getWidth() * 55 / 100); big->setBounds(bigR.withSizeKeepingCentre(std::min(bigR.getWidth(), bigR.getHeight()), bigR.getHeight()));
    vfx->setBounds(r.removeFromTop(r.getHeight() / 2).reduced(6, 2)); speed->setBounds(r.reduced(6, 2));
}

// ===================================================================================================================== Order strip
static const char* kMovNames[kMovable] = {"Tone", "Filter", "Pitch", "Motion", "Texture", "Space"};
juce::Rectangle<int> OrderStrip::chip(int slot) const { int x0 = 56, w = (getWidth() - x0 - 90) / kMovable; return {x0 + slot * w + 3, 4, w - 16, getHeight() - 8}; }
void OrderStrip::paint(juce::Graphics& g) {
    g.setColour(col::creamDim); g.setFont(font(12, true)); g.drawText("ORDER", 0, 0, 52, getHeight(), juce::Justification::centredLeft);
    auto o = proc.getOrder();
    for (int s = 0; s < kMovable; ++s) {
        auto r = chip(s); if (s == dragging) continue;
        bool on = proc.value(sectionSwitch(o[(size_t)s])) >= 0.5f;
        g.setColour(on ? col::cardHi : col::bgDeep); g.fillRoundedRectangle(r.toFloat(), 8); g.setColour(on ? col::honey : col::edge); g.drawRoundedRectangle(r.toFloat(), 8, 1);
        g.setColour(on ? col::cream : col::creamDim); g.setFont(font(13, true)); g.drawText(kMovNames[o[(size_t)s]], r, juce::Justification::centred);
        if (s < kMovable - 1) { g.setColour(col::honeyDark); g.drawText(">", r.getRight(), r.getY(), 16, r.getHeight(), juce::Justification::centred); }
    }
    g.setColour(col::creamDim); g.setFont(font(12)); g.drawText("> Width > Out", getWidth() - 88, 0, 88, getHeight(), juce::Justification::centredRight);
    if (dragging >= 0) { auto r = chip(dragging).withX((int)dragX - chip(0).getWidth() / 2); g.setColour(col::honey); g.fillRoundedRectangle(r.toFloat(), 8); g.setColour(col::ink); g.setFont(font(13, true)); g.drawText(kMovNames[o[(size_t)dragging]], r, juce::Justification::centred); }
}
void OrderStrip::mouseDown(const juce::MouseEvent& e) { dragging = -1; for (int s = 0; s < kMovable; ++s) if (chip(s).contains(e.getPosition())) { dragging = s; dragX = (float)e.x; } repaint(); }
void OrderStrip::mouseDrag(const juce::MouseEvent& e) {
    if (dragging < 0) return; dragX = (float)e.x; int target = dragging;
    for (int s = 0; s < kMovable; ++s) { auto r = chip(s); if (e.x >= r.getX() - 8 && e.x < r.getRight() + 8) target = s; }
    if (target != dragging) { auto o = proc.getOrder(); int v = o[(size_t)dragging]; std::vector<int> l(o.begin(), o.end()); l.erase(l.begin() + dragging); l.insert(l.begin() + target, v);
        std::array<int, kMovable> n; for (int i = 0; i < kMovable; ++i) n[(size_t)i] = l[(size_t)i]; proc.setOrder(n); dragging = target; }
    repaint();
}
void OrderStrip::mouseUp(const juce::MouseEvent&) { dragging = -1; repaint(); }

// ===================================================================================================================== Macros
MacroBox::MacroBox(MikeGordonProcessor& p, int kk) : proc(p), k(kk) {
    knob = std::make_unique<Knob>(p, P_MACRO1 + kk, "Macro " + juce::String(kk + 1)); addAndMakeVisible(*knob);
    assign.setTooltip("Choose which knobs this macro turns"); assign.onClick = [this] { showAssign(); }; addAndMakeVisible(assign);
}
void MacroBox::resized() { auto r = getLocalBounds(); auto right = r.removeFromRight(r.getWidth() - 64); knob->setBounds(r); right.removeFromTop(22); assign.setBounds(right.removeFromTop(22).withWidth(std::min(64, right.getWidth()))); }
void MacroBox::paint(juce::Graphics& g) { auto r = getLocalBounds().withTrimmedLeft(66); g.setColour(col::honey); g.setFont(font(13, true)); g.drawFittedText(proc.macroName(k), r.removeFromTop(22), juce::Justification::centredLeft, 1);
    auto m = proc.macros(); int n = 0; for (auto& s : m.s[k]) n += s.param >= 0; g.setColour(col::creamDim); g.setFont(font(11)); g.drawText(juce::String(n) + (n == 1 ? " target" : " targets"), r.withTrimmedTop(26), juce::Justification::centredLeft); }
void MacroBox::showAssign() {
    juce::PopupMenu menu; auto m = proc.macros();
    menu.addSectionHeader("Macro " + juce::String(k + 1) + " turns:");
    for (int s = 0; s < kMacroSlots; ++s) { auto& sl = m.s[k][s]; if (sl.param < 0) continue;
        menu.addItem(juce::String(kParams[sl.param].name) + "  (" + (sl.depth > 0 ? "+" : "") + juce::String((int)std::lround(sl.depth * 100)) + "%)  - click to remove", [this, s] { auto mm = proc.macros(); mm.s[k][s] = {}; proc.setMacros(mm); repaint(); }); }
    menu.addSeparator();
    juce::PopupMenu add; const float depths[] = {0.25f, 0.5f, 1.f, -0.25f, -0.5f, -1.f};
    for (int sec = S_TONE; sec <= S_WIDTH; ++sec) { juce::PopupMenu sm;
        for (int i = 0; i < kNumParams; ++i) { if (kParams[i].section != sec || kParams[i].kind != K_FLOAT) continue; juce::PopupMenu dm;
            for (float dpt : depths) dm.addItem((dpt > 0 ? "+" : "") + juce::String((int)std::lround(dpt * 100)) + "%", [this, i, dpt] { auto mm = proc.macros(); for (auto& sl : mm.s[k]) if (sl.param < 0 || sl.param == i) { sl = {i, dpt}; break; } proc.setMacros(mm); repaint(); });
            sm.addSubMenu(kParams[i].name, dm); }
        add.addSubMenu(sectionName(sec), sm); }
    int used = 0; for (auto& sl : m.s[k]) used += sl.param >= 0;
    menu.addSubMenu("Add a target", add, used < kMacroSlots);
    menu.addItem("Reset to default", [this] { auto mm = proc.macros(); auto d = defaultMacros(); for (int s = 0; s < kMacroSlots; ++s) mm.s[k][s] = d.s[k][s]; proc.setMacros(mm); repaint(); });
    menu.addItem("Clear", [this] { auto mm = proc.macros(); for (auto& sl : mm.s[k]) sl = {}; proc.setMacros(mm); repaint(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&assign));
}

// ===================================================================================================================== Editor
MikeGordonEditor::MikeGordonEditor(MikeGordonProcessor& p) : AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&look); addAndMakeVisible(content);
    for (auto* b : {&prevBtn, &nextBtn, &presetBtn, &saveBtn, &randomBtn}) content.addAndMakeVisible(b);
    prevBtn.setTooltip("Previous preset"); nextBtn.setTooltip("Next preset"); presetBtn.setTooltip("Pick a vibe preset"); saveBtn.setTooltip("Save your own preset"); randomBtn.setTooltip("Roll a new vibe (locked sections stay as they are)");
    prevBtn.onClick = [this] { stepPreset(-1); }; nextBtn.onClick = [this] { stepPreset(1); }; presetBtn.onClick = [this] { showPresetMenu(); }; saveBtn.onClick = [this] { savePreset(); };
    randomBtn.onClick = [this] { proc.randomize(); };

    tone = std::make_unique<ToneCard>(p); content.addAndMakeVisible(*tone);
    auto mk = [&](const char* t, int sw, int sec) { cards.push_back(std::make_unique<Card>(p, t, sw, sec)); content.addAndMakeVisible(*cards.back()); return cards.back().get(); };
    { auto* c = mk("Filter", P_FLT_ON, S_FILTER); c->typeRow = c->own(new Segmented(p, P_FLT_MOVE)); c->big = c->own(new Knob(p, P_FLT_LP));
      c->mediums = {{{c->own(new Knob(p, P_FLT_RES))}, nullptr}, {{c->own(new Knob(p, P_FLT_HP))}, nullptr}, {{c->own(new Knob(p, P_FLT_DEPTH, "Move"))}, nullptr}};
      c->more = {c->own(new Toggle(p, P_FLT_SYNC, "Sync")), c->own(new Choice(p, P_FLT_DIV, "Sync Rate")), c->own(new Knob(p, P_FLT_RATE, "Free Rate")), c->own(new Knob(p, P_FLT_SENS, "Env Sens"))}; }
    { auto* c = mk("Pitch", P_PIT_ON, S_PITCH); c->big = c->own(new Knob(p, P_PIT_DETUNE));
      c->mediums = {{{c->own(new Knob(p, P_PIT_DOWN, "Oct Down"))}, nullptr}, {{c->own(new Knob(p, P_PIT_UP, "Oct Up"))}, nullptr}, {{c->own(new Knob(p, P_PIT_WIDTH, "Width"))}, nullptr}}; }
    { auto* c = mk("Motion", P_MOT_ON, S_MOTION); c->typeRow = c->own(new Segmented(p, P_MOT_TYPE)); c->big = c->own(new Knob(p, P_MOT_DEPTH, "Depth"));
      c->mediums = {{{c->own(new Knob(p, P_MOT_RATE, "Rate")), c->own(new Choice(p, P_MOT_DIV, "Rate"))}, [&p] { return p.value(P_MOT_SYNC) >= 0.5f ? 1 : 0; }},
                    {{c->own(new Knob(p, P_MOT_MIX, "Mix"))}, nullptr}, {{c->own(new Knob(p, P_MOT_FB))}, nullptr}};
      c->toggles = {c->own(new Toggle(p, P_MOT_SYNC, "Sync to tempo"))}; c->more = {c->own(new Knob(p, P_MOT_SPREAD))}; }
    { auto* c = mk("Texture", P_TEX_ON, S_TEXTURE); c->big = c->own(new Knob(p, P_TEX_WOBBLE));
      c->mediums = {{{c->own(new Knob(p, P_TEX_TAPE))}, nullptr}, {{c->own(new Knob(p, P_TEX_CRACKLE))}, nullptr}, {{c->own(new Knob(p, P_TEX_HISS))}, nullptr}, {{c->own(new Knob(p, P_TEX_CRUSH, "Crush"))}, nullptr}};
      c->more = {c->own(new Toggle(p, P_TEX_GATE, "Noise follows input"))}; }
    { auto* c = mk("Space", P_SPC_ON, S_SPACE); c->typeRow = c->own(new Segmented(p, P_SPC_TYPE)); c->big = c->own(new Knob(p, P_SPC_MIX, "Mix"));
      // when Echo is picked the third medium shows the echo time (a tempo division when synced) instead of Shimmer
      auto pickTime = [&p] { if ((int)std::lround(p.value(P_SPC_TYPE)) != 2) return 0; return p.value(P_SPC_SYNC) >= 0.5f ? 2 : 1; };
      c->mediums = {{{c->own(new Knob(p, P_SPC_DECAY))}, nullptr}, {{c->own(new Knob(p, P_SPC_TONE, "Tone"))}, nullptr},
                    {{c->own(new Knob(p, P_SPC_SHIMMER)), c->own(new Knob(p, P_SPC_TIME, "Time")), c->own(new Choice(p, P_SPC_DIV, "Time"))}, pickTime}};
      c->toggles = {c->own(new Toggle(p, P_SPC_FREEZE))};
      c->more = {c->own(new Knob(p, P_SPC_PRE)), c->own(new Toggle(p, P_SPC_SYNC)), c->own(new Toggle(p, P_SPC_PING)), c->own(new Knob(p, P_SPC_SHIMMER, "Shimmer"))}; }
    { auto* c = mk("Width", P_WID_ON, S_WIDTH); c->big = c->own(new Knob(p, P_WID_WIDTH));
      c->mediums = {{{c->own(new Knob(p, P_WID_FREQ, "Mono below"))}, nullptr}, {{c->own(new Knob(p, P_DUK_DEPTH, "Duck"))}, nullptr}, {{c->own(new Choice(p, P_DUK_DIV, "Pump"))}, nullptr}};
      c->toggles = {c->own(new Toggle(p, P_WID_MONO, "Mono bass")), c->own(new Toggle(p, P_DUK_ON, "Duck"))};
      c->more = {c->own(new Choice(p, P_DUK_SRC, "Duck source")), c->own(new Choice(p, P_DUK_TARGET, "Duck target")), c->own(new Knob(p, P_DUK_SHAPE)), c->own(new Knob(p, P_DUK_THRESH, "Threshold")), c->own(new Knob(p, P_DUK_RELEASE, "Release"))}; }

    amount = std::make_unique<Knob>(p, P_AMOUNT, "AMOUNT"); content.addAndMakeVisible(*amount); amount->s.setTooltip("How much of the vibe you hear. 0% = your original sound");
    before = std::make_unique<Toggle>(p, P_COMPARE, "BEFORE"); content.addAndMakeVisible(*before); before->setTooltip("Hear your original sound for a quick before/after check");
    outGain = std::make_unique<Knob>(p, P_OUTGAIN); content.addAndMakeVisible(*outGain);
    match = std::make_unique<Toggle>(p, P_MATCH, "Level match"); content.addAndMakeVisible(*match); match->setTooltip("Keeps the processed sound as loud as the original, so you judge the vibe, not the volume");
    limit = std::make_unique<Toggle>(p, P_LIMIT, "Safety limiter"); content.addAndMakeVisible(*limit); limit->setTooltip("Catches peaks above -1 dB so nothing clips");
    orderStrip = std::make_unique<OrderStrip>(p); content.addAndMakeVisible(*orderStrip);
    for (int k = 0; k < kMacros; ++k) { macroBoxes.push_back(std::make_unique<MacroBox>(p, k)); content.addAndMakeVisible(*macroBoxes.back()); }

    setResizable(true, true); setResizeLimits(kW * 6 / 10, kH * 6 / 10, kW * 2, kH * 2); getConstrainer()->setFixedAspectRatio((double)kW / kH);
    setSize(kW, kH); timerCallback(); startTimerHz(20);
}
MikeGordonEditor::~MikeGordonEditor() { stopTimer(); setLookAndFeel(nullptr); }

void MikeGordonEditor::paint(juce::Graphics& g) {
    float s = getWidth() / (float)kW; g.addTransform(juce::AffineTransform::scale(s));
    juce::ColourGradient bg(col::bg, 0, 0, col::bgDeep, 0, (float)kH, false); g.setGradientFill(bg); g.fillRect(0, 0, kW, kH);
    g.setColour(col::honey); g.setFont(font(28, true)); g.drawText("MIKE GORDON", 22, 12, 260, 40, juce::Justification::centredLeft);
    g.setColour(col::creamDim); g.setFont(font(13)); g.drawText("vibe + character box", 24, 44, 260, 16, juce::Justification::centredLeft);
    // amount panel
    juce::Rectangle<float> ap(1018, 78, 208, 530); g.setColour(juce::Colours::black.withAlpha(0.25f)); g.fillRoundedRectangle(ap.translated(0, 3), 12);
    g.setColour(col::card); g.fillRoundedRectangle(ap, 12); g.setColour(col::honeyDark); g.drawRoundedRectangle(ap, 12, 1.5f);
    g.setColour(col::creamDim); g.setFont(font(12)); g.drawFittedText(readout, juce::Rectangle<int>(1028, 574, 188, 28), juce::Justification::centred, 2);
    // bottom strip
    juce::Rectangle<float> bs(14, 620, 1212, 86); g.setColour(col::bgDeep.withAlpha(0.7f)); g.fillRoundedRectangle(bs, 12); g.setColour(col::edge); g.drawRoundedRectangle(bs, 12, 1);
}
void MikeGordonEditor::resized() {
    content.setBounds(0, 0, kW, kH); float s = getWidth() / (float)kW; content.setTransform(juce::AffineTransform::scale(s));
    prevBtn.setBounds(392, 16, 38, 38); presetBtn.setBounds(436, 14, 370, 42); nextBtn.setBounds(812, 16, 38, 38); saveBtn.setBounds(860, 18, 66, 34);
    randomBtn.setBounds(1060, 16, 166, 38);
    tone->setBounds(14, 78, 320, 530);
    for (size_t i = 0; i < cards.size(); ++i) { int c = (int)i % 3, r = (int)i / 3; cards[i]->setBounds(346 + c * 223, 78 + r * 270, 213, 260); }
    amount->setBounds(1028, 90, 188, 220); before->setBounds(1040, 324, 164, 48);
    outGain->setBounds(1068, 386, 108, 96); match->setBounds(1036, 494, 172, 28); limit->setBounds(1036, 530, 172, 28);
    for (int k = 0; k < kMacros; ++k) macroBoxes[(size_t)k]->setBounds(28 + k * 150, 626, 146, 74);
    orderStrip->setBounds(500, 638, 716, 50);
}
void MikeGordonEditor::timerCallback() {
    presetBtn.setButtonText(proc.presetName() + "  v");
    tone->refresh(); tone->refreshTone(); for (auto& c : cards) c->refresh();
    orderStrip->repaint(); for (auto& m : macroBoxes) m->repaint();
    auto& e = proc.engine(); juce::String r = "Level match " + juce::String(e.matchDb.load(), 1) + " dB"; float l = e.limitDb.load(); if (l < -0.05f) r << "\nLimiter " << juce::String(l, 1) << " dB";
    if (r != readout) { readout = r; repaint(); }
}
void MikeGordonEditor::stepPreset(int d) { int n = (int)factoryPresets().size(); proc.loadFactory((proc.getCurrentProgram() + d + n) % n); }
void MikeGordonEditor::showPresetMenu() {
    juce::PopupMenu menu; const auto& ps = factoryPresets(); juce::StringArray cats; for (auto& p : ps) cats.addIfNotAlreadyThere(p.category);
    for (auto& c : cats) { juce::PopupMenu sm; for (size_t i = 0; i < ps.size(); ++i) if (c == ps[i].category) sm.addItem(ps[i].name, true, (int)i == proc.getCurrentProgram() && proc.presetName() == ps[i].name, [this, i] { proc.loadFactory((int)i); }); menu.addSubMenu(c, sm); }
    auto user = proc.userPresets(); juce::PopupMenu um; for (auto& f : user) um.addItem(f.getFileNameWithoutExtension(), [this, f] { proc.loadUserPreset(f); });
    if (user.isEmpty()) um.addItem("(none saved yet)", false, false, [] {});
    menu.addSeparator(); menu.addSubMenu("My presets", um); menu.addItem("Save current as...", [this] { savePreset(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn).withMinimumWidth(presetBtn.getWidth()));
}
void MikeGordonEditor::savePreset() {
    auto* w = new juce::AlertWindow("Save preset", "Name your preset. It goes in Music/MIKE GORDON Presets.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", proc.presetName()); w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MikeGordonEditor> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w](int r) { if (r == 1 && safe) safe->proc.saveUserPreset(w->getTextEditorContents("name")); }), true);
}
