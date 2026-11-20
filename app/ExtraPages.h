#pragma once
// Páginas extras: Mixer, Efeitos e MIDI.
// Incluído por MainComponent.cpp depois de PageBase, KnobCard e Widgets.h.

// ============================================================================
//  Mixer: um canal por pad (ou peça do EAD) + microfones + geral
// ============================================================================
class MixerPage : public PageBase {
public:
    MixerPage(nexus::Engine& e, std::function<void()> changed) : engine(e), changedCb(std::move(changed)) {
        for (auto* b : {&modBtn, &eadBtn}) { b->setClickingTogglesState(true); b->setRadioGroupId(303); addAndMakeVisible(*b); }
        modBtn.setButtonText(theme::u8("Bateria eletrônica")); eadBtn.setButtonText("EAD");
        modBtn.onClick = [this] { build(false); };
        eadBtn.onClick = [this] { build(true); };
        hint.setText(theme::u8("Volume e pan de cada peça, mudo (M) e solo (S). O canal “Mics” é o par de microfones do EAD."), juce::dontSendNotification);
        hint.setFont(theme::font(13.0f)); hint.setColour(juce::Label::textColourId, theme::muted);
        addAndMakeVisible(hint);
        build(false);
    }

    void refresh() override { for (auto& s : strips) s->refresh(); }
    void tick() override {
        for (auto& s : strips) s->tick();
    }
    void resized() override {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(40);
        modBtn.setBounds(top.removeFromLeft(190).reduced(2, 2)); eadBtn.setBounds(top.removeFromLeft(110).reduced(2, 2));
        hint.setBounds(top.reduced(14, 0));
        r.removeFromTop(8);
        layoutStrips(r);
    }

private:
    class Strip : public Card {
    public:
        Strip(const juce::String& name, nexus::PadParams* pp, std::function<float()> g, std::function<void(float)> s,
              double maxV, std::function<float()> level, std::function<void()> changed, int hitIdx = -1, bool zone = false)
            : Card(), pad(pp), getLevel(std::move(level)), engineChanged(changed), hit(hitIdx), isZone(zone), getV(std::move(g)), setV(std::move(s)) {
            title.setText(name, juce::dontSendNotification); title.setJustificationType(juce::Justification::centred);
            title.setFont(theme::font(13.0f, true));
            addAndMakeVisible(title);
            fader.setSliderStyle(juce::Slider::LinearVertical);
            fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            fader.setRange(0.0, maxV, 0.01); fader.setDoubleClickReturnValue(true, 1.0);
            fader.onValueChange = [this] { setV(static_cast<float>(fader.getValue())); updateText(); if (engineChanged) engineChanged(); };
            addAndMakeVisible(fader); addAndMakeVisible(meter);
            value.setFont(theme::font(12.0f)); value.setColour(juce::Label::textColourId, theme::muted); value.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(value);
            if (pad) {
                panKnob = std::make_unique<Knob>("Pan", -1, 1, 0.01, "", 0.0, 2);
                panKnob->bind([this] { return pad->pan.load(); }, [this](float v) { pad->pan = v; });
                addAndMakeVisible(*panKnob);
                for (auto* b : {&mute, &solo}) { b->setClickingTogglesState(true); addAndMakeVisible(*b); }
                mute.setButtonText("M"); solo.setButtonText("S");
                mute.onClick = [this] { pad->mute = mute.getToggleState(); if (engineChanged) engineChanged(); };
                solo.onClick = [this] { pad->solo = solo.getToggleState(); if (engineChanged) engineChanged(); };
            }
            refresh();
        }
        void refresh() {
            fader.setValue(getV(), juce::dontSendNotification); updateText();
            if (panKnob) panKnob->refresh();
            if (pad) { mute.setToggleState(pad->mute.load(), juce::dontSendNotification); solo.setToggleState(pad->solo.load(), juce::dontSendNotification); }
        }
        void tick() {
            meter.setLevel(getLevel ? getLevel() : 0.0f);
        }
        void resized() override {
            auto r = getLocalBounds().reduced(6, 8);
            title.setBounds(r.removeFromTop(20));
            if (pad) {
                auto b = r.removeFromBottom(30);
                mute.setBounds(b.removeFromLeft(b.getWidth() / 2).reduced(2, 0)); solo.setBounds(b.reduced(2, 0));
                r.removeFromBottom(4);
                panKnob->setBounds(r.removeFromBottom(74));
            }
            value.setBounds(r.removeFromBottom(18));
            auto m = r.removeFromRight(18); meter.setBounds(m.reduced(2, 4));
            fader.setBounds(r);
        }
        void paint(juce::Graphics& g) override { Card::paint(g); }
        juce::Label title, value;
    private:
        void updateText() { value.setText(juce::String(juce::Decibels::gainToDecibels(static_cast<float>(fader.getValue()), -60.0f), 1) + " dB", juce::dontSendNotification); }
        nexus::PadParams* pad;
        std::function<float()> getLevel;
        std::function<void()> engineChanged;
        int hit; bool isZone;
        std::function<float()> getV; std::function<void(float)> setV;
        juce::Slider fader; SegMeter meter; std::unique_ptr<Knob> panKnob;
        juce::TextButton mute, solo;
    };

