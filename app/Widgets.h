#pragma once
#include "Theme.h"
#include "core/Engine.h"
#include <cmath>
#include <vector>

// ============================================================================
//  Componentes de tela ligados ao motor de áudio
// ============================================================================

// ---------------------------------------------------------------- Kit visual
class DrumVisual : public juce::Component {
public:
    DrumVisual(nexus::Engine& e, bool hybridMode) : engine(e), hybrid(hybridMode) {}
    std::function<void(int)> onSelect;
    int selected = 0;

    void tick() {
        bool dirty = false;
        for (int p = 0; p < nexus::kNumPads; ++p) {
            const unsigned h = engine.status.hitCount[p].load();
            if (h != lastHits[p]) { lastHits[p] = h; flash[p] = 1.0f; }
            if (flash[p] > 0.01f) { flash[p] *= 0.84f; dirty = true; }
        }
        if (dirty) repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (hybrid || !onSelect) return;
        const auto pt = e.position;
        if (pt.getDistanceFrom(kickC()) < kickR() * 1.05f) onSelect(0);
        else if (snareBox().expanded(6.0f).contains(pt)) onSelect(1);
    }

    void paint(juce::Graphics& g) override {
        using namespace juce;
        const auto r = getLocalBounds().toFloat();
        g.setGradientFill(ColourGradient(theme::panelTop, 0, 0, theme::bg, 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 12.0f);
        Graphics::ScopedSaveState ss(g);
        Path clip; clip.addRoundedRectangle(r, 12.0f); g.reduceClipRegion(clip);
        // luz de palco
        g.setGradientFill(ColourGradient(theme::accent.withAlpha(0.30f), r.getCentreX(), -r.getHeight() * 0.05f, Colours::transparentBlack, r.getCentreX(), r.getHeight() * 0.8f, true));
        g.fillRect(r);
        g.setGradientFill(ColourGradient(theme::accent.withAlpha(0.10f), r.getCentreX(), r.getBottom(), Colours::transparentBlack, r.getCentreX(), r.getBottom() - r.getHeight() * 0.35f, true));
        g.fillRect(r);

        const float w = r.getWidth(), h = r.getHeight();
        // pratos e estantes (decoração)
        cymbal(g, {w * 0.30f, h * 0.20f}, w * 0.12f, h * 0.55f);
        cymbal(g, {w * 0.79f, h * 0.26f}, w * 0.13f, h * 0.60f);
        hat(g, {w * 0.09f, h * 0.40f}, w * 0.075f, h * 0.60f);
        // tons (decoração)
        deco(g, {w * 0.42f, h * 0.40f}, w * 0.075f);
        deco(g, {w * 0.60f, h * 0.40f}, w * 0.075f);
        deco(g, {w * 0.82f, h * 0.60f}, w * 0.095f);

        // pad 2 (caixa)
        const bool snareOn = !hybrid;
        snare(g, snareOn ? selected == 1 : false, snareOn ? flash[1] : 0.0f, snareOn);
        // pad 1 (bumbo)
        kick(g, selected == 0, flash[0]);

        // etiquetas dos microfones
        if (hybrid) {
            chip(g, theme::u8("MIC 1 (E)"), {w * 0.27f, h * 0.90f}, true);
            chip(g, theme::u8("MIC 2 (D)"), {w * 0.73f, h * 0.90f}, true);
            chip(g, theme::u8("GATILHO · soma dos 2 mics"), {w * 0.5f, h * 0.08f}, selected == 0);
        } else {
            chip(g, theme::u8("PAD 1 · MIC 1"), {kickC().x, kickC().y + kickR() + 20.0f}, selected == 0);
            chip(g, theme::u8("PAD 2 · MIC 2"), {snareBox().getCentreX(), snareBox().getBottom() + 22.0f}, selected == 1);
        }
    }

private:
    juce::Point<float> kickC() const { return {static_cast<float>(getWidth()) * 0.50f, static_cast<float>(getHeight()) * 0.60f}; }
    float kickR() const { return juce::jmin(static_cast<float>(getHeight()) * 0.30f, static_cast<float>(getWidth()) * 0.17f); }
    juce::Rectangle<float> snareBox() const {
        const float rx = static_cast<float>(getWidth()) * 0.10f, ry = rx * 0.36f, depth = static_cast<float>(getHeight()) * 0.10f;
        const juce::Point<float> c{static_cast<float>(getWidth()) * 0.22f, static_cast<float>(getHeight()) * 0.62f};
        return {c.x - rx, c.y - ry, rx * 2, ry * 2 + depth};
    }

    void glow(juce::Graphics& g, juce::Path p, float f, float th) {
        for (int k = 4; k >= 1; --k) {
            g.setColour(theme::accent.withAlpha(f * 0.10f * static_cast<float>(5 - k)));
            g.strokePath(p, juce::PathStrokeType(th + static_cast<float>(k) * 5.0f));
        }
    }

    void kick(juce::Graphics& g, bool sel, float f) {
        using namespace juce;
        const auto c = kickC(); const float R = kickR();
        Path ring; ring.addEllipse(c.x - R, c.y - R, R * 2, R * 2);
        if (f > 0.02f) glow(g, ring, f, 6.0f);
        g.setColour(Colour(0xff0a0c0b)); g.fillPath(ring);
        g.setColour(sel ? theme::accent : Colour(0xff4b5651));
        g.strokePath(ring, PathStrokeType(7.0f));
        const float hr = R - 10.0f;
        g.setGradientFill(ColourGradient(Colour(0xff34403b), c.x, c.y - hr * 0.5f, Colour(0xff090c0b), c.x, c.y + hr, true));
        g.fillEllipse(c.x - hr, c.y - hr, hr * 2, hr * 2);
        if (f > 0.02f) { g.setColour(theme::accent.withAlpha(f * 0.45f)); g.fillEllipse(c.x - hr, c.y - hr, hr * 2, hr * 2); }
        g.setColour(Colours::white.withAlpha(0.9f)); g.setFont(theme::font(R * 0.30f, true));
        g.drawText("NEXUS", Rectangle<float>(c.x - hr, c.y - R * 0.18f, hr * 2, R * 0.36f), Justification::centred);
        g.setColour(theme::accent); g.setFont(theme::font(R * 0.11f, true));
        g.drawText("DRUM TRIGGER", Rectangle<float>(c.x - hr, c.y + R * 0.14f, hr * 2, R * 0.16f), Justification::centred);
    }

    void snare(juce::Graphics& g, bool sel, float f, bool active) {
        using namespace juce;
        const auto b = snareBox();
        const float rx = b.getWidth() * 0.5f, ry = rx * 0.36f, depth = b.getHeight() - ry * 2;
        const Point<float> c{b.getCentreX(), b.getY() + ry};
        const float dim = active ? 1.0f : 0.45f;
        // corpo
        ColourGradient body(Colour(0xff0b0e0d).withMultipliedAlpha(dim), c.x - rx, 0, Colour(0xff0b0e0d).withMultipliedAlpha(dim), c.x + rx, 0, false);
        body.addColour(0.5, Colour(0xff2d3632).withMultipliedAlpha(dim));
        g.setGradientFill(body);
        g.fillEllipse(c.x - rx, c.y + depth - ry, rx * 2, ry * 2);
        g.fillRect(c.x - rx, c.y, rx * 2, depth);
        Path top; top.addEllipse(c.x - rx, c.y - ry, rx * 2, ry * 2);
        if (f > 0.02f) glow(g, top, f, 4.0f);
        g.setGradientFill(ColourGradient(Colour(0xff3b4742).withMultipliedAlpha(dim), c.x, c.y - ry, Colour(0xff111513).withMultipliedAlpha(dim), c.x, c.y + ry, false));
        g.fillPath(top);
        if (f > 0.02f) { g.setColour(theme::accent.withAlpha(f * 0.4f)); g.fillPath(top); }
        g.setColour(sel ? theme::accent : Colour(0xff6b7771).withMultipliedAlpha(dim));
        g.strokePath(top, PathStrokeType(3.0f));
        g.setColour(Colour(0xff6b7771).withMultipliedAlpha(dim)); // aro inferior
        g.drawEllipse(c.x - rx, c.y + depth - ry, rx * 2, ry * 2, 1.5f);
    }

    void cymbal(juce::Graphics& g, juce::Point<float> c, float rx, float standY) {
        using namespace juce;
        g.setColour(Colour(0xff2a302d)); g.drawLine(c.x, c.y, c.x, standY, 2.0f);
        const float ry = rx * 0.17f;
        g.setGradientFill(ColourGradient(Colour(0xffc9a23a), c.x - rx, c.y, Colour(0xff4b3a10), c.x + rx, c.y, false));
        g.fillEllipse(c.x - rx, c.y - ry, rx * 2, ry * 2);
        g.setColour(Colour(0xffe6cc72).withAlpha(0.6f)); g.drawEllipse(c.x - rx, c.y - ry, rx * 2, ry * 2, 1.0f);
        g.setColour(Colour(0xff1a1405)); g.fillEllipse(c.x - rx * 0.12f, c.y - ry * 0.4f, rx * 0.24f, ry * 0.8f);
    }
    void hat(juce::Graphics& g, juce::Point<float> c, float rx, float standY) {
        cymbal(g, c, rx, standY);
        using namespace juce;
        const float ry = rx * 0.17f;
        g.setColour(Colour(0xff8f7524)); g.fillEllipse(c.x - rx, c.y - ry + 5.0f, rx * 2, ry * 2);
    }
    void deco(juce::Graphics& g, juce::Point<float> c, float rx) {
        using namespace juce;
        const float ry = rx * 0.34f, depth = rx * 0.7f;
        g.setColour(Colour(0xff0d1110)); g.fillEllipse(c.x - rx, c.y + depth - ry, rx * 2, ry * 2); g.fillRect(c.x - rx, c.y, rx * 2, depth);
        g.setGradientFill(ColourGradient(Colour(0xff2a322e), c.x, c.y - ry, Colour(0xff0d1110), c.x, c.y + ry, false));
        g.fillEllipse(c.x - rx, c.y - ry, rx * 2, ry * 2);
        g.setColour(Colour(0xff4a5651)); g.drawEllipse(c.x - rx, c.y - ry, rx * 2, ry * 2, 2.0f);
    }
    void chip(juce::Graphics& g, const juce::String& t, juce::Point<float> c, bool on) {
        using namespace juce;
        g.setFont(theme::font(12.0f, true));
        const float w = g.getCurrentFont().getStringWidthFloat(t) + 30.0f;
        Rectangle<float> r(c.x - w * 0.5f, c.y - 12.0f, w, 24.0f);
        g.setColour(Colour(0xcc060807)); g.fillRoundedRectangle(r, 12.0f);
        g.setColour(on ? theme::accent : theme::edge.brighter(0.3f)); g.drawRoundedRectangle(r, 12.0f, 1.2f);
        g.setColour(on ? theme::accent : theme::muted); g.fillEllipse(r.getX() + 9.0f, c.y - 3.5f, 7.0f, 7.0f);
        g.setColour(theme::text); g.drawText(t, r.withTrimmedLeft(20.0f), Justification::centred);
    }

    nexus::Engine& engine;
    bool hybrid;
    unsigned lastHits[nexus::kNumPads] = {0, 0};
    float flash[nexus::kNumPads] = {0.0f, 0.0f};
};

// ---------------------------------------------------------------- Forma de onda do som
class WaveView : public juce::Component {
public:
    void setData(const std::vector<float>& d) {
        const int cols = 220;
        peaks.assign(static_cast<size_t>(cols), 0.0f);
        if (d.empty()) { repaint(); return; }
        const size_t per = std::max<size_t>(1, d.size() / static_cast<size_t>(cols));
        for (int c = 0; c < cols; ++c) {
            float pk = 0.0f;
            const size_t a = static_cast<size_t>(c) * per, b = std::min(d.size(), a + per);
            for (size_t i = a; i < b; ++i) pk = std::max(pk, std::fabs(d[i]));
            peaks[static_cast<size_t>(c)] = pk;
        }
        repaint();
    }
    void paint(juce::Graphics& g) override {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour(theme::panelBot); g.fillRoundedRectangle(r, 8.0f);
        g.setColour(theme::edge); g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
        if (peaks.empty()) return;
        const auto in = r.reduced(8.0f, 6.0f);
        const float cy = in.getCentreY(), step = in.getWidth() / static_cast<float>(peaks.size());
        g.setGradientFill(ColourGradient(theme::accent, 0, in.getY(), theme::accentDim, 0, in.getBottom(), false));
        for (size_t i = 0; i < peaks.size(); ++i) {
            const float hh = std::max(1.0f, peaks[i] * in.getHeight() * 0.5f);
            g.fillRect(in.getX() + static_cast<float>(i) * step, cy - hh, std::max(1.0f, step - 0.8f), hh * 2.0f);
        }
    }
private:
    std::vector<float> peaks;
};

// ---------------------------------------------------------------- Detecção (trigger) do pad
class DetectionCard : public Card {
public:
    DetectionCard(nexus::Engine& e, std::function<void()> changed)
        : Card("Detecção (Trigger)"), engine(e),
          sens("Sensibilidade (ganho do mic)", -12, 36, 0.5, " dB", 1),
          thr("Limiar", -60, -6, 0.5, " dB", 1),
          retrig("Retrigger", 20, 300, 1, " ms", 0),
          hp("Filtro grave (HP)", 20, 600, 1, " Hz", 0, 150),
          lp("Filtro agudo (LP)", 500, 16000, 10, " Hz", 0, 3000) {
        for (auto* s : {&sens, &thr, &retrig, &hp, &lp}) { addAndMakeVisible(*s); s->afterChange = changed; }
        rebind();
    }
    void setPad(int p) { pad = p; setTitle(theme::u8("Detecção (Trigger) · Pad ") + juce::String(p + 1)); refresh(); }
    void refresh() { for (auto* s : {&sens, &thr, &retrig, &hp, &lp}) s->refresh(); }
    void resized() override {
        Card::resized();
        auto r = content();
        const int h = juce::jlimit(34, 48, r.getHeight() / 5);
        for (auto* s : {&sens, &thr, &retrig, &hp, &lp}) s->setBounds(r.removeFromTop(h));
    }
private:
    void rebind() {
        auto P = [this]() -> nexus::PadParams& { return engine.params.pad[pad]; };
        sens.bind([=] { return P().inputGainDb.load(); }, [=](float v) { P().inputGainDb = v; });
        thr.bind([=] { return P().thresholdDb.load(); }, [=](float v) { P().thresholdDb = v; });
        retrig.bind([=] { return P().retriggerMs.load(); }, [=](float v) { P().retriggerMs = v; });
        hp.bind([=] { return P().hpHz.load(); }, [=](float v) { P().hpHz = v; });
        lp.bind([=] { return P().lpHz.load(); }, [=](float v) { P().lpHz = v; });
    }
    nexus::Engine& engine;
    int pad = 0;
    BarSlider sens, thr, retrig, hp, lp;
};

// ---------------------------------------------------------------- Som do pad (nome + onda + play)
class SoundPanel : public Card {
public:
    SoundPanel(nexus::Engine& e, std::function<void()> changed) : Card("Som do pad"), engine(e), changed(std::move(changed)) {
        for (auto* b : {&prev, &next}) { addAndMakeVisible(*b); }
        prev.setButtonText(theme::u8("‹")); next.setButtonText(theme::u8("›"));
        prev.onClick = [this] { step(-1); };
        next.onClick = [this] { step(+1); };
        play.setButtonText(theme::u8("▶  Ouvir"));
        play.onClick = [this] { engine.params.previewSound = engine.params.pad[pad].sound.load(); };
        addAndMakeVisible(play); addAndMakeVisible(wave);
        for (auto* l : {&name, &cat}) addAndMakeVisible(*l);
        name.setFont(theme::font(20.0f, true)); name.setJustificationType(juce::Justification::centred);
        cat.setFont(theme::font(13.0f)); cat.setColour(juce::Label::textColourId, theme::accent); cat.setJustificationType(juce::Justification::centred);
    }
    void setPad(int p) { pad = p; setTitle(theme::u8("Som do pad · Pad ") + juce::String(p + 1)); refresh(); }
    void refresh() {
        const auto& s = engine.sounds().get(engine.params.pad[pad].sound.load());
        name.setText(theme::u8(s.name.c_str()), juce::dontSendNotification);
        cat.setText(theme::upper(theme::u8(s.category.c_str())), juce::dontSendNotification);
        if (shownSound != engine.params.pad[pad].sound.load()) { shownSound = engine.params.pad[pad].sound.load(); wave.setData(s.data); }
    }
    void resized() override {
        Card::resized();
        auto r = content();
        auto left = r.removeFromLeft(150);
        auto arrows = left.removeFromBottom(34);
        prev.setBounds(arrows.removeFromLeft(arrows.getWidth() / 2 - 3));
        next.setBounds(arrows.withTrimmedLeft(6));
        cat.setBounds(left.removeFromTop(20).withTrimmedTop(2));
        name.setBounds(left.reduced(0, 2));
        r.removeFromLeft(12);
        auto bottom = r.removeFromBottom(34);
        play.setBounds(bottom.removeFromRight(130));
        wave.setBounds(r.withTrimmedBottom(6));
    }
private:
    void step(int d) {
        const int n = engine.sounds().size();
        engine.params.pad[pad].sound = (engine.params.pad[pad].sound.load() + d + n) % n;
        engine.params.previewSound = engine.params.pad[pad].sound.load();
        changed();
    }
    nexus::Engine& engine;
    std::function<void()> changed;
    int pad = 0, shownSound = -1;
    juce::TextButton prev, next, play;
    juce::Label name, cat;
    WaveView wave;
};

// ---------------------------------------------------------------- Ajuste do som (knobs)
class TuneCard : public Card {
public:
    TuneCard(nexus::Engine& e, std::function<void()> changed)
        : Card(theme::u8("Ajuste do som")), engine(e),
          pitch(theme::u8("Afinação"), -12, 12, 0.1, " st", 0.0, 1),
          level("Volume", 0, 1.5, 0.01, "", 1.0, 2),
          pan("Pan", -1, 1, 0.01, "", 0.0, 2),
          sens(theme::u8("Dinâmica"), 0, 1, 0.01, "", 1.0, 2) {
        for (auto* k : {&pitch, &level, &pan, &sens}) { addAndMakeVisible(*k); k->afterChange = changed; }
        auto P = [this]() -> nexus::PadParams& { return engine.params.pad[pad]; };
        pitch.bind([=] { return P().pitchSemis.load(); }, [=](float v) { P().pitchSemis = v; });
        level.bind([=] { return P().level.load(); }, [=](float v) { P().level = v; });
        pan.bind([=] { return P().pan.load(); }, [=](float v) { P().pan = v; });
        sens.bind([=] { return P().velocitySens.load(); }, [=](float v) { P().velocitySens = v; });
    }
    void setPad(int p) { pad = p; refresh(); }
    void refresh() { for (auto* k : {&pitch, &level, &pan, &sens}) k->refresh(); }
    void resized() override {
        Card::resized();
        auto r = content();
        const int w = r.getWidth() / 4;
        for (auto* k : {&pitch, &level, &pan, &sens}) k->setBounds(r.removeFromLeft(w));
    }
private:
    nexus::Engine& engine;
    int pad = 0;
    Knob pitch, level, pan, sens;
};

// ---------------------------------------------------------------- Biblioteca de sons
class LibraryCard : public Card, private juce::ListBoxModel {
public:
    LibraryCard(nexus::Engine& e, std::function<void()> changed) : Card(theme::u8("Biblioteca de sons")), engine(e), changed(std::move(changed)) {
        const auto& bank = engine.sounds();
        std::vector<std::string> cats;
        for (int i = 0; i < bank.size(); ++i) if (std::find(cats.begin(), cats.end(), bank.get(i).category) == cats.end()) cats.push_back(bank.get(i).category);
        for (const auto& c : cats) {
            rows.push_back({true, c, -1});
            for (int i = 0; i < bank.size(); ++i) if (bank.get(i).category == c) rows.push_back({false, bank.get(i).name, i});
        }
        list.setModel(this); list.setRowHeight(30); list.setOutlineThickness(0);
        list.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, theme::edge.brighter(0.5f));
        addAndMakeVisible(list);
    }
    void setPad(int p) { pad = p; refresh(); }
    void refresh() { list.repaint(); }
    void resized() override { Card::resized(); list.setBounds(content().expanded(4, 0)); }

private:
    struct Row { bool header; std::string text; int sound; };
    int getNumRows() override { return static_cast<int>(rows.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool) override {
        using namespace juce;
        if (row < 0 || row >= static_cast<int>(rows.size())) return;
        const auto& r = rows[static_cast<size_t>(row)];
        if (r.header) {
            g.setColour(theme::muted); g.setFont(theme::font(11.5f, true));
            g.drawText(theme::upper(theme::u8(r.text.c_str())), 8, 8, w - 16, h - 8, Justification::centredLeft);
            return;
        }
        const bool cur = engine.params.pad[pad].sound.load() == r.sound;
        auto rc = Rectangle<float>(2.0f, 2.0f, static_cast<float>(w) - 4.0f, static_cast<float>(h) - 4.0f);
        if (cur) {
            g.setColour(theme::accent.withAlpha(0.18f)); g.fillRoundedRectangle(rc, 7.0f);
            g.setColour(theme::accent); g.drawRoundedRectangle(rc, 7.0f, 1.2f);
        }
        Path tri; tri.addTriangle(14.0f, static_cast<float>(h) * 0.5f - 5.0f, 14.0f, static_cast<float>(h) * 0.5f + 5.0f, 23.0f, static_cast<float>(h) * 0.5f);
        g.setColour(cur ? theme::accent : theme::muted); g.fillPath(tri);
        g.setColour(theme::text); g.setFont(theme::font(14.0f, cur));
        g.drawText(theme::u8(r.text.c_str()), 32, 0, w - 44, h, Justification::centredLeft);
        if (cur) { g.setColour(theme::accent); g.fillEllipse(static_cast<float>(w) - 18.0f, static_cast<float>(h) * 0.5f - 3.5f, 7.0f, 7.0f); }
    }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override {
        if (row < 0 || row >= static_cast<int>(rows.size()) || rows[static_cast<size_t>(row)].header) return;
        engine.params.pad[pad].sound = rows[static_cast<size_t>(row)].sound;
        engine.params.previewSound = rows[static_cast<size_t>(row)].sound;
        changed();
    }
    nexus::Engine& engine;
    std::function<void()> changed;
    int pad = 0;
    std::vector<Row> rows;
    juce::ListBox list;
};

// ---------------------------------------------------------------- Nível de entrada
class InputCard : public Card {
public:
    InputCard(nexus::Engine& e, const juce::String& a, const juce::String& b) : Card(theme::u8("Nível de entrada")), engine(e) {
        names[0] = a; names[1] = b;
        for (int i = 0; i < 2; ++i) {
            addAndMakeVisible(meter[i]);
            lbl[i].setFont(theme::font(13.0f, true)); lbl[i].setJustificationType(juce::Justification::centred);
            vel[i].setFont(theme::font(12.0f)); vel[i].setColour(juce::Label::textColourId, theme::muted); vel[i].setJustificationType(juce::Justification::centred);
            lbl[i].setText(names[i], juce::dontSendNotification); vel[i].setText("Vel. -", juce::dontSendNotification);
            addAndMakeVisible(lbl[i]); addAndMakeVisible(vel[i]);
        }
    }
    void tick() {
        for (int i = 0; i < 2; ++i) {
            meter[i].setLevel(engine.status.inputLevel[i].load());
            const unsigned h = engine.status.hitCount[i].load();
            if (h != lastHits[i]) {
                lastHits[i] = h; flash[i] = 1.0f;
                vel[i].setText("Vel. " + juce::String(juce::roundToInt(engine.status.lastVelocity[i].load() * 127.0f)), juce::dontSendNotification);
            }
            if (flash[i] > 0.01f) { flash[i] *= 0.85f; repaint(ledBox[i].expanded(8)); }
        }
    }
    void paint(juce::Graphics& g) override {
        Card::paint(g);
        for (int i = 0; i < 2; ++i) {
            const auto c = ledBox[i].toFloat();
            g.setColour(theme::accent.withAlpha(flash[i] * 0.35f)); g.fillEllipse(c.expanded(5.0f));
            g.setColour(juce::Colour(0xff2a342f).interpolatedWith(theme::hot, flash[i])); g.fillEllipse(c);
        }
    }
    void resized() override {
        Card::resized();
        auto r = content();
        const int w = r.getWidth() / 2;
        for (int i = 0; i < 2; ++i) {
            auto col = r.removeFromLeft(w).reduced(12, 0);
            auto top = col.removeFromTop(20);
            ledBox[i] = juce::Rectangle<int>(top.getRight() - 14, top.getY() + 3, 12, 12);
            lbl[i].setBounds(top.withTrimmedRight(16));
            vel[i].setBounds(col.removeFromBottom(18));
            meter[i].setBounds(col.reduced(juce::jmax(0, (col.getWidth() - 46) / 2), 2));
        }
    }
private:
    nexus::Engine& engine;
    juce::String names[2];
    SegMeter meter[2];
    juce::Label lbl[2], vel[2];
    juce::Rectangle<int> ledBox[2];
    unsigned lastHits[2] = {0, 0};
    float flash[2] = {0.0f, 0.0f};
};

// ---------------------------------------------------------------- Curva do EQ (resposta real dos filtros)
class EqCurve : public juce::Component {
public:
    explicit EqCurve(nexus::EngineParams& p) : params(p) {}
    void paint(juce::Graphics& g) override {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour(theme::panelBot); g.fillRoundedRectangle(r, 8.0f);
        g.setColour(theme::edge); g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
        const auto in = r.reduced(8.0f, 10.0f);
        g.setColour(theme::track); g.drawHorizontalLine(static_cast<int>(in.getCentreY()), in.getX(), in.getRight());
        nexus::Biquad lo, mid, hi;
        const double fs = 48000.0;
        lo.setLowShelf(fs, 120.0, params.eqLowDb.load());
        mid.setPeaking(fs, 1200.0, 0.9, params.eqMidDb.load());
        hi.setHighShelf(fs, 7000.0, params.eqHighDb.load());
        Path p;
        const int n = 120;
        for (int i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / (n - 1);
            const double f = 20.0 * std::pow(1000.0, t);
            const double db = lo.magnitudeDb(fs, f) + mid.magnitudeDb(fs, f) + hi.magnitudeDb(fs, f);
            const float y = in.getCentreY() - static_cast<float>(jlimit(-14.0, 14.0, db)) / 14.0f * (in.getHeight() * 0.5f);
            const float x = in.getX() + static_cast<float>(t) * in.getWidth();
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        Path fill(p); fill.lineTo(in.getRight(), in.getCentreY()); fill.lineTo(in.getX(), in.getCentreY()); fill.closeSubPath();
        g.setColour(theme::accent.withAlpha(0.16f)); g.fillPath(fill);
        g.setColour(theme::accent); g.strokePath(p, PathStrokeType(2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(theme::muted); g.setFont(theme::font(10.5f));
        const char* lab[] = {"20", "1k", "20k"};
        const double fr[] = {20, 1000, 20000};
        for (int i = 0; i < 3; ++i) {
            const float x = in.getX() + static_cast<float>(std::log10(fr[i] / 20.0) / 3.0) * in.getWidth();
            g.drawText(lab[i], static_cast<int>(x) - 16, static_cast<int>(r.getBottom()) - 14, 32, 12, Justification::centred);
        }
    }
private:
    nexus::EngineParams& params;
};
