#include "MainComponent.h"
#include "Preset.h"

using namespace juce;

static const Colour kBg(0xff14161a), kPanel(0xff1d2027), kAccent(0xff2fd0c4), kHot(0xffff8a3d), kMuted(0xff8b93a1);

// ---------------------------------------------------------------- ParamSlider
ParamSlider::ParamSlider(const String& name, double mn, double mx, double step, const String& suffix, double skewMid) {
    label.setText(name, dontSendNotification);
    label.setFont(Font(FontOptions(13.0f)));
    label.setColour(Label::textColourId, kMuted);
    addAndMakeVisible(label);
    slider.setSliderStyle(Slider::LinearHorizontal);
    slider.setTextBoxStyle(Slider::TextBoxRight, false, 78, 20);
    slider.setRange(mn, mx, step);
    if (skewMid > mn && skewMid < mx) slider.setSkewFactorFromMidPoint(skewMid);
    slider.setTextValueSuffix(suffix);
    slider.onValueChange = [this] { if (onChange) onChange(static_cast<float>(slider.getValue())); };
    addAndMakeVisible(slider);
}

void ParamSlider::resized() {
    auto r = getLocalBounds();
    label.setBounds(r.removeFromLeft(juce::jmin(130, r.getWidth() / 3)));
    slider.setBounds(r);
}

// ---------------------------------------------------------------- MeterBar
void MeterBar::setLevel(float lin) {
    const float db = Decibels::gainToDecibels(lin, -80.0f);
    const float norm = jlimit(0.0f, 1.0f, jmap(db, -60.0f, 0.0f, 0.0f, 1.0f));
    shown = jmax(norm, shown * 0.88f);
    repaint();
}

void MeterBar::paint(Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(Colour(0xff0e1013)); g.fillRoundedRectangle(r, 3.0f);
    auto bar = r.withWidth(r.getWidth() * shown);
    g.setColour(shown > 0.92f ? Colours::red : (shown > 0.75f ? kHot : kAccent));
    g.fillRoundedRectangle(bar, 3.0f);
}

// ---------------------------------------------------------------- PadStrip
PadStrip::PadStrip(int idx, nexus::Engine& e, bool compactMode, std::function<void()> onUserChange)
    : index(idx), engine(e), compact(compactMode), changed(std::move(onUserChange)),
      level("Volume", 0.0, 1.5, 0.01, ""),
      pitch(String::fromUTF8("Afinação"), -12.0, 12.0, 0.1, " st"),
      pan("Pan", -1.0, 1.0, 0.01, ""),
      inGain("Ganho do microfone", -12.0, 36.0, 0.5, " dB"),
      thr("Limiar (sensib.)", -60.0, -6.0, 0.5, " dB"),
      retrig("Retrigger", 20.0, 300.0, 1.0, " ms"),
      hp("Filtro grave (HP)", 20.0, 600.0, 1.0, " Hz", 150.0),
      lp("Filtro agudo (LP)", 500.0, 16000.0, 10.0, " Hz", 3000.0),
      dyn(String::fromUTF8("Curva dinâmica"), 0.4, 2.5, 0.01, ""),
      sens(String::fromUTF8("Resposta à força"), 0.0, 1.0, 0.01, "") {
    title.setText(compact ? String(String::fromUTF8("GATILHO  ·  par estéreo (Mic 1 + Mic 2)")) : "PAD " + String(index + 1) + String::fromUTF8("  ·  Microfone ") + String(index + 1), dontSendNotification);
    title.setFont(Font(FontOptions(15.0f, Font::bold)));
    title.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible(title);

    velLabel.setText("Vel. -", dontSendNotification);
    velLabel.setJustificationType(Justification::centredRight);
    velLabel.setColour(Label::textColourId, kMuted);
    addAndMakeVisible(velLabel);

    const auto& bank = engine.sounds();
    for (int i = 0; i < bank.size(); ++i) soundBox.addItem(String::fromUTF8(bank.get(i).category.c_str()) + String::fromUTF8("  ›  ") + String::fromUTF8(bank.get(i).name.c_str()), i + 1);
    soundBox.onChange = [this] {
        engine.params.pad[index].sound = soundBox.getSelectedId() - 1;
        changed();
    };
    addAndMakeVisible(soundBox);

    testBtn.onClick = [this] { engine.params.manualHit[index] = 900; };
    addAndMakeVisible(testBtn);
    addAndMakeVisible(meter);

    auto P = [this]() -> nexus::PadParams& { return engine.params.pad[index]; };
    level.onChange = [this, P](float v) { P().level = v; changed(); };
    pitch.onChange = [this, P](float v) { P().pitchSemis = v; changed(); };
    pan.onChange = [this, P](float v) { P().pan = v; changed(); };
    inGain.onChange = [this, P](float v) { P().inputGainDb = v; changed(); };
    thr.onChange = [this, P](float v) { P().thresholdDb = v; changed(); };
    retrig.onChange = [this, P](float v) { P().retriggerMs = v; changed(); };
    hp.onChange = [this, P](float v) { P().hpHz = v; changed(); };
    lp.onChange = [this, P](float v) { P().lpHz = v; changed(); };
    dyn.onChange = [this, P](float v) { P().dynamics = v; changed(); };
    sens.onChange = [this, P](float v) { P().velocitySens = v; changed(); };

    addRow(level, true); addRow(thr, true); addRow(retrig, true);
    addRow(inGain, true); addRow(hp, true); addRow(lp, true);
    addRow(dyn, false); addRow(sens, false); addRow(pitch, false); addRow(pan, false);
    refresh();
}

