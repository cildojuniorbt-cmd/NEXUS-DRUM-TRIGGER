#include "MainComponent.h"
#include "Preset.h"
#include "Widgets.h"

using namespace juce;

// Base das páginas (Trigger, EAD, Áudio, Aparência)
struct PageBase : public Component {
    virtual void refresh() = 0;
    virtual void tick() {}
    virtual void setInputs(int) {}
};

namespace {

// Cartão com knobs distribuídos (e, opcionalmente, um componente à esquerda).
class KnobCard : public Card {
public:
    explicit KnobCard(const String& t, bool power = false) : Card(t, power) {
        info.setFont(theme::font(13.0f, true));
        info.setColour(Label::textColourId, theme::muted);
        info.setJustificationType(Justification::centredRight);
        addAndMakeVisible(info);
    }
    Knob& add(const String& name, double mn, double mx, double step, const String& suffix, double def, int dec,
              std::function<float()> g, std::function<void(float)> s, std::function<void()> changed, double skew = 0.0) {
        auto k = std::make_unique<Knob>(name, mn, mx, step, suffix, def, dec, skew);
        k->bind(std::move(g), std::move(s));
        k->afterChange = std::move(changed);
        addAndMakeVisible(*k);
        knobs.push_back(std::move(k));
        return *knobs.back();
    }
    void setLeading(Component* c) { lead = c; addAndMakeVisible(c); }
    void refresh() { for (auto& k : knobs) k->refresh(); }
    void resized() override {
        Card::resized();
        info.setBounds(getWidth() - 140, 9, 126, 26);
        auto r = content();
        const int n = static_cast<int>(knobs.size());
        if (n == 0) return;
        if (lead) {
            const int kw = jlimit(52, 76, (r.getWidth() - 110) / n);
            const int total = kw * n;
            lead->setBounds(r.removeFromLeft(r.getWidth() - total - 10));
            r.removeFromLeft(10);
            for (auto& k : knobs) k->setBounds(r.removeFromLeft(kw));
        } else {
            const int kw = jmin(104, r.getWidth() / n);
            r = r.withSizeKeepingCentre(kw * n, r.getHeight());
            for (auto& k : knobs) k->setBounds(r.removeFromLeft(kw));
        }
    }
    Label info;
private:
    std::vector<std::unique_ptr<Knob>> knobs;
    Component* lead = nullptr;
};

struct FactoryKit { const char* name; int s0, s1; };
const FactoryKit kKits[] = {{"Rock Moderno", 2, 4}, {"Eletrônico 808", 0, 3}, {"Seco / Funk", 1, 5}, {"Percussão", 12, 7}, {"Chimbal Pop", 1, 8}};
constexpr int kNumKits = 5;

} // namespace

// ============================================================================
//  Botão do menu lateral
// ============================================================================
class MainComponent::NavButton : public Button {
public:
    NavButton(const String& text, int iconIdx) : Button(text), icon(iconIdx) {
        setClickingTogglesState(false);
    }
    void paintButton(Graphics& g, bool hl, bool) override {
        auto r = getLocalBounds().toFloat().reduced(8.0f, 3.0f);
        const bool on = getToggleState();
        if (on) {
            g.setColour(theme::accent.withAlpha(0.22f)); g.fillRoundedRectangle(r.expanded(2.0f), 12.0f);
            g.setColour(theme::accent); g.fillRoundedRectangle(r, 10.0f);
        } else if (hl) {
            g.setColour(Colours::white.withAlpha(0.06f)); g.fillRoundedRectangle(r, 10.0f);
        }
        const Colour ic = on ? Colours::black : (hl ? theme::text : theme::muted);
        const auto b = Rectangle<float>(r.getX() + 14.0f, r.getCentreY() - 11.0f, 22.0f, 22.0f);
        g.setColour(ic);
        const auto c = b.getCentre();
        switch (icon) {
            case 0: // alvo
                g.drawEllipse(b.reduced(3.0f), 2.0f);
                g.fillEllipse(c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f);
                g.drawLine(c.x, b.getY(), c.x, b.getY() + 4.0f, 2.0f); g.drawLine(c.x, b.getBottom() - 4.0f, c.x, b.getBottom(), 2.0f);
                g.drawLine(b.getX(), c.y, b.getX() + 4.0f, c.y, 2.0f); g.drawLine(b.getRight() - 4.0f, c.y, b.getRight(), c.y, 2.0f);
                break;
            case 1: { // ondas
                const float hs[] = {0.35f, 0.7f, 1.0f, 0.55f, 0.8f};
                for (int i = 0; i < 5; ++i) {
                    const float x = b.getX() + 2.0f + static_cast<float>(i) * 4.5f, hh = hs[i] * b.getHeight();
                    g.fillRoundedRectangle(x, c.y - hh * 0.5f, 2.6f, hh, 1.3f);
                }
                break; }
            case 2: { // alto-falante
                Path p; p.startNewSubPath(b.getX() + 1.0f, c.y - 4.0f); p.lineTo(b.getX() + 6.0f, c.y - 4.0f); p.lineTo(b.getX() + 12.0f, c.y - 9.0f);
                p.lineTo(b.getX() + 12.0f, c.y + 9.0f); p.lineTo(b.getX() + 6.0f, c.y + 4.0f); p.lineTo(b.getX() + 1.0f, c.y + 4.0f); p.closeSubPath();
                g.fillPath(p);
                Path a1; a1.addCentredArc(c.x + 3.0f, c.y, 6.0f, 6.0f, 0.0f, 0.6f, 2.5f, true);
                g.strokePath(a1, PathStrokeType(2.0f, PathStrokeType::curved, PathStrokeType::rounded));
                break; }
            case 4: { // grade (módulo)
                for (int a = 0; a < 3; ++a) for (int bb = 0; bb < 3; ++bb) g.fillRoundedRectangle(b.getX() + 1.0f + static_cast<float>(a) * 7.5f, b.getY() + 1.0f + static_cast<float>(bb) * 7.5f, 5.5f, 5.5f, 1.5f);
                break; }
            default: { // paleta
                g.drawEllipse(b.reduced(2.0f), 2.0f);
                g.fillEllipse(c.x - 5.5f, c.y - 4.0f, 3.6f, 3.6f); g.fillEllipse(c.x + 1.5f, c.y - 5.0f, 3.6f, 3.6f); g.fillEllipse(c.x - 1.5f, c.y + 2.0f, 3.6f, 3.6f);
                break; }
        }
        g.setColour(on ? Colours::black : theme::text);
        g.setFont(theme::font(16.0f, true));
        g.drawText(getButtonText(), static_cast<int>(r.getX()) + 50, static_cast<int>(r.getY()), static_cast<int>(r.getWidth()) - 52, static_cast<int>(r.getHeight()), Justification::centredLeft);
    }
private:
    int icon;
};

