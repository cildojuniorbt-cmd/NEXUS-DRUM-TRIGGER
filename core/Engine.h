#pragma once
#include "Fx.h"
#include "Sounds.h"
#include "Trigger.h"
#include <array>
#include <atomic>
#include <cmath>

namespace nexus {

constexpr int kNumPads = 2;

enum class Mode : int { Module = 0, Hybrid = 1 };

// Parâmetros editáveis em tempo real (a interface escreve, o áudio lê).
struct PadParams {
    std::atomic<int> sound{0};
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
};

struct EngineParams {
    PadParams pad[kNumPads];
    std::atomic<int> mode{static_cast<int>(Mode::Module)};
    std::atomic<float> bleedReject{0.0f};    // 0..1 (rejeição de vazamento entre os microfones)
    std::atomic<float> master{0.9f};

    // Reverb (vale para os dois modos)
    std::atomic<float> reverbRoom{0.5f}, reverbDamp{0.5f}, reverbWet{0.0f};

    // Aba EAD (híbrido): acústico + eletrônico
    std::atomic<float> acousticLevel{0.8f};   // nível do par estéreo de microfones (Mic 1 = E, Mic 2 = D)
    std::atomic<float> stereoWidth{1.0f};     // 0 = mono, 1 = estéreo natural, 1.5 = mais aberto
    std::atomic<float> electronicLevel{1.0f};                       // nível dos sons eletrônicos
    std::atomic<float> eqLowDb{0.0f}, eqMidDb{0.0f}, eqHighDb{0.0f};
    std::atomic<float> compThresholdDb{-18.0f}, compRatio{3.0f}, compAttackMs{8.0f}, compReleaseMs{120.0f}, compMakeupDb{0.0f};
    std::atomic<bool> compOn{true};

    // Comandos vindos da interface
    std::atomic<int> manualHit[kNumPads] = {{0}, {0}};  // velocity*1000 (0 = nenhum)
    std::atomic<int> previewSound{-1};                  // audição de um som
};

// Medidores e eventos para a interface.
struct EngineStatus {
    std::atomic<float> inputLevel[kNumPads] = {{0.0f}, {0.0f}};
    std::atomic<float> lastVelocity[kNumPads] = {{0.0f}, {0.0f}};
    std::atomic<unsigned> hitCount[kNumPads] = {{0u}, {0u}};
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
        eq.prepare(fs); comp.prepare(fs); reverb.prepare(fs);
        for (auto& v : voices) v = Voice{};
        voiceCounter = 0;
        outMeter = 0.0f;
    }

    const SoundBank& sounds() const { return bank; }

    // in1/in2: um buffer por microfone (in2 pode ser nulo = silêncio). Saída estéreo.
    void processBlock(const float* in1, const float* in2, float* outL, float* outR, int n) {
        const Mode mode = static_cast<Mode>(params.mode.load(std::memory_order_relaxed));
        const bool hybrid = mode == Mode::Hybrid;
        const float* in[kNumPads] = {in1, in2};

        // atualiza configurações (uma vez por bloco)
        for (int p = 0; p < kNumPads; ++p) {
            const PadParams& pp = params.pad[p];
            TriggerDetector::Settings s;
            s.inputGainDb = pp.inputGainDb.load(); s.thresholdDb = pp.thresholdDb.load();
            s.retriggerMs = pp.retriggerMs.load(); s.hpHz = pp.hpHz.load(); s.lpHz = pp.lpHz.load();
            s.dynamics = pp.dynamics.load();
            det[p].configure(s);
        }
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
            if (m > 0) fire(p, static_cast<float>(m) / 1000.0f);
        }
        const int pv = params.previewSound.exchange(-1);
        if (pv >= 0) startVoice(pv, 0.9f, 1.0f, 0.0f, 1.0f);

        float inPeak[kNumPads] = {0.0f, 0.0f};
        float gr = 0.0f;
        for (int i = 0; i < n; ++i) {
            float x[kNumPads];
            for (int p = 0; p < kNumPads; ++p) { x[p] = in[p] ? in[p][i] : 0.0f; inPeak[p] = std::max(inPeak[p], std::fabs(x[p])); }

            // 1) detecção
            if (hybrid) {
                // EAD: os dois microfones formam UM par estéreo; o gatilho ouve a soma dos dois (pad 1)
                const float m = in[1] ? 0.5f * (x[0] + x[1]) : x[0];
                const TriggerEvent ev = det[0].process(m);
                if (ev.fired) fire(0, ev.velocity);
                det[1].process(0.0f);
            } else {
                // Módulo: cada microfone dispara o seu pad (com rejeição de vazamento entre eles)
                for (int p = 0; p < kNumPads; ++p) {
                    const int other = 1 - p;
                    const float rej = bleed > 0.0f ? det[other].recentLevel() * (bleed * 0.5f) : 0.0f;
                    const TriggerEvent ev = det[p].process(x[p], rej);
                    if (ev.fired) fire(p, ev.velocity);
                }
            }

            // 2) sons eletrônicos
            float eL = 0.0f, eR = 0.0f;
            renderVoices(eL, eR);
            eL *= elec; eR *= elec;

            // 3) microfones acústicos (só na aba EAD)
            float aL = 0.0f, aR = 0.0f;
            if (hybrid) {
                const float l = x[0] * acLvl;
                const float r = (in[1] ? x[1] : x[0]) * acLvl; // entrada mono: duplica nos dois lados
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

        for (int p = 0; p < kNumPads; ++p) status.inputLevel[p].store(inPeak[p], std::memory_order_relaxed);
        status.outputLevel.store(outMeter, std::memory_order_relaxed);
        status.gainReductionDb.store(gr, std::memory_order_relaxed);
    }

private:
    struct Voice {
        const std::vector<float>* data = nullptr;
        double pos = 0.0, inc = 1.0;
        float gainL = 0.0f, gainR = 0.0f;
        int choke = 0;
        float fade = 1.0f, fadeStep = 0.0f; // fadeStep>0: em fade-out
        unsigned long long age = 0;
        bool active = false;
    };

    void fire(int pad, float velocity) {
        const PadParams& pp = params.pad[pad];
        const float sens = pp.velocitySens.load();
        const float vel = 1.0f - sens + sens * velocity; // sens=0 -> sempre 1.0
        const float pitch = std::pow(2.0f, pp.pitchSemis.load() / 12.0f);
        startVoice(pp.sound.load(), vel * pp.level.load(), pitch, pp.pan.load(), 1.0f);
        status.lastVelocity[pad].store(velocity, std::memory_order_relaxed);
        status.hitCount[pad].fetch_add(1u, std::memory_order_relaxed);
    }

    void startVoice(int soundIdx, float gain, float pitch, float pan, float) {
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
    TriggerDetector det[kNumPads];
    Eq3 eq; Compressor comp; Reverb reverb;
    std::array<Voice, 24> voices{};
    unsigned long long voiceCounter = 0;
    float outMeter = 0.0f;
};

} // namespace nexus