void PadStrip::addRow(ParamSlider& s, bool showInCompact) {
    if (compact && !showInCompact) return;
    addAndMakeVisible(s);
    rows.push_back(&s);
}

void PadStrip::refresh() {
    auto& p = engine.params.pad[index];
    soundBox.setSelectedId(p.sound.load() + 1, dontSendNotification);
    level.setValue(p.level.load()); pitch.setValue(p.pitchSemis.load()); pan.setValue(p.pan.load());
    inGain.setValue(p.inputGainDb.load()); thr.setValue(p.thresholdDb.load()); retrig.setValue(p.retriggerMs.load());
    hp.setValue(p.hpHz.load()); lp.setValue(p.lpHz.load()); dyn.setValue(p.dynamics.load()); sens.setValue(p.velocitySens.load());
}

void PadStrip::tick() {
    meter.setLevel(engine.status.inputLevel[index].load());
    const unsigned h = engine.status.hitCount[index].load();
    if (h != lastHits) {
        lastHits = h;
        flash = 1.0f;
        velLabel.setText("Vel. " + String(roundToInt(engine.status.lastVelocity[index].load() * 127.0f)), dontSendNotification);
    }
    if (flash > 0.01f) { flash *= 0.82f; repaint(getWidth() - 44, 4, 36, 32); }
}

void PadStrip::resized() {
    auto r = getLocalBounds().reduced(12, 8);
    auto head = r.removeFromTop(26);
    head.removeFromRight(30); // espaço da luz de batida
    title.setBounds(head);
    auto row = r.removeFromTop(30);
    testBtn.setBounds(row.removeFromRight(80).reduced(0, 2));
    row.removeFromRight(6);
    soundBox.setBounds(row.reduced(0, 2));
    auto m = r.removeFromTop(20);
    velLabel.setBounds(m.removeFromRight(80));
    meter.setBounds(m.reduced(0, 5));
    r.removeFromTop(4);
    for (auto* s : rows) s->setBounds(r.removeFromTop(30));
}