    // nível do medidor de cada canal: batida recente (velocity x volume) com decaimento
    std::function<float()> hitLevel(int idx, nexus::PadParams* pp) {
        auto st = std::make_shared<std::pair<unsigned, float>>(0u, 0.0f);
        return [this, idx, pp, st]() {
            const unsigned h = engine.status.hitCount[idx].load();
            if (h != st->first) { st->first = h; st->second = engine.status.lastVelocity[idx].load() * pp->level.load(); }
            st->second *= 0.88f;
            return st->second;
        };
    }

    void build(bool ead) {
        strips.clear();
        modBtn.setToggleState(!ead, juce::dontSendNotification); eadBtn.setToggleState(ead, juce::dontSendNotification);
        auto& p = engine.params;
        static const char* padNames[] = {"Bumbo", "Caixa", "Chimbal", "Tom 1", "Tom 2", "Surdo", "Prato 1", "Prato 2"};
        static const char* zoneNames[] = {"Bumbo", "Caixa", "Chimbal", "Tom 1", "Tom 2", "Prato"};
        const int n = ead ? nexus::kNumZones : nexus::kNumPads;
        for (int i = 0; i < n; ++i) {
            nexus::PadParams* pp = ead ? &p.zone[i] : &p.pad[i];
            auto s = std::make_unique<Strip>(theme::u8(ead ? zoneNames[i] : padNames[i]), pp,
                                             [pp] { return pp->level.load(); }, [pp](float v) { pp->level = v; }, 1.5, hitLevel(i, pp), changedCb, i, ead);
            addAndMakeVisible(*s); strips.push_back(std::move(s));
        }
        if (ead) {
            auto s = std::make_unique<Strip>("Mics", nullptr, [&p] { return p.acousticLevel.load(); }, [&p](float v) { p.acousticLevel = v; }, 1.5,
                                             [this] { return 0.5f * (engine.status.inputLevel[0].load() + engine.status.inputLevel[1].load()); }, changedCb);
            addAndMakeVisible(*s); strips.push_back(std::move(s));
        }
        auto m = std::make_unique<Strip>("Geral", nullptr, [&p] { return p.master.load(); }, [&p](float v) { p.master = v; }, 1.2,
                                         [this] { return engine.status.outputLevel.load(); }, changedCb);
        addAndMakeVisible(*m); strips.push_back(std::move(m));
        resized();
    }
    void layoutStrips(juce::Rectangle<int> r) {
        const int n = static_cast<int>(strips.size());
        if (n == 0) return;
        const int gap = 8, w = juce::jmin(130, (r.getWidth() - gap * (n - 1)) / n);
        int x = r.getX();
        for (auto& s : strips) { s->setBounds(x, r.getY(), w, r.getHeight()); x += w + gap; }
    }

    nexus::Engine& engine;
    std::function<void()> changedCb;
    juce::TextButton modBtn, eadBtn;
    juce::Label hint;
    std::vector<std::unique_ptr<Strip>> strips;
};

