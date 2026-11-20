#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "core/Engine.h"

// Tabela única com todos os parâmetros salvos em kit/preset (e na última sessão).
struct ParamEntry {
    juce::String key;
    std::atomic<float>* f = nullptr;
    std::atomic<int>* i = nullptr;
    std::atomic<bool>* b = nullptr;
};

inline std::vector<ParamEntry> makeParamTable(nexus::EngineParams& p) {
    std::vector<ParamEntry> t;
    auto F = [&](const juce::String& k, std::atomic<float>& a) { t.push_back({k, &a, nullptr, nullptr}); };
    auto I = [&](const juce::String& k, std::atomic<int>& a) { t.push_back({k, nullptr, &a, nullptr}); };
    auto B = [&](const juce::String& k, std::atomic<bool>& a) { t.push_back({k, nullptr, nullptr, &a}); };
    for (int n = 0; n < nexus::kNumPads; ++n) {
        const juce::String s = "pad" + juce::String(n + 1) + "_";
        auto& q = p.pad[n];
        I(s + "sound", q.sound); F(s + "level", q.level); F(s + "pitch", q.pitchSemis); F(s + "pan", q.pan);
        F(s + "inGain", q.inputGainDb); F(s + "thr", q.thresholdDb); F(s + "retrig", q.retriggerMs);
        F(s + "hp", q.hpHz); F(s + "lp", q.lpHz); F(s + "dyn", q.dynamics); F(s + "sens", q.velocitySens);
    }
    F("bleed", p.bleedReject); F("master", p.master);
    F("revRoom", p.reverbRoom); F("revDamp", p.reverbDamp); F("revWet", p.reverbWet);
    F("electronic", p.electronicLevel); F("acoustic", p.acousticLevel); F("width", p.stereoWidth);
    F("eqLow", p.eqLowDb); F("eqMid", p.eqMidDb); F("eqHigh", p.eqHighDb);
    F("compThr", p.compThresholdDb); F("compRatio", p.compRatio); F("compAtt", p.compAttackMs);
    F("compRel", p.compReleaseMs); F("compMakeup", p.compMakeupDb); B("compOn", p.compOn);
    return t;
}

inline bool savePreset(nexus::EngineParams& p, const juce::File& file) {
    juce::XmlElement xml("NexusDrumTriggerKit");
    xml.setAttribute("version", 1);
    for (auto& e : makeParamTable(p)) {
        if (e.f) xml.setAttribute(e.key, static_cast<double>(e.f->load()));
        else if (e.i) xml.setAttribute(e.key, e.i->load());
        else if (e.b) xml.setAttribute(e.key, e.b->load() ? 1 : 0);
    }
    file.getParentDirectory().createDirectory();
    return xml.writeTo(file);
}

inline bool loadPreset(nexus::EngineParams& p, const juce::File& file) {
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || xml->getTagName() != "NexusDrumTriggerKit") return false;
    for (auto& e : makeParamTable(p)) {
        if (!xml->hasAttribute(e.key)) continue; // ausente = mantém o valor atual
        if (e.f) e.f->store(static_cast<float>(xml->getDoubleAttribute(e.key)));
        else if (e.i) e.i->store(xml->getIntAttribute(e.key));
        else if (e.b) e.b->store(xml->getIntAttribute(e.key) != 0);
    }
    return true;
}