void PadStrip::paint(Graphics& g) {
    g.setColour(kPanel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    const auto c = Colour(0xff3a3f4a).interpolatedWith(kHot, flash);
    g.setColour(c);
    g.fillEllipse(static_cast<float>(getWidth()) - 34.0f, 14.0f, 16.0f, 16.0f);
}

// ---------------------------------------------------------------- Painéis
namespace {
// Slider ligado a um parâmetro do motor (lê e escreve).
struct Bound {
    std::unique_ptr<ParamSlider> s;
    std::function<float()> get;
};

Label* makeHeading(Component& parent, const String& text) {
    auto* l = new Label();
    l->setText(text, dontSendNotification);
    l->setFont(Font(FontOptions(14.0f, Font::bold)));
    l->setColour(Label::textColourId, kAccent);
    parent.addAndMakeVisible(l);
    return l;
}
} // namespace

class MainComponent::ModulePanel : public Component {
public:
    ModulePanel(nexus::Engine& e, std::function<void()> changed)
        : s1(0, e, false, changed), s2(1, e, false, changed) {
        addAndMakeVisible(s1); addAndMakeVisible(s2);
        auto add = [&](const String& n, double mn, double mx, double st, const String& suf, std::function<float()> get, std::function<void(float)> set) {
            Bound b; b.s = std::make_unique<ParamSlider>(n, mn, mx, st, suf); b.get = std::move(get);
            b.s->onChange = [set, changed](float v) { set(v); changed(); };
            addAndMakeVisible(*b.s); bound.push_back(std::move(b));
        };
        auto& p = e.params;
        add("Anti-vazamento", 0, 1, 0.01, "", [&p] { return p.bleedReject.load(); }, [&p](float v) { p.bleedReject = v; });
        add("Reverb (sala)", 0, 1, 0.01, "", [&p] { return p.reverbWet.load(); }, [&p](float v) { p.reverbWet = v; });
        add("Volume geral", 0, 1.2, 0.01, "", [&p] { return p.master.load(); }, [&p](float v) { p.master = v; });
        info.setText("Dica: aumente o Anti-vazamento se o microfone de um pad disparar o outro (ex.: caixa soando no microfone do bumbo).", dontSendNotification);
        info.setFont(Font(FontOptions(12.5f)));
        info.setColour(Label::textColourId, kMuted);
        addAndMakeVisible(info);
    }
    void refresh() { s1.refresh(); s2.refresh(); for (auto& b : bound) b.s->setValue(b.get()); }
    void tick() { s1.tick(); s2.tick(); }
    void resized() override {
        auto r = getLocalBounds().reduced(10);
        auto bottom = r.removeFromBottom(110);
        auto left = r.removeFromLeft(r.getWidth() / 2 - 6);
        r.removeFromLeft(12);
        s1.setBounds(left); s2.setBounds(r);
        bottom.removeFromTop(8);
        auto cols = bottom.removeFromTop(30);
        const int w = cols.getWidth() / 3;
        for (size_t i = 0; i < bound.size(); ++i) bound[i].s->setBounds(cols.removeFromLeft(w).reduced(6, 0));
        info.setBounds(bottom.removeFromTop(40));
    }
    PadStrip s1, s2;
private:
    std::vector<Bound> bound;
    Label info;
};

class MainComponent::EadPanel : public Component {
public:
    EadPanel(nexus::Engine& e, std::function<void()> changed)
        : engine(e), s1(0, e, true, changed) {
        addAndMakeVisible(s1);
        auto& p = e.params;
        auto add = [&](const String& n, double mn, double mx, double st, const String& suf, std::function<float()> get, std::function<void(float)> set) -> ParamSlider* {
            Bound b; b.s = std::make_unique<ParamSlider>(n, mn, mx, st, suf); b.get = std::move(get);
            b.s->onChange = [set, changed](float v) { set(v); changed(); };
            addAndMakeVisible(*b.s); auto* raw = b.s.get(); bound.push_back(std::move(b)); return raw;
        };
        hMix.reset(makeHeading(*this, "ENTRADAS E MISTURA"));
        micA = add(String::fromUTF8("Mic estéreo (nível)"), 0, 1.5, 0.01, "", [&p] { return p.acousticLevel.load(); }, [&p](float v) { p.acousticLevel = v; });
        micB = add(String::fromUTF8("Largura estéreo"), 0, 1.5, 0.01, "", [&p] { return p.stereoWidth.load(); }, [&p](float v) { p.stereoWidth = v; });
        elec = add(String::fromUTF8("Sons eletrônicos"), 0, 1.5, 0.01, "", [&p] { return p.electronicLevel.load(); }, [&p](float v) { p.electronicLevel = v; });
        mast = add("Volume geral", 0, 1.2, 0.01, "", [&p] { return p.master.load(); }, [&p](float v) { p.master = v; });
        hEq.reset(makeHeading(*this, "EQUALIZADOR"));
        eqL = add("Grave", -12, 12, 0.5, " dB", [&p] { return p.eqLowDb.load(); }, [&p](float v) { p.eqLowDb = v; });
        eqM = add(String::fromUTF8("Médio"), -12, 12, 0.5, " dB", [&p] { return p.eqMidDb.load(); }, [&p](float v) { p.eqMidDb = v; });
        eqH = add("Agudo", -12, 12, 0.5, " dB", [&p] { return p.eqHighDb.load(); }, [&p](float v) { p.eqHighDb = v; });
        hComp.reset(makeHeading(*this, "COMPRESSOR"));
        compBtn.setButtonText("Compressor ligado");
        compBtn.onClick = [this, changed] { engine.params.compOn = compBtn.getToggleState(); changed(); };
        addAndMakeVisible(compBtn);
        cThr = add("Limiar", -48, 0, 0.5, " dB", [&p] { return p.compThresholdDb.load(); }, [&p](float v) { p.compThresholdDb = v; });
        cRat = add("Taxa", 1, 20, 0.1, ":1", [&p] { return p.compRatio.load(); }, [&p](float v) { p.compRatio = v; });
        cAtt = add("Ataque", 1, 100, 1, " ms", [&p] { return p.compAttackMs.load(); }, [&p](float v) { p.compAttackMs = v; });
        cRel = add(String::fromUTF8("Liberação"), 20, 800, 1, " ms", [&p] { return p.compReleaseMs.load(); }, [&p](float v) { p.compReleaseMs = v; });
        cMk = add("Ganho final", 0, 18, 0.5, " dB", [&p] { return p.compMakeupDb.load(); }, [&p](float v) { p.compMakeupDb = v; });
        grLabel.setText(String::fromUTF8("Redução: 0.0 dB"), dontSendNotification);
        grLabel.setColour(Label::textColourId, kMuted);
        addAndMakeVisible(grLabel);
        hRev.reset(makeHeading(*this, String::fromUTF8("AMBIÊNCIA (REVERB)")));
        rRoom = add("Tamanho da sala", 0, 1, 0.01, "", [&p] { return p.reverbRoom.load(); }, [&p](float v) { p.reverbRoom = v; });
        rDamp = add("Abafamento", 0, 1, 0.01, "", [&p] { return p.reverbDamp.load(); }, [&p](float v) { p.reverbDamp = v; });
        rWet = add("Quantidade", 0, 1, 0.01, "", [&p] { return p.reverbWet.load(); }, [&p](float v) { p.reverbWet = v; });
        warn.setText(String::fromUTF8("Atenção: nesta aba os 2 microfones viram um par estéreo (Mic 1 = esquerda, Mic 2 = direita) e também saem nas caixas. Use fones ou a P.A. longe dos microfones para evitar microfonia."), dontSendNotification);
        warn.setFont(Font(FontOptions(12.5f)));
        warn.setColour(Label::textColourId, kHot);
        warn.setJustificationType(Justification::topLeft);
        addAndMakeVisible(warn);
        refresh();
    }
    void refresh() {
        s1.refresh();
        for (auto& b : bound) b.s->setValue(b.get());
        compBtn.setToggleState(engine.params.compOn.load(), dontSendNotification);
    }
    void tick() {
        s1.tick();
        grLabel.setText(String::fromUTF8("Redução: ") + String(engine.status.gainReductionDb.load(), 1) + " dB", dontSendNotification);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(10);
        auto left = r.removeFromLeft(juce::jmax(320, r.getWidth() * 2 / 5));
        r.removeFromLeft(12);
        auto w = left.removeFromBottom(54); warn.setBounds(w);
        s1.setBounds(left.removeFromTop(290));
        auto colA = r.removeFromLeft(r.getWidth() / 2 - 6);
        r.removeFromLeft(12);
        auto& colB = r;
        auto place = [](Rectangle<int>& col, Component* c, int h) { c->setBounds(col.removeFromTop(h)); };
        place(colA, hMix.get(), 26); for (auto* s : { micA, micB, elec, mast }) place(colA, s, 30);
        colA.removeFromTop(10);
        place(colA, hEq.get(), 26); for (auto* s : { eqL, eqM, eqH }) place(colA, s, 30);
        place(colB, hComp.get(), 26); place(colB, &compBtn, 28);
        for (auto* s : { cThr, cRat, cAtt, cRel, cMk }) place(colB, s, 30);
        place(colB, &grLabel, 24);
        colB.removeFromTop(10);
        place(colB, hRev.get(), 26); for (auto* s : { rRoom, rDamp, rWet }) place(colB, s, 30);
    }
    PadStrip s1;
private:
    nexus::Engine& engine;
    std::vector<Bound> bound;
    std::unique_ptr<Label> hMix, hEq, hComp, hRev;
    ParamSlider *micA, *micB, *elec, *mast, *eqL, *eqM, *eqH, *cThr, *cRat, *cAtt, *cRel, *cMk, *rRoom, *rDamp, *rWet;
    ToggleButton compBtn;
    Label grLabel, warn;
};

class MainComponent::AudioPanel : public Component {
public:
    explicit AudioPanel(AudioDeviceManager& dm)
        : selector(dm, 0, 2, 0, 2, false, false, false, false) {
        addAndMakeVisible(selector);
        help.setText(String::fromUTF8("Como ligar: use uma interface de áudio com 2 entradas e escolha o driver dela acima (no Windows, ASIO ou WASAPI de baixa latência). "
                                      "Mic 1 = canal de entrada 1, Mic 2 = canal de entrada 2. Para tocar ao vivo, deixe o tamanho do buffer em 64 a 128 amostras "
                                      "(latência de 2 a 5 ms). Se ouvir estalos, aumente o buffer."), dontSendNotification);
        help.setFont(Font(FontOptions(13.0f)));
        help.setColour(Label::textColourId, kMuted);
        help.setJustificationType(Justification::topLeft);
        addAndMakeVisible(help);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(14);
        help.setBounds(r.removeFromBottom(70));
        selector.setBounds(r);
    }
private:
    AudioDeviceSelectorComponent selector;
    Label help;
};

namespace {
class Tabs : public TabbedComponent {
public:
    Tabs() : TabbedComponent(TabbedButtonBar::TabsAtTop) {}
    std::function<void(int)> onChange;
    void currentTabChanged(int i, const String&) override { if (onChange) onChange(i); }
};
} // namespace

// ---------------------------------------------------------------- MainComponent
File MainComponent::dataDir() const {
    return File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Nexus Drum Trigger");
}

MainComponent::MainComponent() {
    if (auto* lf = dynamic_cast<LookAndFeel_V4*>(&LookAndFeel::getDefaultLookAndFeel())) {
        lf->setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
        lf->setColour(Slider::thumbColourId, kAccent);
        lf->setColour(Slider::trackColourId, kAccent.withAlpha(0.55f));
        lf->setColour(ToggleButton::tickColourId, kAccent);
    }

    engine.prepare(48000.0); // monta o banco de sons para a interface (refeito em prepareToPlay)
    engine.params.pad[0].sound = 0;  // Bumbo 808
    engine.params.pad[1].sound = 3;  // Caixa 909
    engine.params.pad[0].lpHz = 1500.0f; // microfone do bumbo: só graves
    engine.params.pad[0].hpHz = 40.0f;
    engine.params.pad[0].thresholdDb = -30.0f;
    engine.params.pad[1].hpHz = 120.0f;
    loadPreset(engine.params, dataDir().getChildFile("ultima-sessao.ndtkit"));
    engine.params.mode = static_cast<int>(nexus::Mode::Module);

    auto changed = [this] { refreshAll(); };
    auto t = std::make_unique<Tabs>();
    auto* mp = new ModulePanel(engine, changed);
    auto* ep = new EadPanel(engine, changed);
    auto* ap = new AudioPanel(deviceManager);
    modulePanel = mp; eadPanel = ep; audioPanel = ap;
    t->addTab(String::fromUTF8("Módulo de bateria"), Colour(0xff1a1d23), mp, true);
    t->addTab(String::fromUTF8("EAD · Acústico + Eletrônico"), Colour(0xff1a1d23), ep, true);
    t->addTab(String::fromUTF8("Áudio"), Colour(0xff1a1d23), ap, true);
    t->setTabBarDepth(38);
    t->onChange = [this](int i) { onTabChanged(i); };
    tabs = std::move(t);
    addAndMakeVisible(*tabs);

    saveBtn.onClick = [this] { saveKit(); };
    loadBtn.onClick = [this] { loadKit(); };
    addAndMakeVisible(saveBtn); addAndMakeVisible(loadBtn);
    statusLabel.setColour(Label::textColourId, kMuted);
    statusLabel.setJustificationType(Justification::centredRight);
    statusLabel.setFont(Font(FontOptions(12.5f)));
    addAndMakeVisible(statusLabel);

    refreshAll();
    setSize(1080, 700);

    if (RuntimePermissions::isRequired(RuntimePermissions::recordAudio) && !RuntimePermissions::isGranted(RuntimePermissions::recordAudio))
        RuntimePermissions::request(RuntimePermissions::recordAudio, [this](bool granted) { setAudioChannels(granted ? 2 : 0, 2); });
    else
        setAudioChannels(2, 2);

    startTimerHz(30);
}

MainComponent::~MainComponent() {
    stopTimer();
    savePreset(engine.params, dataDir().getChildFile("ultima-sessao.ndtkit"));
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    const size_t cap = static_cast<size_t>(jmax(256, samplesPerBlockExpected));
    in1.assign(cap, 0.0f); in2.assign(cap, 0.0f); outL.assign(cap, 0.0f); outR.assign(cap, 0.0f);
    sampleRateInt = static_cast<int>(sampleRate);
    engine.prepare(sampleRate);
}

void MainComponent::getNextAudioBlock(const AudioSourceChannelInfo& info) {
    auto* buf = info.buffer;
    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev == nullptr || in1.empty()) { info.clearActiveBufferRegion(); return; }

    const int nIn = dev->getActiveInputChannels().countNumberOfSetBits();
    const int nCh = buf->getNumChannels();
    const int cap = static_cast<int>(in1.size());

    for (int off = 0; off < info.numSamples; off += cap) {
        const int n = jmin(cap, info.numSamples - off);
        const int at = info.startSample + off;
        if (nIn >= 1 && nCh >= 1) FloatVectorOperations::copy(in1.data(), buf->getReadPointer(0, at), n);
        if (nIn >= 2 && nCh >= 2) FloatVectorOperations::copy(in2.data(), buf->getReadPointer(1, at), n);

        engine.processBlock(nIn >= 1 ? in1.data() : nullptr, nIn >= 2 ? in2.data() : nullptr, outL.data(), outR.data(), n);

        for (int ch = 0; ch < nCh; ++ch) {
            if (ch == 0) FloatVectorOperations::copy(buf->getWritePointer(0, at), outL.data(), n);
            else if (ch == 1) FloatVectorOperations::copy(buf->getWritePointer(1, at), outR.data(), n);
            else FloatVectorOperations::clear(buf->getWritePointer(ch, at), n);
        }
        if (nCh == 1) { // saída mono: soma L+R
            auto* o = buf->getWritePointer(0, at);
            for (int i = 0; i < n; ++i) o[i] = 0.5f * (outL[static_cast<size_t>(i)] + outR[static_cast<size_t>(i)]);
        }
    }
}

