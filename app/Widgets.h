#pragma once
#include "Theme.h"
#include "core/Engine.h"
#include <BinaryData.h>
#include <cmath>
#include <vector>

// ============================================================================
//  Componentes de tela ligados ao motor de áudio
// ============================================================================

// ---------------------------------------------------------------- Kit visual (foto + pontos clicáveis)
inline nexus::PadParams& pp(nexus::Engine& e, bool zone, int i) { return zone ? e.params.zone[i] : e.params.pad[i]; }

class DrumVisual : public juce::Component {
public:
    static constexpr int kSpots = nexus::kNumPads;

    DrumVisual(nexus::Engine& e, bool hybridMode) : engine(e), hybrid(hybridMode) {
        img = juce::ImageCache::getFromMemory(BinaryData::kit_png, BinaryData::kit_pngSize);
    }
    std::function<void(int)> onSelect;
    int selected = 0;

    int maxPads = nexus::kNumPads;   // 2 na aba Trigger, 8 na aba Módulo
    int numShown() const { return hybrid ? nexus::kNumZones : maxPads; }
    bool isActive(int i) const { return hybrid || i < engine.params.activePads.load(); }

    void tick() {
        bool dirty = false;
        for (int p = 0; p < numShown(); ++p) {
            const unsigned h = engine.status.hitCount[p].load();
            if (h != lastHits[p]) { lastHits[p] = h; flash[p] = 1.0f; }
            if (flash[p] > 0.01f) { flash[p] *= 0.84f; dirty = true; }
        }
        if (dirty) repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (!onSelect) return;
        int best = -1; float bd = 1e9f;
        for (int i = 0; i < numShown(); ++i) {
            const auto c = spotRect(spotOf(i));
            const float dx = (e.position.x - c.getCentreX()) / (c.getWidth() * 0.5f), dy = (e.position.y - c.getCentreY()) / (c.getHeight() * 0.5f);
            const float d = dx * dx + dy * dy;
            if (d < 1.2f && d < bd) { bd = d; best = i; }
        }
        if (best >= 0) onSelect(best);
    }

    void paint(juce::Graphics& g) override {
        using namespace juce;
        const auto r = getLocalBounds().toFloat();
        g.setColour(theme::panelBot); g.fillRoundedRectangle(r, 12.0f);
        Graphics::ScopedSaveState ss(g);
        Path clip; clip.addRoundedRectangle(r, 12.0f); g.reduceClipRegion(clip);
        if (img.isValid()) g.drawImage(img, imageBox(), RectanglePlacement::stretchToFit);
        // véu na cor de destaque (acompanha a cor escolhida na aba Aparência)
        g.setGradientFill(ColourGradient(theme::accent.withAlpha(0.20f), r.getCentreX(), r.getY(), Colours::transparentBlack, r.getCentreX(), r.getBottom(), true));
        g.fillRect(r);
        g.setColour(theme::edge); g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, 1.0f);

        for (int i = 0; i < numShown(); ++i) {
            const auto c = spotRect(spotOf(i));
            const bool act = isActive(i), sel = selected == i;
            if (flash[i] > 0.02f) {
                for (int k = 3; k >= 1; --k) { g.setColour(theme::accent.withAlpha(flash[i] * 0.12f * static_cast<float>(4 - k))); g.fillEllipse(c.expanded(static_cast<float>(k) * 4.0f)); }
                g.setColour(theme::accent.withAlpha(flash[i] * 0.55f)); g.fillEllipse(c);
            }
            if (sel) { g.setColour(theme::accent); g.drawEllipse(c, 2.6f); }
            else if (act) { g.setColour(theme::accent.withAlpha(0.45f)); g.drawEllipse(c, 1.3f); }
            else { g.setColour(Colours::white.withAlpha(0.18f)); g.drawEllipse(c, 1.0f); }
            // etiqueta
            g.setFont(theme::font(11.5f, true));
            const String t = theme::u8(hybrid ? kZoneNames[i] : kNames[i]);
            const float w = g.getCurrentFont().getStringWidthFloat(t) + 14.0f;
            const auto lp = labelPos(spotOf(i));
            Rectangle<float> lb(lp.x - w * 0.5f, lp.y - 9.0f, w, 18.0f);
            g.setColour(Colour(0xcc060807).withMultipliedAlpha(act ? 1.0f : 0.6f)); g.fillRoundedRectangle(lb, 9.0f);
            g.setColour((sel ? theme::accent : theme::edge.brighter(0.3f)).withMultipliedAlpha(act ? 1.0f : 0.5f)); g.drawRoundedRectangle(lb, 9.0f, 1.0f);
            g.setColour(act ? theme::text : theme::muted); g.drawText(t, lb, Justification::centred);
        }
        if (hybrid) {
            g.setColour(theme::muted); g.setFont(theme::font(12.0f, true));
            g.drawText(theme::u8("MIC 1 (E)"), 12, getHeight() - 24, 120, 18, Justification::centredLeft);
            g.drawText(theme::u8("MIC 2 (D)"), getWidth() - 132, getHeight() - 24, 120, 18, Justification::centredRight);
        }
    }

    static constexpr const char* kNames[8] = {"BUMBO", "CAIXA", "CHIMBAL", "TOM 1", "TOM 2", "SURDO", "PRATO 1", "PRATO 2"};
    static constexpr const char* kZoneNames[6] = {"BUMBO", "CAIXA", "CHIMBAL", "TOM 1", "TOM 2", "PRATO"};