// ============================================================================
//  Efeitos: reverb, equalizador, compressor e eco do mix principal
// ============================================================================
class EffectsPage : public PageBase {
public:
    EffectsPage(nexus::Engine& e, std::function<void()> changed)
        : reverb(theme::u8("Reverb (ambiência)")), eq(theme::u8("Equalizador principal"), true),
          comp(theme::u8("Compressor principal"), true), echo(theme::u8("Eco (delay)")), eng(e) {
        auto& p = e.params;
        reverb.add("Sala", 0, 1, 0.01, "", 0.5, 2, [&p] { return p.reverbRoom.load(); }, [&p](float v) { p.reverbRoom = v; }, changed);
        reverb.add("Abafamento", 0, 1, 0.01, "", 0.5, 2, [&p] { return p.reverbDamp.load(); }, [&p](float v) { p.reverbDamp = v; }, changed);
        reverb.add("Quantidade", 0, 1, 0.01, "", 0.0, 2, [&p] { return p.reverbWet.load(); }, [&p](float v) { p.reverbWet = v; }, changed);

        eq.add("Grave", -12, 12, 0.5, " dB", 0.0, 1, [&p] { return p.mEqLowDb.load(); }, [&p](float v) { p.mEqLowDb = v; }, changed);
        eq.add(theme::u8("Médio"), -12, 12, 0.5, " dB", 0.0, 1, [&p] { return p.mEqMidDb.load(); }, [&p](float v) { p.mEqMidDb = v; }, changed);
        eq.add("Agudo", -12, 12, 0.5, " dB", 0.0, 1, [&p] { return p.mEqHighDb.load(); }, [&p](float v) { p.mEqHighDb = v; }, changed);
        eq.power->onClick = [this, changed] { eng.params.mEqOn = eq.power->getToggleState(); changed(); };

        comp.add("Limiar", -48, 0, 0.5, " dB", -12.0, 1, [&p] { return p.mCompThresholdDb.load(); }, [&p](float v) { p.mCompThresholdDb = v; }, changed);
        comp.add("Taxa", 1, 20, 0.1, ":1", 2.0, 1, [&p] { return p.mCompRatio.load(); }, [&p](float v) { p.mCompRatio = v; }, changed);
        comp.add("Ataque", 1, 100, 1, " ms", 10.0, 0, [&p] { return p.mCompAttackMs.load(); }, [&p](float v) { p.mCompAttackMs = v; }, changed);
        comp.add(theme::u8("Liberação"), 20, 800, 1, " ms", 150.0, 0, [&p] { return p.mCompReleaseMs.load(); }, [&p](float v) { p.mCompReleaseMs = v; }, changed);
        comp.add("Ganho", 0, 18, 0.5, " dB", 0.0, 1, [&p] { return p.mCompMakeupDb.load(); }, [&p](float v) { p.mCompMakeupDb = v; }, changed);
        comp.power->onClick = [this, changed] { eng.params.mCompOn = comp.power->getToggleState(); changed(); };

        echo.add("Tempo", 40, 1000, 1, " ms", 320.0, 0, [&p] { return p.delayMs.load(); }, [&p](float v) { p.delayMs = v; }, changed);
        echo.add(theme::u8("Repetições"), 0, 0.9, 0.01, "", 0.35, 2, [&p] { return p.delayFeedback.load(); }, [&p](float v) { p.delayFeedback = v; }, changed);
        echo.add("Mistura", 0, 1, 0.01, "", 0.0, 2, [&p] { return p.delayMix.load(); }, [&p](float v) { p.delayMix = v; }, changed);

        for (auto* c : std::initializer_list<Component*>{&reverb, &eq, &comp, &echo}) addAndMakeVisible(c);
        note.setText(theme::u8("Estes efeitos valem para tudo que sai no mix estéreo (L+R). As saídas separadas por peça saem sem efeitos, para você mixar depois."), juce::dontSendNotification);
        note.setFont(theme::font(13.0f)); note.setColour(juce::Label::textColourId, theme::muted);
        addAndMakeVisible(note);
        refresh();
    }
    void refresh() override {
        reverb.refresh(); eq.refresh(); comp.refresh(); echo.refresh();
        eq.power->setToggleState(eng.params.mEqOn.load(), juce::dontSendNotification);
        comp.power->setToggleState(eng.params.mCompOn.load(), juce::dontSendNotification);
    }
    void resized() override {
        auto r = getLocalBounds();
        note.setBounds(r.removeFromBottom(26));
        auto top = r.removeFromTop((r.getHeight() - 10) / 2); r.removeFromTop(10);
        eq.setBounds(top.removeFromLeft(top.getWidth() / 2 - 5)); top.removeFromLeft(10); comp.setBounds(top);
        reverb.setBounds(r.removeFromLeft(r.getWidth() / 2 - 5)); r.removeFromLeft(10); echo.setBounds(r);
    }
private:
    KnobCard reverb, eq, comp, echo;
    juce::Label note;
    nexus::Engine& eng;
};

