#pragma once
#include "Fx.h"
#include "Sounds.h"
#include "Trigger.h"
#include <array>
#include <atomic>
#include <cmath>

namespace nexus {

constexpr int kNumPads = 8;    // módulo virtual: até 8 pads, cada um ligado a uma entrada da interface
constexpr int kNumZones = 3;   // EAD: bumbo / caixa / chimbal separados por faixa de frequência
constexpr int kMaxInputs = 8;

enum class Mode : int { Module = 0, Hybrid = 1 };

// Parâmetros editáveis em tempo real (a interface escreve, o áudio lê).
struct PadParams {
    std::atomic<int> sound{0};
    std::atomic<int> inputChannel{0};        // entrada da interface (0 = entrada 1) - só no módulo virtual
    std::atomic<float> level{1.0f};          // 0..1.5
    std::atomic<float> pitchSemis{0.0f};     // -12..+12
    std::atomic<float> pan{0.0f};            // -1..+1
    std::atomic<float> inputGainDb{0.0f};
    std::atomic<float> thresholdDb{-36.0f};
    std::atomic<float> retriggerMs{70.0f};
    std::atomic<float> hpHz{60.0f};
    std::atomic<float> lpHz{6000.0f};
    std::atomic<float> dynamics{1.0f};       // curva de velocity
    std::atomic<float> velocitySens{1.0f};   // 0 = volume fixo, 1 = totalmente dinâmico

    void setup(int snd, int ch, float thr, float hp, float lp, float retrig, float pn) {
        sound = snd; inputChannel = ch; thresholdDb = thr; hpHz = hp; lpHz = lp; retriggerMs = retrig; pan = pn;
    }
};

struct EngineParams {
    PadParams pad[kNumPads];     // módulo virtual
    PadParams zone[kNumZones];   // EAD (par estéreo -> zonas por frequência)
    std::atomic<int> activePads{2};
    std::atomic<int> mode{static_cast<int>(Mode::Module)};
    std::atomic<float> bleedReject{0.0f};    // 0..1 (rejeição de vazamento entre microfones / faixas)
    std::atomic<float> master{0.9f};

    // Reverb (vale para os dois modos)
    std::atomic<float> reverbRoom{0.5f}, reverbDamp{0.5f}, reverbWet{0.0f};

    // EAD: acústico + eletrônico
    std::atomic<float> acousticLevel{0.8f};   // nível do par estéreo de microfones (Mic 1 = E, Mic 2 = D)
    std::atomic<float> stereoWidth{1.0f};     // 0 = mono, 1 = estéreo natural, 1.5 = mais aberto
    std::atomic<float> electronicLevel{1.0f};
    std::atomic<float> eqLowDb{0.0f}, eqMidDb{0.0f}, eqHighDb{0.0f};
    std::atomic<float> compThresholdDb{-18.0f}, compRatio{3.0f}, compAttackMs{8.0f}, compReleaseMs{120.0f}, compMakeupDb{0.0f};
    std::atomic<bool> compOn{true};

    // Comandos vindos da interface
    std::atomic<int> manualHit[kNumPads] = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}};  // velocity*1000 (0 = nenhum)
    std::atomic<int> manualZone[kNumZones] = {{0}, {0}, {0}};
    std::atomic<int> previewSound{-1};

    EngineParams() {
        // módulo virtual: bumbo, caixa, chimbal, tom 1, tom 2, surdo, prato 1, prato 2
        pad[0].setup(0, 0, -30.0f, 40.0f, 1500.0f, 70.0f, 0.0f);
        pad[1].setup(3, 1, -36.0f, 120.0f, 6000.0f, 70.0f, 0.0f);
        pad[2].setup(8, 2, -36.0f, 3000.0f, 16000.0f, 40.0f, 0.25f);
        pad[3].setup(10, 3, -36.0f, 60.0f, 4000.0f, 60.0f, -0.2f);
        pad[4].setup(11, 4, -36.0f, 60.0f, 4000.0f, 60.0f, 0.2f);
        pad[5].setup(12, 5, -36.0f, 40.0f, 3000.0f, 60.0f, 0.4f);
        pad[6].setup(13, 6, -36.0f, 2000.0f, 16000.0f, 120.0f, -0.4f);
        pad[7].setup(15, 7, -36.0f, 2000.0f, 16000.0f, 120.0f, 0.5f);
        // EAD: faixas de frequência do par estéreo
        zone[0].setup(0, 0, -30.0f, 30.0f, 150.0f, 70.0f, 0.0f);      // bumbo = graves
        zone[1].setup(3, 0, -32.0f, 800.0f, 4000.0f, 70.0f, 0.0f);    // caixa = médios
        zone[2].setup(8, 0, -30.0f, 7000.0f, 16000.0f, 40.0f, 0.25f); // chimbal = agudos
    }
};

