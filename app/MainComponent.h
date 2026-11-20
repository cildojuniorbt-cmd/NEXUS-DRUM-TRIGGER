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
    void stepKit(int delta);
    void showKitMenu();
    void applyTheme();
    void saveKit();
    void loadKit();
    juce::File dataDir() const;

    NexusLnF lnf;
    nexus::Engine engine;
    std::vector<float> in1, in2, outL, outR;

    std::unique_ptr<PageBase> pages[4];
    int currentPage = 0, currentKit = -1;
    std::unique_ptr<NavButton> nav[4];
    juce::TextButton kitBtn, kitPrev, kitNext, gearBtn;
    std::shared_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> kitRect, statusRect;
    double cpu = 0.0, latencyMs = 0.0;
    juce::String deviceInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