// ============================================================================
//  Página: Trigger (módulo de bateria)
// ============================================================================
class MainComponent::TriggerPage : public PageBase {
public:
    void setImportHandler(std::function<void()> f) { lib.onImport = std::move(f); }
    TriggerPage(nexus::Engine& e, std::function<void()> changed, int maxPadsIn)
        : engine(e), maxPads(maxPadsIn), visual(e, false), det(e, changed), sound(e, changed), tune(e, changed), lib(e, changed),
          input(e), detector(theme::u8("Vazamento")), reverb(theme::u8("Ambiência (Reverb)")), out(theme::u8("Saída")) {
        auto& p = e.params;
        detector.add(theme::u8("Rejeição"), 0, 1, 0.01, "", 0.0, 2, [&p] { return p.bleedReject.load(); }, [&p](float v) { p.bleedReject = v; }, changed);
        reverb.add("Sala", 0, 1, 0.01, "", 0.5, 2, [&p] { return p.reverbRoom.load(); }, [&p](float v) { p.reverbRoom = v; }, changed);
        reverb.add("Abafamento", 0, 1, 0.01, "", 0.5, 2, [&p] { return p.reverbDamp.load(); }, [&p](float v) { p.reverbDamp = v; }, changed);
        reverb.add("Quantidade", 0, 1, 0.01, "", 0.0, 2, [&p] { return p.reverbWet.load(); }, [&p](float v) { p.reverbWet = v; }, changed);
        out.add("Volume geral", 0, 1.2, 0.01, "", 0.9, 2, [&p] { return p.master.load(); }, [&p](float v) { p.master = v; }, changed);

        for (auto* c : std::initializer_list<Component*>{&visual, &det, &sound, &tune, &lib, &input, &detector, &reverb, &out, &padsLbl, &padsMinus, &padsPlus, &testBtn}) addAndMakeVisible(c);
        visual.maxPads = maxPads;
        const bool few = maxPads <= 2;
        padsLbl.setVisible(!few); padsMinus.setVisible(!few); padsPlus.setVisible(!few);
        det.maxInputs = maxPads;
        for (int i = 0; i < nexus::kNumPads; ++i) {
            padBtn[i].setVisible(i < maxPads);
            padBtn[i].setClickingTogglesState(true);
            padBtn[i].setRadioGroupId(101);
            padBtn[i].setButtonText(theme::u8(i == 2 ? "CHIMB." : DrumVisual::kNames[i]));
            padBtn[i].onClick = [this, i] { select(i); };
            addAndMakeVisible(padBtn[i]);
        }
        padsLbl.setFont(theme::font(12.5f, true)); padsLbl.setColour(Label::textColourId, theme::muted); padsLbl.setJustificationType(Justification::centred);
        padsMinus.setButtonText("-"); padsPlus.setButtonText("+");
        padsMinus.onClick = [this, changed] { engine.params.activePads = jmax(1, engine.params.activePads.load() - 1); changed(); };
        padsPlus.onClick = [this, changed] { engine.params.activePads = jmin(maxPads, engine.params.activePads.load() + 1); changed(); };
        testBtn.setButtonText(theme::u8("▶ Testar"));
        testBtn.onClick = [this] { engine.params.manualHit[selected] = 900; };
        visual.onSelect = [this, changed](int i) { if (i >= engine.params.activePads.load()) { engine.params.activePads = i + 1; changed(); } select(i); };
        select(0);
    }
    void select(int i) {
        selected = i;
        visual.selected = i; det.setPad(i); sound.setPad(i); tune.setPad(i); lib.setPad(i);
        refresh(); visual.repaint();
    }
    void refresh() override {
        det.refresh(); sound.refresh(); tune.refresh(); lib.refresh();
        detector.refresh(); reverb.refresh(); out.refresh();
        const int act = engine.params.activePads.load();
        for (int i = 0; i < maxPads; ++i) {
            padBtn[i].setToggleState(selected == i, dontSendNotification);
            padBtn[i].setAlpha(i < jmax(act, maxPads <= 2 ? 2 : 0) ? 1.0f : 0.4f);
        }
        padsLbl.setText(theme::u8("Pads: ") + String(act), dontSendNotification);
        visual.repaint();
    }
    void setInputs(int n) override { input.setCount(maxPads <= 2 ? jmin(n, 2) : n, false); }
    void tick() override { visual.tick(); input.tick(); }
    void resized() override {
        auto r = getLocalBounds();
        auto right = r.removeFromRight(300); r.removeFromRight(10);
        input.setBounds(right.removeFromTop(150)); right.removeFromTop(10);
        det.setBounds(right.removeFromTop(310)); right.removeFromTop(10);
        lib.setBounds(right);
        auto D = r.removeFromBottom(150); r.removeFromBottom(10);
        auto C = r.removeFromBottom(150); r.removeFromBottom(10);
        auto B = r.removeFromBottom(46); r.removeFromBottom(10);
        visual.setBounds(r);
        testBtn.setBounds(B.removeFromRight(92)); B.removeFromRight(8);
        if (maxPads > 2) { padsPlus.setBounds(B.removeFromRight(36)); padsLbl.setBounds(B.removeFromRight(70)); padsMinus.setBounds(B.removeFromRight(36)); B.removeFromRight(10); }
        if (maxPads <= 2) { const int w2 = 150; for (int i = 0; i < maxPads; ++i) padBtn[i].setBounds(B.removeFromLeft(w2).reduced(2, 0)); }
        else { const int w = B.getWidth() / maxPads; for (int i = 0; i < maxPads; ++i) padBtn[i].setBounds(B.removeFromLeft(w).reduced(2, 0)); }
        tune.setBounds(C.removeFromRight(340)); C.removeFromRight(10); sound.setBounds(C);
        out.setBounds(D.removeFromRight(200)); D.removeFromRight(10); detector.setBounds(D.removeFromRight(200)); D.removeFromRight(10); reverb.setBounds(D);
    }
private:
    nexus::Engine& engine;
    int maxPads;
    int selected = 0;
    DrumVisual visual;
    DetectionCard det; SoundPanel sound; TuneCard tune; LibraryCard lib; InputCard input;
    KnobCard detector, reverb, out;
    TextButton padBtn[nexus::kNumPads], padsMinus, padsPlus, testBtn;
    Label padsLbl;
};

