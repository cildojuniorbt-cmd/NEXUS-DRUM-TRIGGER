#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>
#include "core/Engine.h"
#include "Theme.h"

struct PageBase;

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
    class TriggerPage; class EadPage; class AudioPage; class LookPage; class NavButton;
    void timerCallback() override;
    void showPage(int index);
    void refreshAll();
    void applyKit(int index);
    int findSound(const std::string& name) const;
    void stepKit(int delta);
    void showKitMenu();
    void applyTheme();
    void saveKit();
    void loadKit();
    juce::File dataDir() const;
    void loadCustomSounds();
    // sons de fábrica gravados (decodificados uma vez; o banco só guarda referência)
    struct FactorySound { std::string name, cat; int choke = 0; std::shared_ptr<const nexus::Sound::Layers> layers; double rate = 44100.0; };
    std::vector<FactorySound> factory;
    void decodeFactorySounds();
    void addFactorySounds();
    static juce::File factoryDir();
    void importSounds();
    int loadWavInto(const juce::File& f);
    double currentFs = 48000.0;
    int modulePads = 4;

    NexusLnF lnf;
    nexus::Engine engine;
    std::vector<std::vector<float>> inBuf;
    const float* inPtr[nexus::kMaxInputs] = {};
    std::vector<float> outL, outR;
    std::vector<std::vector<float>> outX;

    std::unique_ptr<PageBase> pages[8];
    int currentPage = 0, currentKit = -1;
    std::unique_ptr<NavButton> nav[8];
    juce::TextButton kitBtn, kitPrev, kitNext, gearBtn;
    std::shared_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> kitRect, statusRect;
    double cpu = 0.0, latencyMs = 0.0;
    juce::String deviceInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