// ============================================================================
//  MIDI: entrada (módulo/teclado dispara pads), saída (pads enviam notas) e mapa de notas
// ============================================================================
class MidiPage : public PageBase, private juce::MidiInputCallback, private juce::Thread, private juce::Timer {
public:
    MidiPage(nexus::Engine& e, std::function<void()> changed, juce::File settingsFile)
        : juce::Thread("MIDI out"), engine(e), changedCb(std::move(changed)), settings(std::move(settingsFile)),
          inCard(theme::u8("Entrada MIDI")), outCard(theme::u8("Saída MIDI")), mapCard(theme::u8("Notas de cada peça")) {
        for (auto* c : std::initializer_list<Component*>{&inCard, &outCard, &mapCard}) addAndMakeVisible(c);
        inCard.addAndMakeVisible(inBox); inCard.addAndMakeVisible(inInfo); inCard.addAndMakeVisible(rescan);
        outCard.addAndMakeVisible(outBox); outCard.addAndMakeVisible(outOn); outCard.addAndMakeVisible(chBox); outCard.addAndMakeVisible(outInfo);
        rescan.setButtonText("Procurar dispositivos");
        rescan.onClick = [this] { fillDevices(); };
        inInfo.setFont(theme::font(13.0f)); inInfo.setColour(juce::Label::textColourId, theme::muted);
        outInfo.setFont(theme::font(13.0f)); outInfo.setColour(juce::Label::textColourId, theme::muted);
        inInfo.setText(theme::u8("Ligue um módulo/pad MIDI: cada nota toca o pad (ou a peça do EAD) com a força da tecla."), juce::dontSendNotification);
        inInfo.setJustificationType(juce::Justification::topLeft); outInfo.setJustificationType(juce::Justification::topLeft);
        outInfo.setText(theme::u8("Cada batida detectada envia uma nota MIDI (para tocar outro programa/VST ao mesmo tempo)."), juce::dontSendNotification);
        outOn.setButtonText(theme::u8("Enviar notas MIDI"));
        outOn.setClickingTogglesState(true);
        outOn.onClick = [this] { engine.params.midiOutOn = outOn.getToggleState(); changedCb(); };
        for (int c = 1; c <= 16; ++c) chBox.addItem("Canal " + juce::String(c), c);
        chBox.onChange = [this] { engine.params.midiOutChannel = chBox.getSelectedId(); changedCb(); };
        inBox.onChange = [this] { openInput(); saveSettings(); };
        outBox.onChange = [this] { openOutput(); saveSettings(); };

        static const char* padNames[] = {"Bumbo", "Caixa", "Chimbal", "Tom 1", "Tom 2", "Surdo", "Prato 1", "Prato 2"};
        static const char* zoneNames[] = {"Bumbo", "Caixa", "Chimbal", "Tom 1", "Tom 2", "Prato"};
        for (int i = 0; i < 14; ++i) {
            auto row = std::make_unique<Row>();
            const bool zone = i >= 8; const int idx = zone ? i - 8 : i;
            row->zone = zone; row->idx = idx;
            row->name.setText(juce::String(zone ? "EAD - " : "Bateria - ") + theme::u8(zone ? zoneNames[idx] : padNames[idx]), juce::dontSendNotification);
            row->name.setFont(theme::font(13.5f)); row->name.setColour(juce::Label::textColourId, theme::text);
            row->note.setSliderStyle(juce::Slider::IncDecButtons);
            row->note.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 90, 26);
            row->note.setRange(0, 127, 1);
            row->note.textFromValueFunction = [](double v) { return noteName(static_cast<int>(v)); };
            row->note.valueFromTextFunction = [](const juce::String& t) { return static_cast<double>(t.getIntValue()); };
            auto* rp = row.get();
            row->note.onValueChange = [this, rp] {
                (rp->zone ? engine.params.midiZoneNote[rp->idx] : engine.params.midiNote[rp->idx]) = static_cast<int>(rp->note.getValue());
                changedCb();
            };
            row->learn.setButtonText("Aprender");
            row->learn.onClick = [this, rp] { learnRow = (learnRow == rp) ? nullptr : rp; };
            mapCard.addAndMakeVisible(row->name); mapCard.addAndMakeVisible(row->note); mapCard.addAndMakeVisible(row->learn);
            rows.push_back(std::move(row));
        }
        resetBtn.setButtonText(theme::u8("Padrão General MIDI"));
        resetBtn.onClick = [this] {
            static const int pn[8] = {36, 38, 42, 48, 45, 41, 49, 51}, zn[6] = {36, 38, 42, 48, 45, 49};
            for (int i = 0; i < 8; ++i) engine.params.midiNote[i] = pn[i];
            for (int i = 0; i < 6; ++i) engine.params.midiZoneNote[i] = zn[i];
            refresh(); changedCb();
        };
        mapCard.addAndMakeVisible(resetBtn);
        loadSettings();
        fillDevices();
        startTimerHz(20);
        startThread();
        refresh();
    }
    ~MidiPage() override {
        stopTimer(); signalThreadShouldExit(); stopThread(500);
        if (input) { input->stop(); input.reset(); }
        output.reset();
    }

    void refresh() override {
        for (auto& r : rows) r->note.setValue((r->zone ? engine.params.midiZoneNote[r->idx] : engine.params.midiNote[r->idx]).load(), juce::dontSendNotification);
        outOn.setToggleState(engine.params.midiOutOn.load(), juce::dontSendNotification);
        chBox.setSelectedId(juce::jlimit(1, 16, engine.params.midiOutChannel.load()), juce::dontSendNotification);
    }
    void tick() override {}

    void resized() override {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(210); r.removeFromTop(10);
        inCard.setBounds(top.removeFromLeft(top.getWidth() / 2 - 5)); top.removeFromLeft(10); outCard.setBounds(top);
        mapCard.setBounds(r);
        { auto c = inCard.content(); inBox.setBounds(c.removeFromTop(32)); c.removeFromTop(8); rescan.setBounds(c.removeFromTop(32).withWidth(200)); c.removeFromTop(8); inInfo.setBounds(c); }
        { auto c = outCard.content(); outBox.setBounds(c.removeFromTop(32)); c.removeFromTop(8);
          auto row = c.removeFromTop(32); outOn.setBounds(row.removeFromLeft(180)); row.removeFromLeft(8); chBox.setBounds(row.removeFromLeft(130)); c.removeFromTop(8); outInfo.setBounds(c); }
        { auto c = mapCard.content(); resetBtn.setBounds(c.removeFromBottom(32).withWidth(220)); c.removeFromBottom(6);
          const int colW = c.getWidth() / 2, rowH = c.getHeight() / 7;
          for (size_t i = 0; i < rows.size(); ++i) {
              const int col = static_cast<int>(i) / 7, rr = static_cast<int>(i) % 7;
              auto cell = juce::Rectangle<int>(c.getX() + col * colW, c.getY() + rr * rowH, colW - 12, rowH).reduced(0, 3);
              rows[i]->name.setBounds(cell.removeFromLeft(130)); rows[i]->note.setBounds(cell.removeFromLeft(160));
              cell.removeFromLeft(8); rows[i]->learn.setBounds(cell.removeFromLeft(90));
          } }
    }