// ============================================================================
//  Página: EAD (acústico + eletrônico, microfones em par estéreo)
// ============================================================================
class MainComponent::EadPage : public PageBase {
public:
    void setImportHandler(std::function<void()> f) { lib.onImport = std::move(f); }
    EadPage(nexus::Engine& e, std::function<void()> changed)
        : engine(e), visual(e, true), det(e, changed), sound(e, changed), tune(e, changed), lib(e, changed),
          input(e), eq(theme::u8("Equalizador")), comp("Compressor", true), reverb(theme::u8("Ambiência (Reverb)")), mix(theme::u8("Mistura")),
          curve(e.params) {
        auto& p = e.params;
        auto eqChanged = [this, changed] { curve.repaint(); changed(); };
        eq.setLeading(&curve);
        eq.add("Grave", -12, 12, 0.5, " dB", 0.0, 1, [&p] { return p.eqLowDb.load(); }, [&p](float v) { p.eqLowDb = v; }, eqChanged);
        eq.add(theme::u8("Médio"), -12, 12, 0.5, " dB", 0.0, 1, [&p] { return p.eqMidDb.load(); }, [&p](float v) { p.eqMidDb = v; }, eqChanged);
        eq.add("Agudo", -12, 12, 0.5, " dB", 0.0, 1, [&p] { return p.eqHighDb.load(); }, [&p](float v) { p.eqHighDb = v; }, eqChanged);

        comp.add("Limiar", -48, 0, 0.5, " dB", -18.0, 1, [&p] { return p.compThresholdDb.load(); }, [&p](float v) { p.compThresholdDb = v; }, changed);
        comp.add("Taxa", 1, 20, 0.1, ":1", 3.0, 1, [&p] { return p.compRatio.load(); }, [&p](float v) { p.compRatio = v; }, changed);
        comp.add("Ataque", 1, 100, 1, " ms", 8.0, 0, [&p] { return p.compAttackMs.load(); }, [&p](float v) { p.compAttackMs = v; }, changed);
        comp.add(theme::u8("Liberação"), 20, 800, 1, " ms", 120.0, 0, [&p] { return p.compReleaseMs.load(); }, [&p](float v) { p.compReleaseMs = v; }, changed);
        comp.add("Ganho", 0, 18, 0.5, " dB", 0.0, 1, [&p] { return p.compMakeupDb.load(); }, [&p](float v) { p.compMakeupDb = v; }, changed);
        comp.power->onClick = [this, changed] { engine.params.compOn = comp.power->getToggleState(); changed(); };

        reverb.add("Sala", 0, 1, 0.01, "", 0.5, 2, [&p] { return p.reverbRoom.load(); }, [&p](float v) { p.reverbRoom = v; }, changed);
        reverb.add("Abafamento", 0, 1, 0.01, "", 0.5, 2, [&p] { return p.reverbDamp.load(); }, [&p](float v) { p.reverbDamp = v; }, changed);
        reverb.add("Quantidade", 0, 1, 0.01, "", 0.0, 2, [&p] { return p.reverbWet.load(); }, [&p](float v) { p.reverbWet = v; }, changed);

        mix.add(theme::u8("Separação"), 0, 1, 0.01, "", 0.0, 2, [&p] { return p.bleedReject.load(); }, [&p](float v) { p.bleedReject = v; }, changed);
        mix.add(theme::u8("Mics estéreo"), 0, 1.5, 0.01, "", 0.8, 2, [&p] { return p.acousticLevel.load(); }, [&p](float v) { p.acousticLevel = v; }, changed);
        mix.add("Largura", 0, 1.5, 0.01, "", 1.0, 2, [&p] { return p.stereoWidth.load(); }, [&p](float v) { p.stereoWidth = v; }, changed);
        mix.add(theme::u8("Eletrônico"), 0, 1.5, 0.01, "", 1.0, 2, [&p] { return p.electronicLevel.load(); }, [&p](float v) { p.electronicLevel = v; }, changed);
        mix.add("Volume geral", 0, 1.2, 0.01, "", 0.9, 2, [&p] { return p.master.load(); }, [&p](float v) { p.master = v; }, changed);

        for (auto* c : std::initializer_list<Component*>{&visual, &det, &sound, &tune, &lib, &input, &eq, &comp, &reverb, &mix, &testBtn}) addAndMakeVisible(c);
        det.zone = sound.zone = tune.zone = lib.zone = true;
        for (int i = 0; i < nexus::kNumZones; ++i) {
            zoneBtn[i].setClickingTogglesState(true); zoneBtn[i].setRadioGroupId(202);
            zoneBtn[i].setButtonText(theme::u8(i == 0 ? "BUMBO · graves" : i == 1 ? "CAIXA · médios" : "CHIMBAL · agudos"));
            zoneBtn[i].onClick = [this, i] { select(i); };
            addAndMakeVisible(zoneBtn[i]);
        }
        testBtn.setButtonText(theme::u8("▶ Testar"));
        testBtn.onClick = [this] { engine.params.manualZone[selected] = 900; };
        visual.onSelect = [this](int i) { select(i); };
        warn.setText(theme::u8("2 mics = par estéreo; o app separa bumbo, caixa e chimbal por frequência."), dontSendNotification);
        warn.setFont(theme::font(11.5f)); warn.setColour(Label::textColourId, theme::warn); warn.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(warn);
        select(0);
    }
    void select(int i) {
        selected = i;
        visual.selected = i; det.setPad(i); sound.setPad(i); tune.setPad(i); lib.setPad(i);
        refresh();
    }
    void setInputs(int n) override { input.setCount(n, true); }
    void refresh() override {
        det.refresh(); sound.refresh(); tune.refresh(); lib.refresh();
        eq.refresh(); comp.refresh(); reverb.refresh(); mix.refresh(); curve.repaint();
        comp.power->setToggleState(engine.params.compOn.load(), dontSendNotification);
        for (int i = 0; i < nexus::kNumZones; ++i) zoneBtn[i].setToggleState(selected == i, dontSendNotification);
        visual.repaint();
    }
    void tick() override {
        visual.tick(); input.tick();
        comp.info.setText(theme::u8("Redução ") + String(engine.status.gainReductionDb.load(), 1) + " dB", dontSendNotification);
    }
    void resized() override {
        auto r = getLocalBounds();
        auto right = r.removeFromRight(300); r.removeFromRight(10);
        input.setBounds(right.removeFromTop(150)); right.removeFromTop(10);
        det.setBounds(right.removeFromTop(310)); right.removeFromTop(10);
        lib.setBounds(right);
        auto D = r.removeFromBottom(140); r.removeFromBottom(8);
        auto C = r.removeFromBottom(150); r.removeFromBottom(8);
        auto B = r.removeFromBottom(140); r.removeFromBottom(8);
        auto Z = r.removeFromBottom(44); r.removeFromBottom(8);
        visual.setBounds(r);
        testBtn.setBounds(Z.removeFromRight(92)); Z.removeFromRight(8);
        for (int i = 0; i < nexus::kNumZones; ++i) zoneBtn[i].setBounds(Z.removeFromLeft(150).reduced(2, 0));
        warn.setBounds(Z.reduced(8, 0));
        tune.setBounds(B.removeFromRight(340)); B.removeFromRight(10); sound.setBounds(B);
        eq.setBounds(C.removeFromLeft(C.getWidth() * 9 / 20)); C.removeFromLeft(10); comp.setBounds(C);
        reverb.setBounds(D.removeFromRight(330)); D.removeFromRight(10); mix.setBounds(D);
    }
private:
    nexus::Engine& engine;
    DrumVisual visual; DetectionCard det; SoundPanel sound; TuneCard tune; LibraryCard lib; InputCard input;
    KnobCard eq, comp, reverb, mix;
    EqCurve curve;
    Label warn;
    int selected = 0;
    TextButton zoneBtn[nexus::kNumZones], testBtn;
};