// Medidores e eventos para a interface.
struct EngineStatus {
    std::atomic<float> inputLevel[kMaxInputs] = {{0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}};
    std::atomic<float> lastVelocity[kNumPads] = {{0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}};
    std::atomic<unsigned> hitCount[kNumPads] = {{0u}, {0u}, {0u}, {0u}, {0u}, {0u}, {0u}, {0u}};
    std::atomic<float> outputLevel{0.0f};
    std::atomic<float> gainReductionDb{0.0f};
};

class Engine {
public:
    EngineParams params;
    EngineStatus status;

    void prepare(double sampleRate) {
        fs = sampleRate;
        bank.build(fs);
        for (int i = 0; i < kNumPads; ++i) { det[i].prepare(fs); det[i].setRecentWindowMs(12.0f); }
        for (int i = 0; i < kNumZones; ++i) { zdet[i].prepare(fs); zdet[i].setRecentWindowMs(12.0f); }
        eq.prepare(fs); comp.prepare(fs); reverb.prepare(fs);
        for (auto& v : voices) v = Voice{};
        voiceCounter = 0;
        outMeter = 0.0f;
    }

    const SoundBank& sounds() const { return bank; }
    SoundBank& soundBank() { return bank; }

    // inputs: um ponteiro por canal de entrada da interface (nIn canais). Saída estéreo.
    void processBlock(const float* const* inputs, int nIn, float* outL, float* outR, int n) {
        const Mode mode = static_cast<Mode>(params.mode.load(std::memory_order_relaxed));
        const bool hybrid = mode == Mode::Hybrid;
        nIn = std::min(nIn, kMaxInputs);

        const int active = std::min(kNumPads, std::max(1, params.activePads.load()));
        int chan[kNumPads];
        for (int p = 0; p < kNumPads; ++p) {
            chan[p] = std::min(kMaxInputs - 1, std::max(0, params.pad[p].inputChannel.load()));
            det[p].configure(settingsOf(params.pad[p]));
        }
        for (int z = 0; z < kNumZones; ++z) zdet[z].configure(settingsOf(params.zone[z]));

        const float bleed = params.bleedReject.load();
        eq.set(params.eqLowDb.load(), params.eqMidDb.load(), params.eqHighDb.load());
        comp.set(params.compThresholdDb.load(), params.compRatio.load(), params.compAttackMs.load(),
                 params.compReleaseMs.load(), params.compMakeupDb.load());
        reverb.set(params.reverbRoom.load(), params.reverbDamp.load(), params.reverbWet.load());
        const bool compOn = params.compOn.load();
        const float elec = params.electronicLevel.load();
        const float master = params.master.load();
        const float acLvl = hybrid ? params.acousticLevel.load() : 0.0f;
        const float width = params.stereoWidth.load();

        // comandos da interface
        for (int p = 0; p < kNumPads; ++p) {
            const int m = params.manualHit[p].exchange(0);
            if (m > 0) fire(params.pad[p], p, static_cast<float>(m) / 1000.0f);
        }
        for (int z = 0; z < kNumZones; ++z) {
            const int m = params.manualZone[z].exchange(0);
            if (m > 0) fire(params.zone[z], z, static_cast<float>(m) / 1000.0f);
        }
        const int pv = params.previewSound.exchange(-1);
        if (pv >= 0) startVoice(pv, 0.9f, 1.0f, 0.0f);

        float inPeak[kMaxInputs] = {0, 0, 0, 0, 0, 0, 0, 0};
        float gr = 0.0f;
        for (int i = 0; i < n; ++i) {
            float x[kMaxInputs];
            for (int c = 0; c < kMaxInputs; ++c) {
                x[c] = (c < nIn && inputs[c]) ? inputs[c][i] : 0.0f;
                inPeak[c] = std::max(inPeak[c], std::fabs(x[c]));
            }

            // 1) detecção
            if (hybrid) {
                // EAD: os 2 microfones viram um par estéreo; o som todo (soma) é separado por faixa de frequência
                const float m = nIn >= 2 ? 0.5f * (x[0] + x[1]) : x[0];
                for (int z = 0; z < kNumZones; ++z) {
                    float others = 0.0f;
                    if (bleed > 0.0f) for (int q = 0; q < kNumZones; ++q) if (q != z) others = std::max(others, zdet[q].recentLevel());
                    const TriggerEvent ev = zdet[z].process(m, others * bleed * 0.2f);
                    if (ev.fired) fire(params.zone[z], z, ev.velocity);
                }
            } else {
                // Módulo virtual: cada pad escuta a sua entrada (com rejeição de vazamento entre os microfones)
                for (int p = 0; p < active; ++p) {
                    float others = 0.0f;
                    if (bleed > 0.0f) for (int q = 0; q < active; ++q) if (q != p) others = std::max(others, det[q].recentLevel());
                    const TriggerEvent ev = det[p].process(x[chan[p]], others * bleed * 0.5f);
                    if (ev.fired) fire(params.pad[p], p, ev.velocity);
                }
            }

            // 2) sons eletrônicos
            float eL = 0.0f, eR = 0.0f;
            renderVoices(eL, eR);
            eL *= elec; eR *= elec;

            // 3) microfones acústicos (só na aba EAD): Mic 1 = esquerda, Mic 2 = direita
            float aL = 0.0f, aR = 0.0f;
            if (hybrid) {
                const float l = x[0] * acLvl;
                const float r = (nIn >= 2 ? x[1] : x[0]) * acLvl;
                const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * width;
                aL = mid + side; aR = mid - side;
                eq.process(aL, aR);
                if (compOn) { comp.process(aL, aR); gr = comp.gainReductionDb(); }
            }

            float L = aL + eL, R = aR + eR;

            // 4) reverb (ambiência)
            float wL, wR;
            reverb.process(L, R, wL, wR);
            L = softLimit((L + wL) * master);
            R = softLimit((R + wR) * master);
            outL[i] = L; outR[i] = R;
            outMeter = std::max(outMeter * 0.9995f, std::max(std::fabs(L), std::fabs(R)));
        }

        for (int c = 0; c < kMaxInputs; ++c) status.inputLevel[c].store(inPeak[c], std::memory_order_relaxed);
        status.outputLevel.store(outMeter, std::memory_order_relaxed);
        status.gainReductionDb.store(gr, std::memory_order_relaxed);
    }

private:
    static TriggerDetector::Settings settingsOf(const PadParams& pp) {
        TriggerDetector::Settings s;
        s.inputGainDb = pp.inputGainDb.load(); s.thresholdDb = pp.thresholdDb.load();
        s.retriggerMs = pp.retriggerMs.load(); s.hpHz = pp.hpHz.load(); s.lpHz = pp.lpHz.load();
        s.dynamics = pp.dynamics.load();
        return s;
    }