private:
    int spotOf(int i) const { static const int m[6] = {0, 1, 2, 3, 4, 6}; return hybrid ? m[i] : i; }
    // a foto preenche o quadro (corta o excesso) e os círculos usam a MESMA conta, para cair em cima das peças
    juce::Rectangle<float> imageBox() const {
        const auto r = getLocalBounds().toFloat();
        if (!img.isValid()) return r;
        const float iw = static_cast<float>(img.getWidth()), ih = static_cast<float>(img.getHeight());
        const float sc = juce::jmax(r.getWidth() / iw, r.getHeight() / ih);
        const float w = iw * sc, h = ih * sc;
        return {r.getCentreX() - w * 0.5f, r.getCentreY() - h * 0.5f, w, h};
    }
    // posição de cada peça em pixels da foto kit.png (654 x 305): centro, raios e ponto da etiqueta
    struct PixSpot { float cx, cy, rx, ry, lx, ly; };
    static const PixSpot& pix(int i) {
        static const PixSpot p[kSpots] = {
            {352.0f, 330.0f, 138.0f, 138.0f, 352.0f, 292.0f}, // bumbo (pele da frente)
            {155.0f, 236.0f,  52.0f,  64.0f, 155.0f, 236.0f}, // caixa
            {577.0f, 143.0f,  70.0f,  14.0f, 577.0f, 162.0f}, // chimbal
            {270.0f, 142.0f,  66.0f,  58.0f, 270.0f, 142.0f}, // tom 1
            {408.0f, 145.0f,  61.0f,  54.0f, 408.0f, 145.0f}, // tom 2
            {490.0f, 212.0f,  50.0f,  33.0f, 490.0f, 212.0f}, // surdo
            {136.0f,  55.0f, 102.0f,  28.0f, 136.0f,  55.0f}, // prato 1
            {502.0f,  56.0f,  87.0f,  22.0f, 502.0f,  56.0f}};// prato 2
        return p[i];
    }
    juce::Rectangle<float> spotRect(int i) const {
        const auto b = imageBox();
        const float sc = img.isValid() ? b.getWidth() / static_cast<float>(img.getWidth()) : 1.0f;
        const auto& s = pix(i);
        return {b.getX() + (s.cx - s.rx) * sc, b.getY() + (s.cy - s.ry) * sc, s.rx * 2.0f * sc, s.ry * 2.0f * sc};
    }
    juce::Point<float> labelPos(int i) const {
        const auto b = imageBox();
        const float sc = img.isValid() ? b.getWidth() / static_cast<float>(img.getWidth()) : 1.0f;
        const auto& s = pix(i);
        auto pt = juce::Point<float>(b.getX() + s.lx * sc, b.getY() + s.ly * sc);
        return {pt.x, juce::jlimit(12.0f, static_cast<float>(getHeight()) - 30.0f, pt.y)};
    }

    nexus::Engine& engine;
    bool hybrid;
    juce::Image img;
    unsigned lastHits[nexus::kNumPads] = {};
    float flash[nexus::kNumPads] = {};
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
    int maxInputs = nexus::kMaxInputs;
    int maxOutputs = nexus::kMaxOutputs;
    bool zone = false;   // true = editar uma zona do EAD em vez de um pad
    DetectionCard(nexus::Engine& e, std::function<void()> changed)
        : Card("Detecção (Trigger)"), engine(e),
          sens("Sensibilidade (ganho do mic)", -12, 36, 0.5, " dB", 1),
          thr("Limiar", -60, -6, 0.5, " dB", 1),
          retrig("Retrigger", 20, 300, 1, " ms", 0),
          hold("Hold (leitura do pico)", 1, 20, 0.5, " ms", 1, 2.5),
          hp("Filtro grave (HP)", 20, 8000, 1, " Hz", 0, 150),
          lp("Filtro agudo (LP)", 100, 16000, 10, " Hz", 0, 3000) {
        for (auto* s : {&sens, &thr, &retrig, &hold, &hp, &lp}) { addAndMakeVisible(*s); s->afterChange = changed; }
        for (int i = 1; i <= nexus::kMaxInputs; ++i) inSel.addItem(theme::u8("Entrada ") + juce::String(i), i);
        inSel.onChange = [this, changed] { if (!zone && inSel.getSelectedId() > 0) { pp(engine, zone, pad).inputChannel = inSel.getSelectedId() - 1; changed(); } };
        addAndMakeVisible(inSel); addAndMakeVisible(inLbl);
        outSel.onChange = [this, changed] { if (!zone && outSel.getSelectedId() > 0) { pp(engine, zone, pad).outRoute = outSel.getSelectedId() - 1; changed(); } };
        addAndMakeVisible(outSel); addAndMakeVisible(outLbl);
        outLbl.setText(theme::u8("Sai em"), juce::dontSendNotification);
        outLbl.setFont(theme::font(13.0f, true)); outLbl.setColour(juce::Label::textColourId, theme::muted);
        fillOut();
        inLbl.setText(theme::u8("Ouve a"), juce::dontSendNotification);
        inLbl.setFont(theme::font(13.0f, true)); inLbl.setColour(juce::Label::textColourId, theme::muted);
        rebind();
    }
    void setPad(int p) {
        pad = p;
        static const char* names[] = {"Bumbo", "Caixa", "Chimbal", "Tom 1", "Tom 2", "Surdo", "Prato 1", "Prato 2"};
        static const char* zn[] = {"Bumbo", "Caixa", "Chimbal", "Tom 1", "Tom 2", "Prato"};
        setTitle(theme::u8("Detecção · ") + theme::u8(zone ? zn[p] : names[p]));
        refresh();
    }
    void fillOut() {
        outSel.clear(juce::dontSendNotification);
        outSel.addItem(theme::u8("Estéreo (mix L+R)"), 1);
        for (int i = 1; i <= maxOutputs; ++i)
            outSel.addItem(theme::u8("Saída ") + juce::String(i) + (maxOutputs == 2 ? (i == 1 ? " (somente L)" : " (somente R)") : (i == 1 ? " (L)" : i == 2 ? " (R)" : "")), i + 1);
    }
    void refresh() {
        outSel.setVisible(!zone); outLbl.setVisible(!zone);
        if (outSel.getNumItems() != maxOutputs + 1) fillOut();
        if (!zone) outSel.setSelectedId(juce::jmin(maxOutputs, pp(engine, zone, pad).outRoute.load()) + 1, juce::dontSendNotification);
        for (auto* s : {&sens, &thr, &retrig, &hold, &hp, &lp}) s->refresh();
        hp.setVisible(!zone); lp.setVisible(!zone); hold.setVisible(!zone);
        inSel.setVisible(!zone); inLbl.setVisible(!zone);
        if (inSel.getNumItems() != nexus::kMaxInputs) {}
        if (maxInputs < nexus::kMaxInputs && inSel.getNumItems() > maxInputs) { inSel.clear(juce::dontSendNotification); for (int i = 1; i <= maxInputs; ++i) inSel.addItem(theme::u8("Entrada ") + juce::String(i), i); }
        if (!zone) inSel.setSelectedId(pp(engine, zone, pad).inputChannel.load() + 1, juce::dontSendNotification);
        resized();
    }
    void resized() override {
        Card::resized();
        auto r = content();
        if (!zone) {
            auto row = r.removeFromTop(32);
            inLbl.setBounds(row.removeFromLeft(70));
            inSel.setBounds(row.reduced(0, 2));
            auto row2 = r.removeFromTop(32);
            outLbl.setBounds(row2.removeFromLeft(70));
            outSel.setBounds(row2.reduced(0, 2));
            r.removeFromTop(2);
        }
        const int h = juce::jlimit(34, 48, r.getHeight() / 5);
        if (zone) { const int h3 = juce::jlimit(34, 56, r.getHeight() / 3); for (auto* s : {&sens, &thr, &retrig}) s->setBounds(r.removeFromTop(h3)); }
        else { const int h6 = juce::jlimit(32, 46, r.getHeight() / 6); for (auto* s : {&sens, &thr, &retrig, &hold, &hp, &lp}) s->setBounds(r.removeFromTop(h6)); }
    }