// ============================================================================
//  Página: Áudio
// ============================================================================
class MainComponent::AudioPage : public PageBase {
public:
    explicit AudioPage(AudioDeviceManager& dm) : card(theme::u8("Áudio · Dispositivos")), selector(dm, 0, 8, 0, 2, false, false, false, false) {
        addAndMakeVisible(card);
        card.addAndMakeVisible(selector);
        help.setText(theme::u8("Como ligar: use uma interface de áudio e escolha o driver dela acima (no Windows, ASIO ou WASAPI de baixa latência; no Mac, CoreAudio). Ative as entradas que for usar (até 8). "
                               "No Trigger (módulo virtual), cada pad escolhe a entrada que ouve; com 2 entradas você usa 2 pads, com 8 usa a bateria inteira. No EAD, as entradas 1 e 2 são o par estéreo de microfones. "
                               "Triggers/pads piezo (ex.: Roland) entram na entrada de instrumento (Hi-Z) com o phantom power DESLIGADO. Para tocar ao vivo, deixe o buffer em 64 a 128 amostras. Se ouvir estalos, aumente o buffer."), dontSendNotification);
        help.setFont(theme::font(13.5f)); help.setColour(Label::textColourId, theme::muted); help.setJustificationType(Justification::topLeft);
        card.addAndMakeVisible(help);
    }
    void refresh() override {}
    void resized() override {
        card.setBounds(getLocalBounds());
        auto r = card.content();
        help.setBounds(r.removeFromBottom(110));
        selector.setBounds(r);
    }
private:
    Card card;
    AudioDeviceSelectorComponent selector;
    Label help;
};