    struct Voice {
        const std::vector<float>* data = nullptr;
        double pos = 0.0, inc = 1.0;
        float gainL = 0.0f, gainR = 0.0f;
        int choke = 0;
        float fade = 1.0f, fadeStep = 0.0f; // fadeStep>0: em fade-out
        unsigned long long age = 0;
        bool active = false;
    };

    void fire(PadParams& pp, int statusIdx, float velocity) {
        const float sens = pp.velocitySens.load();
        const float vel = 1.0f - sens + sens * velocity; // sens=0 -> sempre 1.0
        const float pitch = std::pow(2.0f, pp.pitchSemis.load() / 12.0f);
        startVoice(pp.sound.load(), vel * pp.level.load(), pitch, pp.pan.load());
        status.lastVelocity[statusIdx].store(velocity, std::memory_order_relaxed);
        status.hitCount[statusIdx].fetch_add(1u, std::memory_order_relaxed);
    }

    void startVoice(int soundIdx, float gain, float pitch, float pan) {
        if (bank.size() == 0) return;
        const Sound& s = bank.get(soundIdx);
        if (s.chokeGroup > 0) // chimbal: novo golpe corta o anterior
            for (auto& v : voices) if (v.active && v.choke == s.chokeGroup && v.fadeStep == 0.0f) v.fadeStep = static_cast<float>(1.0 / (fs * 0.006));
        Voice* slot = nullptr;
        for (auto& v : voices) if (!v.active) { slot = &v; break; }
        if (!slot) { slot = &voices[0]; for (auto& v : voices) if (v.age < slot->age) slot = &v; } // rouba a mais antiga
        const float p = std::min(1.0f, std::max(-1.0f, pan));
        const float ang = (p + 1.0f) * 0.25f * static_cast<float>(kPi);
        slot->data = &s.data; slot->pos = 0.0; slot->inc = pitch;
        slot->gainL = gain * std::cos(ang) * 1.414f; slot->gainR = gain * std::sin(ang) * 1.414f;
        slot->choke = s.chokeGroup; slot->fade = 1.0f; slot->fadeStep = 0.0f; slot->age = ++voiceCounter; slot->active = true;
    }

    inline void renderVoices(float& L, float& R) {
        for (auto& v : voices) {
            if (!v.active) continue;
            const auto& d = *v.data;
            const size_t i0 = static_cast<size_t>(v.pos);
            if (i0 + 1 >= d.size()) { v.active = false; continue; }
            const float fr = static_cast<float>(v.pos - static_cast<double>(i0));
            const float s = (d[i0] * (1.0f - fr) + d[i0 + 1] * fr) * v.fade;
            L += s * v.gainL; R += s * v.gainR;
            v.pos += v.inc;
            if (v.fadeStep > 0.0f) { v.fade -= v.fadeStep; if (v.fade <= 0.0f) v.active = false; }
        }
    }

    double fs = 48000.0;
    SoundBank bank;
    TriggerDetector det[kNumPads], zdet[kNumZones];
    Eq3 eq; Compressor comp; Reverb reverb;
    std::array<Voice, 32> voices{};
    unsigned long long voiceCounter = 0;
    float outMeter = 0.0f;
};

} // namespace nexus