private:
    void rebind() {
        auto P = [this]() -> nexus::PadParams& { return pp(engine, zone, pad); };
        sens.bind([=] { return P().inputGainDb.load(); }, [=](float v) { P().inputGainDb = v; });
        thr.bind([=] { return P().thresholdDb.load(); }, [=](float v) { P().thresholdDb = v; });
        retrig.bind([=] { return P().retriggerMs.load(); }, [=](float v) { P().retriggerMs = v; });
        hold.bind([=] { return P().holdMs.load(); }, [=](float v) { P().holdMs = v; });
        hp.bind([=] { return P().hpHz.load(); }, [=](float v) { P().hpHz = v; });
        lp.bind([=] { return P().lpHz.load(); }, [=](float v) { P().lpHz = v; });
    }
    nexus::Engine& engine;
    int pad = 0;
    BarSlider sens, thr, retrig, hold, hp, lp;
    juce::ComboBox inSel, outSel;
    juce::Label inLbl, outLbl;
};

// ---------------------------------------------------------------- Som do pad (nome + onda + play)
class SoundPanel : public Card {
public:
    bool zone = false;   // true = editar uma zona do EAD em vez de um pad
    SoundPanel(nexus::Engine& e, std::function<void()> changed) : Card("Som do pad"), engine(e), changed(std::move(changed)) {
        for (auto* b : {&prev, &next}) { addAndMakeVisible(*b); }
        prev.setButtonText(theme::u8("‹")); next.setButtonText(theme::u8("›"));
        prev.onClick = [this] { step(-1); };
        next.onClick = [this] { step(+1); };
        play.setButtonText(theme::u8("▶  Ouvir"));
        play.onClick = [this] { engine.params.previewSound = pp(engine, zone, pad).sound.load(); };
        addAndMakeVisible(play); addAndMakeVisible(wave);
        for (auto* l : {&name, &cat}) addAndMakeVisible(*l);
        name.setFont(theme::font(20.0f, true)); name.setJustificationType(juce::Justification::centred);
        cat.setFont(theme::font(13.0f)); cat.setColour(juce::Label::textColourId, theme::accent); cat.setJustificationType(juce::Justification::centred);
    }
    void setPad(int p) { pad = p; setTitle(theme::u8(zone ? "Som do gatilho" : "Som do pad")); refresh(); }
    void refresh() {
        const auto& s = engine.sounds().get(pp(engine, zone, pad).sound.load());
        name.setText(theme::u8(s.name.c_str()), juce::dontSendNotification);
        cat.setText(theme::upper(theme::u8(s.category.c_str())), juce::dontSendNotification);
        if (shownSound != pp(engine, zone, pad).sound.load()) { shownSound = pp(engine, zone, pad).sound.load(); wave.setData(s.data); }
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
        pp(engine, zone, pad).sound = (pp(engine, zone, pad).sound.load() + d + n) % n;
        engine.params.previewSound = pp(engine, zone, pad).sound.load();
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
    bool zone = false;   // true = editar uma zona do EAD em vez de um pad
    TuneCard(nexus::Engine& e, std::function<void()> changed)
        : Card(theme::u8("Ajuste do som")), engine(e),
          pitch(theme::u8("Afinação"), -12, 12, 0.1, " st", 0.0, 1),
          level("Volume", 0, 1.5, 0.01, "", 1.0, 2),
          pan("Pan", -1, 1, 0.01, "", 0.0, 2),
          sens(theme::u8("Dinâmica"), 0, 1, 0.01, "", 1.0, 2) {
        for (auto* k : {&pitch, &level, &pan, &sens}) { addAndMakeVisible(*k); k->afterChange = changed; }
        auto P = [this]() -> nexus::PadParams& { return pp(engine, zone, pad); };
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
    bool zone = false;   // true = editar uma zona do EAD em vez de um pad
    LibraryCard(nexus::Engine& e, std::function<void()> changed) : Card(theme::u8("Biblioteca de sons")), engine(e), changed(std::move(changed)) {
        importBtn.setButtonText(theme::u8("+  Importar sons (WAV)"));
        importBtn.onClick = [this] { if (onImport) onImport(); };
        addAndMakeVisible(importBtn);
        rebuild();
        list.setModel(this); list.setRowHeight(30); list.setOutlineThickness(0);
        list.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, theme::edge.brighter(0.5f));
        addAndMakeVisible(list);
    }
    void setPad(int p) { pad = p; refresh(); }
    std::function<void()> onImport;
    void refresh() { if (builtCount != engine.sounds().size()) { rebuild(); list.updateContent(); } list.repaint(); }
    void resized() override {
        Card::resized();
        auto r = content().expanded(4, 0);
        importBtn.setBounds(r.removeFromBottom(34).reduced(4, 0)); r.removeFromBottom(4);
        list.setBounds(r);
    }

private:
    struct Row { bool header; std::string text; int sound; };
    void rebuild() {
        const auto& bank = engine.sounds();
        builtCount = bank.size();
        rows.clear();
        std::vector<std::string> cats;
        for (int i = 0; i < bank.size(); ++i) if (std::find(cats.begin(), cats.end(), bank.get(i).category) == cats.end()) cats.push_back(bank.get(i).category);
        for (const auto& c : cats) {
            rows.push_back({true, c, -1});
            for (int i = 0; i < bank.size(); ++i) if (bank.get(i).category == c) rows.push_back({false, bank.get(i).name, i});
        }
    }
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
        const bool cur = pp(engine, zone, pad).sound.load() == r.sound;
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
        pp(engine, zone, pad).sound = rows[static_cast<size_t>(row)].sound;
        engine.params.previewSound = rows[static_cast<size_t>(row)].sound;
        changed();
    }
    nexus::Engine& engine;
    std::function<void()> changed;
    int pad = 0;
    std::vector<Row> rows;
    int builtCount = 0;
    juce::ListBox list;
    juce::TextButton importBtn;
};