// ============================================================================
//  Página: Aparência (cores personalizadas)
// ============================================================================
class MainComponent::LookPage : public PageBase, private ChangeListener {
public:
    explicit LookPage(std::function<void()> onTheme)
        : card(theme::u8("Aparência · Cores")), onThemeChanged(std::move(onTheme)),
          selector(ColourSelector::showColourAtTop | ColourSelector::showSliders | ColourSelector::showColourspace, 4, 8) {
        addAndMakeVisible(card);
        struct P { const char* n; uint32 c; };
        const P pal[] = {{"Verde", 0xff39e639}, {"Ciano", 0xff20d6e8}, {"Azul", 0xff3b82ff}, {"Roxo", 0xffa35bff},
                         {"Rosa", 0xffff4fa3}, {"Vermelho", 0xffff4d4d}, {"Laranja", 0xffff8a1f}, {"Amarelo", 0xffffd21f}};
        for (auto& p : pal) {
            auto* s = swatches.add(new Swatch(Colour(p.c)));
            s->onPick = [this](Colour c) { pick(c); };
            card.addAndMakeVisible(s);
        }
        for (auto* l : {&l1, &l2}) { l->setFont(theme::font(14.0f, true)); l->setColour(Label::textColourId, theme::accent); card.addAndMakeVisible(*l); }
        l1.setText(theme::u8("COR DE DESTAQUE"), dontSendNotification);
        l2.setText("FUNDO", dontSendNotification);
        hint.setText(theme::u8("A cor muda na hora e fica salva para a próxima vez. Escolha uma cor pronta ou crie a sua no seletor."), dontSendNotification);
        hint.setFont(theme::font(13.0f)); hint.setColour(Label::textColourId, theme::muted); hint.setJustificationType(Justification::topLeft);
        card.addAndMakeVisible(hint);
        selector.setCurrentColour(theme::accent, dontSendNotification);
        selector.addChangeListener(this);
        card.addAndMakeVisible(selector);
        for (int i = 0; i < 4; ++i) {
            bg[i].setButtonText(theme::u8(theme::bgNames[i]));
            bg[i].setClickingTogglesState(true); bg[i].setRadioGroupId(77);
            bg[i].onClick = [this, i] { theme::setBackground(i); onThemeChanged(); };
            card.addAndMakeVisible(bg[i]);
        }
        reset.setButtonText(theme::u8("Restaurar padrão"));
        reset.onClick = [this] { theme::setBackground(0); pick(Colour(0xff39e639)); };
        card.addAndMakeVisible(reset);
        refresh();
    }
    void refresh() override { for (int i = 0; i < 4; ++i) bg[i].setToggleState(theme::bgStyle == i, dontSendNotification); for (auto* s : swatches) s->repaint(); }
    void resized() override {
        card.setBounds(getLocalBounds());
        auto r = card.content();
        auto left = r.removeFromLeft(juce::jmin(470, r.getWidth() * 3 / 5));
        r.removeFromLeft(24);
        l1.setBounds(left.removeFromTop(24)); left.removeFromTop(6);
        auto row = left.removeFromTop(46);
        for (auto* s : swatches) s->setBounds(row.removeFromLeft(54).reduced(4));
        left.removeFromTop(10);
        selector.setBounds(left.removeFromTop(juce::jmin(300, left.getHeight())));
        l2.setBounds(r.removeFromTop(24)); r.removeFromTop(6);
        for (auto& b : bg) { b.setBounds(r.removeFromTop(40).withWidth(jmin(260, r.getWidth()))); r.removeFromTop(8); }
        r.removeFromTop(8);
        reset.setBounds(r.removeFromTop(40).withWidth(jmin(260, r.getWidth()))); r.removeFromTop(14);
        hint.setBounds(r.removeFromTop(70));
    }
private:
    class Swatch : public Component {
    public:
        explicit Swatch(Colour c) : colour(c) {}
        std::function<void(Colour)> onPick;
        void paint(Graphics& g) override {
            auto r = getLocalBounds().toFloat().reduced(2.0f);
            g.setColour(colour.withAlpha(0.3f)); g.fillEllipse(r.expanded(2.0f));
            g.setColour(colour); g.fillEllipse(r);
            if (colour == theme::accent) { g.setColour(Colours::white); g.drawEllipse(r.expanded(2.5f), 2.0f); }
        }
        void mouseDown(const MouseEvent&) override { if (onPick) onPick(colour); }
    private:
        Colour colour;
    };
    void pick(Colour c) {
        theme::setAccent(c);
        syncing = true; selector.setCurrentColour(c, dontSendNotification); syncing = false;
        onThemeChanged(); refresh();
    }
    void changeListenerCallback(ChangeBroadcaster*) override {
        if (syncing) return;
        theme::setAccent(selector.getCurrentColour().withAlpha(1.0f));
        onThemeChanged(); refresh();
    }
    Card card;
    std::function<void()> onThemeChanged;
    ColourSelector selector;
    OwnedArray<Swatch> swatches;
    Label l1, l2, hint;
    TextButton bg[4], reset;
    bool syncing = false;
};

// ============================================================================
//  MainComponent
// ============================================================================
File MainComponent::dataDir() const {
    return File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Nexus Drum Trigger");
}

