#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>
#include "core/Engine.h"

// Slider com nome à esquerda.
class ParamSlider : public juce::Component {
public:
    ParamSlider(const juce::String& name, double mn, double mx, double step, const juce::String& suffix, double skewMid = 0.0);
    void setValue(double v) { slider.setValue(v, juce::dontSendNotification); }
    void resized() override;
    std::function<void(float)> onChange;
    juce::Label label;
    juce::Slider slider;
};

// Barra de nível (dB).
class MeterBar : public juce::Component {
public:
    void setLevel(float lin);
    void paint(juce::Graphics& g) override;
private:
    float shown = 0.0f;
};

// Um pad = um microfone + um som. 'compact' mostra só o essencial (usado na aba EAD).
class PadStrip : public juce::Component {
public:
    PadStrip(int index, nexus::Engine& engine, bool compact, std::function<void()> onUserChange);
    void refresh();   // lê os parâmetros do motor e atualiza os controles
    void tick();      // chamado pelo timer da interface (medidores e luz de batida)
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void addRow(ParamSlider& s, bool showInCompact);
    int index;
    nexus::Engine& engine;
    bool compact;
    std::function<void()> changed;

    juce::Label title, velLabel;
    juce::ComboBox soundBox;
    juce::TextButton testBtn { "Testar" };
    MeterBar meter;
    ParamSlider level, pitch, pan, inGain, thr, retrig, hp, lp, dyn, sens;
    std::vector<ParamSlider*> rows;

    unsigned lastHits = 0;
    float flash = 0.0f;
};

class MainComponent : public juce::AudioAppComponent, private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;
    void releaseResources() override {}

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ModulePanel; class EadPanel; class AudioPanel;
    void timerCallback() override;
    void onTabChanged(int index);
    void refreshAll();
    void saveKit();
    void loadKit();
    juce::File dataDir() const;

    nexus::Engine engine;
    std::vector<float> in1, in2, outL, outR;

    std::unique_ptr<juce::TabbedComponent> tabs;
    ModulePanel* modulePanel = nullptr;
    EadPanel* eadPanel = nullptr;
    AudioPanel* audioPanel = nullptr;
    juce::TextButton saveBtn { "Salvar kit" }, loadBtn { "Abrir kit" };
    std::shared_ptr<juce::FileChooser> chooser;
    juce::Label statusLabel;
    int sampleRateInt = 48000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