// ---------------------------------------------------------------- Nível de entrada (até 8 entradas)
class InputCard : public Card {
public:
    explicit InputCard(nexus::Engine& e) : Card(theme::u8("Nível de entrada")), engine(e) {
        for (int i = 0; i < nexus::kMaxInputs; ++i) {
            addChildComponent(meter[i]);
            lbl[i].setFont(theme::font(12.5f, true)); lbl[i].setJustificationType(juce::Justification::centred);
            addChildComponent(lbl[i]);
        }
        setCount(2);
    }
    void setCount(int n, bool hybridMode = false) {
        n = juce::jlimit(1, nexus::kMaxInputs, n);
        if (n == count && hybridMode == hybrid) return;
        count = n; hybrid = hybridMode;
        for (int i = 0; i < nexus::kMaxInputs; ++i) {
            meter[i].setVisible(i < count); lbl[i].setVisible(i < count);
            lbl[i].setText(hybrid ? (i == 0 ? theme::u8("E") : theme::u8("D")) : juce::String(i + 1), juce::dontSendNotification);
        }
        resized(); repaint();
    }
    void tick() {
        for (int i = 0; i < count; ++i) {
            meter[i].setLevel(engine.status.inputLevel[i].load());
            bool hit = false;
            if (hybrid) {
                for (int z = 0; z < nexus::kNumZones; ++z) { const unsigned h = engine.status.hitCount[z].load(); if (h != lastHits[z]) hit = true; }
            } else {
                for (int p = 0; p < engine.params.activePads.load(); ++p)
                    if (engine.params.pad[p].inputChannel.load() == i && engine.status.hitCount[p].load() != lastHits[p]) hit = true;
            }
            if (hit) flash[i] = 1.0f;
            if (flash[i] > 0.01f) { flash[i] *= 0.85f; repaint(ledBox[i].expanded(8)); }
        }
        for (int p = 0; p < nexus::kNumPads; ++p) lastHits[p] = engine.status.hitCount[p].load();
    }
    void paint(juce::Graphics& g) override {
        Card::paint(g);
        for (int i = 0; i < count; ++i) {
            const auto c = ledBox[i].toFloat();
            g.setColour(theme::accent.withAlpha(flash[i] * 0.35f)); g.fillEllipse(c.expanded(4.0f));
            g.setColour(juce::Colour(0xff2a342f).interpolatedWith(theme::hot, flash[i])); g.fillEllipse(c);
        }
    }
    void resized() override {
        Card::resized();
        auto r = content();
        const int w = r.getWidth() / count;
        for (int i = 0; i < count; ++i) {
            auto col = r.removeFromLeft(w).reduced(count > 4 ? 2 : 10, 0);
            ledBox[i] = juce::Rectangle<int>(col.getCentreX() - 5, col.getY() + 2, 10, 10);
            col.removeFromTop(16);
            lbl[i].setBounds(col.removeFromBottom(18));
            meter[i].setBounds(col.reduced(juce::jmax(0, (col.getWidth() - 40) / 2), 2));
        }
    }
private:
    nexus::Engine& engine;
    int count = 0; bool hybrid = false;
    SegMeter meter[nexus::kMaxInputs];
    juce::Label lbl[nexus::kMaxInputs];
    juce::Rectangle<int> ledBox[nexus::kMaxInputs];
    unsigned lastHits[nexus::kNumPads] = {};
    float flash[nexus::kMaxInputs] = {};
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