MainComponent::MainComponent() {
    theme::load();
    lnf.applyColours();
    LookAndFeel::setDefaultLookAndFeel(&lnf);

    engine.prepare(48000.0); // monta o banco de sons para a interface (refeito em prepareToPlay)
    loadCustomSounds();
    loadPreset(engine.params, dataDir().getChildFile("ultima-sessao.ndtkit"));
    engine.params.mode = static_cast<int>(nexus::Mode::Module);

    auto changed = [this] { refreshAll(); };
    pages[0] = std::unique_ptr<PageBase>(new TriggerPage(engine, changed, 2));
    pages[1] = std::unique_ptr<PageBase>(new TriggerPage(engine, changed, nexus::kNumPads));
    pages[2] = std::unique_ptr<PageBase>(new EadPage(engine, changed));
    pages[3] = std::unique_ptr<PageBase>(new AudioPage(deviceManager));
    pages[4] = std::unique_ptr<PageBase>(new LookPage([this] { applyTheme(); }));
    for (auto& p : pages) addChildComponent(*p);
    {
        auto imp = [this] { importSounds(); };
        static_cast<TriggerPage*>(pages[0].get())->setImportHandler(imp);
        static_cast<TriggerPage*>(pages[1].get())->setImportHandler(imp);
        static_cast<EadPage*>(pages[2].get())->setImportHandler(imp);
    }

    const String labels[] = {"Trigger", theme::u8("Bateria eletrônica"), "EAD", theme::u8("Áudio"), theme::u8("Aparência")};
    for (int i = 0; i < 5; ++i) {
        static const int icons[] = {0, 4, 1, 2, 3};
        nav[i] = std::make_unique<NavButton>(labels[i], icons[i]);
        nav[i]->onClick = [this, i] { showPage(i); };
        addAndMakeVisible(*nav[i]);
    }

    for (auto* b : {&kitPrev, &kitNext, &kitBtn, &gearBtn}) { b->getProperties().set("flat", true); addAndMakeVisible(*b); }
    kitPrev.setButtonText(theme::u8("‹")); kitNext.setButtonText(theme::u8("›"));
    kitPrev.onClick = [this] { stepKit(-1); }; kitNext.onClick = [this] { stepKit(+1); };
    kitBtn.onClick = [this] { showKitMenu(); };
    gearBtn.setButtonText(theme::u8("⚙"));
    gearBtn.onClick = [this] { showPage(3); };
    kitBtn.setButtonText(theme::u8("Kit atual"));
    kitBtn.setColour(TextButton::textColourOffId, theme::text);

    refreshAll();
    showPage(0);
    setSize(1280, 860);

    if (RuntimePermissions::isRequired(RuntimePermissions::recordAudio) && !RuntimePermissions::isGranted(RuntimePermissions::recordAudio))
        RuntimePermissions::request(RuntimePermissions::recordAudio, [this](bool granted) { setAudioChannels(granted ? 8 : 0, 2); });
    else
        setAudioChannels(8, 2);

    startTimerHz(30);
}

MainComponent::~MainComponent() {
    stopTimer();
    savePreset(engine.params, dataDir().getChildFile("ultima-sessao.ndtkit"));
    shutdownAudio();
    LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    const size_t cap = static_cast<size_t>(jmax(256, samplesPerBlockExpected));
    inBuf.assign(nexus::kMaxInputs, std::vector<float>(cap, 0.0f)); outL.assign(cap, 0.0f); outR.assign(cap, 0.0f);
    currentFs = sampleRate;
    engine.prepare(sampleRate);
    loadCustomSounds();
    MessageManager::callAsync([this] { refreshAll(); });
}

void MainComponent::getNextAudioBlock(const AudioSourceChannelInfo& info) {
    auto* buf = info.buffer;
    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev == nullptr || inBuf.empty()) { info.clearActiveBufferRegion(); return; }

    const int nIn = dev->getActiveInputChannels().countNumberOfSetBits();
    const int nCh = buf->getNumChannels();
    const int cap = static_cast<int>(inBuf[0].size());

    for (int off = 0; off < info.numSamples; off += cap) {
        const int n = jmin(cap, info.numSamples - off);
        const int at = info.startSample + off;
        const int use = jmin(nIn, nCh, static_cast<int>(nexus::kMaxInputs));
        for (int c = 0; c < nexus::kMaxInputs; ++c) {
            if (c < use) { FloatVectorOperations::copy(inBuf[static_cast<size_t>(c)].data(), buf->getReadPointer(c, at), n); inPtr[c] = inBuf[static_cast<size_t>(c)].data(); }
            else inPtr[c] = nullptr;
        }

        engine.processBlock(inPtr, use, outL.data(), outR.data(), n);

        for (int ch = 0; ch < nCh; ++ch) {
            if (ch == 0) FloatVectorOperations::copy(buf->getWritePointer(0, at), outL.data(), n);
            else if (ch == 1) FloatVectorOperations::copy(buf->getWritePointer(1, at), outR.data(), n);
            else FloatVectorOperations::clear(buf->getWritePointer(ch, at), n);
        }
        if (nCh == 1) {
            auto* o = buf->getWritePointer(0, at);
            for (int i = 0; i < n; ++i) o[i] = 0.5f * (outL[static_cast<size_t>(i)] + outR[static_cast<size_t>(i)]);
        }
    }
}

int MainComponent::loadWavInto(const File& f) {
    AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> rd(fm.createReaderFor(f));
    if (rd == nullptr || rd->lengthInSamples < 64 || rd->sampleRate <= 0) return -1;
    const int64 len = jmin<int64>(rd->lengthInSamples, static_cast<int64>(rd->sampleRate * 10.0));
    AudioBuffer<float> buf(static_cast<int>(rd->numChannels), static_cast<int>(len));
    rd->read(&buf, 0, static_cast<int>(len), 0, true, true);
    std::vector<float> mono(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i) { float v = 0.0f; for (int c = 0; c < buf.getNumChannels(); ++c) v += buf.getSample(c, i); mono[static_cast<size_t>(i)] = v / static_cast<float>(jmax(1, buf.getNumChannels())); }
    const double ratio = currentFs / rd->sampleRate;
    std::vector<float> out;
    if (std::abs(ratio - 1.0) < 1e-6) out = std::move(mono);
    else {
        const size_t n = static_cast<size_t>(static_cast<double>(mono.size()) * ratio);
        out.resize(n);
        for (size_t i = 0; i < n; ++i) {
            const double pos = static_cast<double>(i) / ratio; const size_t i0 = static_cast<size_t>(pos);
            const float fr = static_cast<float>(pos - static_cast<double>(i0));
            const float a = mono[jmin(i0, mono.size() - 1)], b = mono[jmin(i0 + 1, mono.size() - 1)];
            out[i] = a + (b - a) * fr;
        }
    }
    String name = f.getFileNameWithoutExtension();
    if (name.length() > 4 && name[3] == '_' && name.substring(0, 3).containsOnly("0123456789")) name = name.substring(4);
    return engine.soundBank().addCustom(name.toStdString(), std::move(out));
}