void MainComponent::onTabChanged(int index) {
    if (index == 0) engine.params.mode = static_cast<int>(nexus::Mode::Module);
    else if (index == 1) engine.params.mode = static_cast<int>(nexus::Mode::Hybrid);
    refreshAll();
}

void MainComponent::refreshAll() {
    if (modulePanel) modulePanel->refresh();
    if (eadPanel) eadPanel->refresh();
}

void MainComponent::timerCallback() {
    modulePanel->tick();
    eadPanel->tick();
    String s;
    if (auto* d = deviceManager.getCurrentAudioDevice()) {
        const double sr = d->getCurrentSampleRate();
        const int ins = d->getActiveInputChannels().countNumberOfSetBits();
        const double ms = sr > 0 ? (d->getCurrentBufferSizeSamples() + d->getInputLatencyInSamples() + d->getOutputLatencyInSamples()) * 1000.0 / sr : 0.0;
        s = String(d->getName()) + String::fromUTF8("  ·  ") + String(roundToInt(sr)) + String::fromUTF8(" Hz  ·  ") + String(ins) + (ins == 1 ? " entrada" : " entradas")
            + String::fromUTF8("  ·  latência ≈ ") + String(ms, 1) + " ms";
        if (ins < 2) s += "  (precisa de 2 entradas para os 2 pads)";
    } else {
        s = String::fromUTF8("Sem dispositivo de áudio - veja a aba Áudio");
    }
    statusLabel.setText(s, dontSendNotification);
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
                             if (f.existsAsFile() && loadPreset(engine.params, f)) refreshAll();
                         });
}

void MainComponent::paint(Graphics& g) {
    g.fillAll(kBg);
    g.setColour(Colours::white);
    g.setFont(Font(FontOptions(18.0f, Font::bold)));
    g.drawText("NEXUS DRUM TRIGGER", 14, 8, 260, 28, Justification::centredLeft);
    g.setColour(kAccent);
    g.fillRect(14, 36, 40, 2);
}

void MainComponent::resized() {
    auto r = getLocalBounds();
    auto top = r.removeFromTop(44).reduced(10, 7);
    top.removeFromLeft(250);
    loadBtn.setBounds(top.removeFromLeft(100)); top.removeFromLeft(6);
    saveBtn.setBounds(top.removeFromLeft(100)); top.removeFromLeft(10);
    statusLabel.setBounds(top);
    tabs->setBounds(r.reduced(6, 0).withTrimmedBottom(6));
}