private:
    struct Row { bool zone = false; int idx = 0; juce::Label name; juce::Slider note; juce::TextButton learn; };

    static juce::String noteName(int n) {
        static const char* nm[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        return juce::String(nm[n % 12]) + juce::String(n / 12 - 2) + "  (" + juce::String(n) + ")";
    }

    void fillDevices() {
        inDevices = juce::MidiInput::getAvailableDevices();
        outDevices = juce::MidiOutput::getAvailableDevices();
        inBox.clear(juce::dontSendNotification); outBox.clear(juce::dontSendNotification);
        inBox.addItem("(nenhuma)", 1); outBox.addItem("(nenhuma)", 1);
        for (int i = 0; i < inDevices.size(); ++i) inBox.addItem(inDevices[i].name, i + 2);
        for (int i = 0; i < outDevices.size(); ++i) outBox.addItem(outDevices[i].name, i + 2);
        int si = 1, so = 1;
        for (int i = 0; i < inDevices.size(); ++i) if (inDevices[i].identifier == savedIn) si = i + 2;
        for (int i = 0; i < outDevices.size(); ++i) if (outDevices[i].identifier == savedOut) so = i + 2;
        inBox.setSelectedId(si, juce::dontSendNotification); outBox.setSelectedId(so, juce::dontSendNotification);
        openInput(); openOutput();
    }
    void openInput() {
        if (input) { input->stop(); input.reset(); }
        const int id = inBox.getSelectedId();
        if (id >= 2 && id - 2 < inDevices.size()) { input = juce::MidiInput::openDevice(inDevices[id - 2].identifier, this); if (input) input->start(); }
    }
    void openOutput() {
        const juce::ScopedLock sl(outLock);
        output.reset();
        const int id = outBox.getSelectedId();
        if (id >= 2 && id - 2 < outDevices.size()) output = juce::MidiOutput::openDevice(outDevices[id - 2].identifier);
    }
    void saveSettings() {
        juce::XmlElement x("MidiSettings");
        const int i = inBox.getSelectedId(), o = outBox.getSelectedId();
        x.setAttribute("in", i >= 2 && i - 2 < inDevices.size() ? inDevices[i - 2].identifier : juce::String());
        x.setAttribute("out", o >= 2 && o - 2 < outDevices.size() ? outDevices[o - 2].identifier : juce::String());
        settings.getParentDirectory().createDirectory(); x.writeTo(settings);
    }
    void loadSettings() {
        if (auto x = juce::XmlDocument::parse(settings)) { savedIn = x->getStringAttribute("in"); savedOut = x->getStringAttribute("out"); }
    }

    // entrada: nota -> pad (ou peça do EAD, conforme a aba ativa)
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m) override {
        if (!m.isNoteOn()) return;
        const int note = m.getNoteNumber();
        if (Row* lr = learnRow.load()) {
            (lr->zone ? engine.params.midiZoneNote[lr->idx] : engine.params.midiNote[lr->idx]) = note;
            learnRow = nullptr; learnedFlag = true; return;
        }
        const int vel = juce::jmax(1, static_cast<int>(m.getFloatVelocity() * 1000.0f));
        if (engine.params.mode.load() == static_cast<int>(nexus::Mode::Hybrid)) {
            for (int z = 0; z < nexus::kNumZones; ++z) if (engine.params.midiZoneNote[z].load() == note) engine.params.manualZone[z] = vel;
        } else {
            for (int p = 0; p < nexus::kNumPads; ++p) if (engine.params.midiNote[p].load() == note) engine.params.manualHit[p] = vel;
        }
    }

    // saída: thread própria envia as notas das batidas detectadas
    void run() override {
        struct Off { double at; int ch, note; };
        std::vector<Off> offs;
        while (!threadShouldExit()) {
            nexus::EngineStatus::HitEvent ev;
            const bool on = engine.params.midiOutOn.load();
            while (engine.status.popHit(ev)) {
                if (!on) continue;
                const int note = ev.zone ? engine.params.midiZoneNote[ev.idx].load() : engine.params.midiNote[ev.idx].load();
                const int ch = juce::jlimit(1, 16, engine.params.midiOutChannel.load());
                const int vel = juce::jlimit(1, 127, static_cast<int>(ev.vel * 127.0f + 0.5f));
                const juce::ScopedLock sl(outLock);
                if (output) { output->sendMessageNow(juce::MidiMessage::noteOn(ch, note, static_cast<juce::uint8>(vel))); offs.push_back({juce::Time::getMillisecondCounterHiRes() + 60.0, ch, note}); }
            }
            const double now = juce::Time::getMillisecondCounterHiRes();
            for (size_t i = 0; i < offs.size();) {
                if (offs[i].at <= now) { const juce::ScopedLock sl(outLock); if (output) output->sendMessageNow(juce::MidiMessage::noteOff(offs[i].ch, offs[i].note)); offs.erase(offs.begin() + static_cast<long>(i)); }
                else ++i;
            }
            wait(1);
        }
    }

    void timerCallback() override {
        if (learnedFlag) { learnedFlag = false; refresh(); changedCb(); }
        for (auto& r : rows) r->learn.setButtonText(learnRow == r.get() ? theme::u8("Toque…") : juce::String("Aprender"));
    }

    nexus::Engine& engine;
    std::function<void()> changedCb;
    juce::File settings;
    Card inCard, outCard, mapCard;
    juce::ComboBox inBox, outBox, chBox;
    juce::TextButton rescan, resetBtn;
    juce::ToggleButton outOn;
    juce::Label inInfo, outInfo;
    juce::Array<juce::MidiDeviceInfo> inDevices, outDevices;
    juce::String savedIn, savedOut;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::CriticalSection outLock;
    std::vector<std::unique_ptr<Row>> rows;
    std::atomic<Row*> learnRow{nullptr};
    std::atomic<bool> learnedFlag{false};
};