void MainComponent::loadCustomSounds() {
    auto files = dataDir().getChildFile("Sons").findChildFiles(File::findFiles, false, "*.*");
    files.sort();
    for (auto& f : files) loadWavInto(f);
}

void MainComponent::importSounds() {
    const auto dir = dataDir().getChildFile("Sons"); dir.createDirectory();
    chooser = std::make_shared<FileChooser>("Importar sons (WAV, AIFF, FLAC)", File::getSpecialLocation(File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
    chooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectMultipleItems | FileBrowserComponent::canSelectFiles,
                         [this, dir](const FileChooser& fc) {
                             int last = -1;
                             for (auto& f : fc.getResults()) {
                                 int next = dir.getNumberOfChildFiles(File::findFiles) + 1;
                                 const auto dest = dir.getChildFile(String::formatted("%03d_", next) + f.getFileName());
                                 if (!f.copyFileTo(dest)) continue;
                                 const int idx = loadWavInto(dest);
                                 if (idx >= 0) last = idx;
                             }
                             if (last >= 0) { engine.params.previewSound = last; refreshAll(); }
                         });
}

void MainComponent::showPage(int index) {
    if (currentPage == 1) modulePads = jmax(2, engine.params.activePads.load());
    currentPage = index;
    for (int i = 0; i < 5; ++i) { pages[i]->setVisible(i == index); nav[i]->setToggleState(i == index, dontSendNotification); }
    if (index == 0) { engine.params.mode = static_cast<int>(nexus::Mode::Module); engine.params.activePads = 2; }
    else if (index == 1) { engine.params.mode = static_cast<int>(nexus::Mode::Module); engine.params.activePads = jmax(2, modulePads); }
    else if (index == 2) engine.params.mode = static_cast<int>(nexus::Mode::Hybrid);
    refreshAll();
    repaint();
}

void MainComponent::refreshAll() {
    for (auto& p : pages) if (p) p->refresh();
}

void MainComponent::applyKit(int index) {
    currentKit = index;
    engine.params.pad[0].sound = kKits[index].s0;
    engine.params.pad[1].sound = kKits[index].s1;
    engine.params.zone[0].sound = kKits[index].s0;
    engine.params.zone[1].sound = kKits[index].s1;
    kitBtn.setButtonText(theme::u8(kKits[index].name));
    engine.params.previewSound = kKits[index].s0;
    refreshAll();
}

void MainComponent::stepKit(int delta) {
    const int next = currentKit < 0 ? (delta > 0 ? 0 : kNumKits - 1) : (currentKit + delta + kNumKits) % kNumKits;
    applyKit(next);
}

void MainComponent::showKitMenu() {
    PopupMenu m;
    m.addSectionHeader(theme::u8("Kits prontos"));
    for (int i = 0; i < kNumKits; ++i) m.addItem(i + 1, theme::u8(kKits[i].name), true, i == currentKit);
    m.addSeparator();
    m.addItem(100, theme::u8("Abrir kit salvo…"));
    m.addItem(101, theme::u8("Salvar kit…"));
    m.showMenuAsync(PopupMenu::Options().withTargetComponent(&kitBtn), [this](int id) {
        if (id >= 1 && id <= kNumKits) applyKit(id - 1);
        else if (id == 100) loadKit();
        else if (id == 101) saveKit();
    });
}

void MainComponent::applyTheme() {
    lnf.applyColours();
    theme::save();
    kitBtn.setColour(TextButton::textColourOffId, theme::text);
    sendLookAndFeelChange();
    repaint();
}

void MainComponent::timerCallback() {
    pages[currentPage]->tick();
    String s;
    if (auto* d = deviceManager.getCurrentAudioDevice()) {
        const double sr = d->getCurrentSampleRate();
        latencyMs = sr > 0 ? (d->getCurrentBufferSizeSamples() + d->getInputLatencyInSamples() + d->getOutputLatencyInSamples()) * 1000.0 / sr : 0.0;
        const int ins = d->getActiveInputChannels().countNumberOfSetBits();
        s = String(ins) + (ins == 1 ? " entrada" : " entradas");
        if (engine.params.mode.load() == static_cast<int>(nexus::Mode::Module)) {
            const int act = engine.params.activePads.load();
            s += theme::u8(" · ") + String(act) + (act == 1 ? " pad ativo" : " pads ativos");
            if (ins < 1) s += theme::u8(" · ative entradas em Áudio");
        } else if (ins < 2) s += theme::u8(" · EAD usa 2 entradas");
        pages[currentPage]->setInputs(jmax(1, ins));
    } else {
        latencyMs = 0.0; s = theme::u8("sem dispositivo · veja Áudio");
    }
    cpu = deviceManager.getCpuUsage();
    deviceInfo = s;
    repaint(statusRect);
}

void MainComponent::saveKit() {
    dataDir().createDirectory();
    chooser = std::make_shared<FileChooser>("Salvar kit", dataDir().getChildFile("meu-kit.ndtkit"), "*.ndtkit");
    chooser->launchAsync(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                         [this](const FileChooser& fc) {
                             const auto f = fc.getResult();
                             if (f != File()) savePreset(engine.params, f.withFileExtension("ndtkit"));
                         });
}

void MainComponent::loadKit() {
    dataDir().createDirectory();
    chooser = std::make_shared<FileChooser>("Abrir kit", dataDir(), "*.ndtkit");
    chooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                         [this](const FileChooser& fc) {
                             const auto f = fc.getResult();
                             if (f.existsAsFile() && loadPreset(engine.params, f)) { currentKit = -1; kitBtn.setButtonText(f.getFileNameWithoutExtension()); refreshAll(); }
                         });
}

void MainComponent::paint(Graphics& g) {
    g.fillAll(theme::bg);
    // barra lateral
    g.setColour(theme::panelBot); g.fillRect(0, 72, 200, getHeight() - 72);
    g.setColour(theme::edge); g.fillRect(199, 72, 1, getHeight() - 72);
    g.fillRect(0, 71, getWidth(), 1);

    // logo
    const float cy = 36.0f;
    const float hs[] = {0.25f, 0.55f, 0.95f, 0.45f, 0.75f, 0.35f, 0.15f};
    g.setColour(theme::accent);
    for (int i = 0; i < 7; ++i) g.fillRoundedRectangle(18.0f + static_cast<float>(i) * 6.0f, cy - hs[i] * 22.0f, 3.2f, hs[i] * 44.0f, 1.6f);
    g.setFont(theme::font(34.0f, true));
    float x = 70.0f;
    auto part = [&](const String& t, Colour c) { g.setColour(c); const float w = g.getCurrentFont().getStringWidthFloat(t); g.drawText(t, static_cast<int>(x), 6, static_cast<int>(w) + 4, 40, Justification::centredLeft); x += w; };
    part("NE", theme::text); part("X", theme::accent); part("US", theme::text);
    g.setColour(theme::muted); g.setFont(theme::font(16.0f));
    g.drawText("DRUM TRIGGER", 72, 42, 150, 20, Justification::centredLeft);
    g.setFont(theme::font(11.5f)); g.drawText("v" + String(JUCE_APPLICATION_VERSION_STRING), 190, 44, 50, 16, Justification::centredLeft);

    // seletor de kit
    {
        auto r = kitRect.toFloat();
        g.setGradientFill(ColourGradient(theme::panelTop, 0, r.getY(), theme::panelBot, 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 12.0f);
        g.setColour(theme::edge.brighter(0.15f)); g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, 1.2f);
        const String sub = currentPage == 0 ? theme::u8("Módulo eletrônico") : (currentPage == 1 ? theme::u8("Acústico + Eletrônico") : theme::u8("Configurações"));
        g.setColour(theme::muted); g.setFont(theme::font(13.0f));
        g.drawText(sub, kitRect.withTrimmedTop(36).withHeight(22), Justification::centred);
    }

    // CPU / latência
    {
        auto s = statusRect;
        g.setFont(theme::font(13.0f));
        g.setColour(theme::muted); g.drawText("CPU", s.getX(), s.getY() + 6, 36, 18, Justification::centredLeft);
        g.setColour(theme::text); g.drawText(String(roundToInt(static_cast<float>(cpu) * 100.0f)) + "%", s.getX() + 38, s.getY() + 6, 44, 18, Justification::centredLeft);
        g.setColour(theme::muted); g.drawText(theme::u8("Latência"), s.getX(), s.getY() + 30, 62, 18, Justification::centredLeft);
        g.setColour(theme::text); g.drawText(String(latencyMs, 1) + " ms", s.getX() + 64, s.getY() + 30, 64, 18, Justification::centredLeft);
        auto bar = [&](int y, float v, Colour c) {
            Rectangle<float> b(static_cast<float>(s.getX() + 130), static_cast<float>(y), 110.0f, 7.0f);
            g.setColour(theme::track); g.fillRoundedRectangle(b, 3.5f);
            g.setColour(c); g.fillRoundedRectangle(b.withWidth(jmax(3.0f, b.getWidth() * jlimit(0.0f, 1.0f, v))), 3.5f);
        };
        bar(s.getY() + 12, static_cast<float>(cpu), cpu > 0.8 ? theme::danger : theme::accent);
        bar(s.getY() + 36, static_cast<float>(latencyMs / 20.0), latencyMs > 12.0 ? theme::warn : theme::accent);
        g.setColour(theme::muted); g.setFont(theme::font(11.5f));
        g.drawText(deviceInfo, s.getX(), s.getY() + 52, 280, 16, Justification::centredLeft);
    }
}

void MainComponent::resized() {
    auto r = getLocalBounds();
    auto header = r.removeFromTop(72);
    // seletor de kit centralizado
    const auto box = Rectangle<int>(0, 0, 430, 58).withCentre({juce::jmax(560, getWidth() / 2 - 60), 36});
    kitRect = box;
    auto inner = box;
    kitPrev.setBounds(inner.removeFromLeft(48).reduced(4));
    kitNext.setBounds(inner.removeFromRight(48).reduced(4));
    kitBtn.setBounds(inner.withHeight(34).translated(0, 4));
    statusRect = Rectangle<int>(kitRect.getRight() + 30, 6, 290, 66);
    gearBtn.setBounds(header.getRight() - 64, 14, 48, 44);

    auto side = r.removeFromLeft(200).withTrimmedTop(14);
    for (int i = 0; i < 5; ++i) nav[i]->setBounds(side.removeFromTop(54));
    auto area = r.reduced(10);
    for (auto& p : pages) p->setBounds(area);
}
