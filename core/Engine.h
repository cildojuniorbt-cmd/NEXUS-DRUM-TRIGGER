#pragma once
#include "Fx.h"
#include "Sounds.h"
#include "Spectral.h"
#include "Aec.h"
#include "Trigger.h"
#include <array>
#include <atomic>
#include <cmath>

namespace nexus {

constexpr int kNumPads = 8;    // módulo virtual: até 8 pads, cada um ligado a uma entrada da interface
constexpr int kNumZones = 6;
constexpr int kChannels = 14;  // canais do mixer interno: 8 pads + 6 peças do EAD   // EAD: bumbo, caixa, chimbal, tom 1, tom 2, prato (reconhecidos pelo espectro)
constexpr int kMaxInputs = 8;
constexpr int kMaxOutputs = 8;

enum class Mode : int { Module = 0, Hybrid = 1 };

// Parâmetros editáveis em tempo real (a interface escreve, o áudio lê).
struct PadParams {
    std::atomic<int> sound{0};
    std::atomic<int> outRoute{0};            // 0 = mix estéreo; 1..8 = saída independente (1 = só L, 2 = só R, 3..8 = canais extras)
    std::atomic<int> inputChannel{0};        // entrada da interface (0 = entrada 1) - só no módulo virtual
    std::atomic<float> level{1.0f};          // 0..1.5
    std::atomic<float> pitchSemis{0.0f};     // -12..+12
    std::atomic<float> pan{0.0f};            // -1..+1
    std::atomic<float> inputGainDb{0.0f};
    std::atomic<float> thresholdDb{-36.0f};
    std::atomic<float> retriggerMs{70.0f};
    std::atomic<float> holdMs{2.5f};         // tempo de leitura do pico (mede a força da batida; mais tempo = mais preciso, mais atraso)
    std::atomic<float> hpHz{60.0f};
    std::atomic<float> lpHz{6000.0f};
    std::atomic<bool> mute{false}, solo{false};
    // efeitos por canal (como numa mesa de som)
    std::atomic<float> revSend{1.0f};        // envio para o reverb (0..2; 1 = normal)
    std::atomic<float> dlySend{1.0f};        // envio para o eco (0..2; 1 = normal)
    std::atomic<float> chEqLowDb{0.0f}, chEqMidDb{0.0f}, chEqHighDb{0.0f};
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
    std::atomic<float> acRevSend{1.0f}, acDlySend{1.0f}; // envios dos microfones acústicos (EAD)
    std::atomic<float> acousticLevel{0.8f};   // nível do par estéreo de microfones (Mic 1 = E, Mic 2 = D)
    std::atomic<float> stereoWidth{1.0f};     // 0 = mono, 1 = estéreo natural, 1.5 = mais aberto
    std::atomic<float> electronicLevel{1.0f};
    std::atomic<float> eqLowDb{0.0f}, eqMidDb{0.0f}, eqHighDb{0.0f};
    std::atomic<float> compThresholdDb{-18.0f}, compRatio{3.0f}, compAttackMs{8.0f}, compReleaseMs{120.0f}, compMakeupDb{0.0f};
    std::atomic<bool> compOn{true};

    // Efeitos do barramento principal (valem para tudo que sai no mix estéreo)
    std::atomic<float> mEqLowDb{0.0f}, mEqMidDb{0.0f}, mEqHighDb{0.0f};
    std::atomic<float> mCompThresholdDb{-12.0f}, mCompRatio{2.0f}, mCompAttackMs{10.0f}, mCompReleaseMs{150.0f}, mCompMakeupDb{0.0f};
    std::atomic<bool> mCompOn{false}, mEqOn{false};
    std::atomic<float> delayMs{320.0f}, delayFeedback{0.35f}, delayMix{0.0f};

    // MIDI: nota de cada pad / peça do EAD (padrão General MIDI) e saída MIDI
    std::atomic<int> midiNote[8] = {{36}, {38}, {42}, {48}, {45}, {41}, {49}, {51}};
    std::atomic<int> midiZoneNote[6] = {{36}, {38}, {42}, {48}, {45}, {49}};
    std::atomic<bool> midiOutOn{false};
    std::atomic<int> midiOutChannel{10};

    // Comandos vindos da interface
    std::atomic<int> manualHit[kNumPads] = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}};  // velocity*1000 (0 = nenhum)
    std::atomic<int> manualZone[kNumZones] = {{0}, {0}, {0}, {0}, {0}, {0}};
    std::atomic<bool> learn{false};          // "Aprender": a próxima batida seleciona a peça/entrada
    std::atomic<int> calibZone{-1};          // >=0: aprendendo o som dessa peça
    std::atomic<float> profile[kNumZones][kBands];   // impressão digital de cada peça (0 = usar o padrão)
    std::atomic<float> blend{0.5f};          // 0 = só acústico, 1 = só kit eletrônico
    std::atomic<bool> antiFeedback{false};   // tira do microfone o som que o app toca nas caixas
    std::atomic<int> previewSound{-1};

    EngineParams() {
        for (auto& z : profile) for (auto& v : z) v = 0.0f;
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
        zone[3].setup(10, 0, -32.0f, 60.0f, 4000.0f, 60.0f, -0.2f);   // tom 1
        zone[4].setup(12, 0, -32.0f, 60.0f, 4000.0f, 60.0f, 0.3f);    // tom 2 / surdo
        zone[5].setup(13, 0, -32.0f, 2000.0f, 16000.0f, 120.0f, -0.4f); // prato
    }
};

// Medidores e eventos para a interface.
struct EngineStatus {
    std::atomic<float> inputLevel[kMaxInputs] = {{0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}};
    std::atomic<float> lastVelocity[kNumPads] = {{0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}};
    std::atomic<unsigned> hitCount[kNumPads] = {{0u}, {0u}, {0u}, {0u}, {0u}, {0u}, {0u}, {0u}};
    std::atomic<float> outputLevel{0.0f};
    std::atomic<float> gainReductionDb{0.0f};
    std::atomic<int> calibCount{0};      // batidas capturadas na calibração em andamento
    std::atomic<int> calibDone{-1};      // última zona calibrada (para a interface avisar)
    std::atomic<int> learnedInput{-1};   // entrada em que a batida foi ouvida (modo Aprender)
    std::atomic<int> learnedZone{-1};    // peça reconhecida (modo Aprender, EAD)
    // fila de batidas para a saída MIDI (1 produtor = áudio, 1 consumidor = thread MIDI)
    struct HitEvent { int idx = 0; bool zone = false; float vel = 0.0f; };
    HitEvent hitQueue[64];
    std::atomic<unsigned> hitHead{0}, hitTail{0};
    void pushHit(int idx, bool zone, float vel) {
        const unsigned h = hitHead.load(std::memory_order_relaxed);
        if (h - hitTail.load(std::memory_order_acquire) >= 64) return;
        hitQueue[h & 63] = {idx, zone, vel};
        hitHead.store(h + 1, std::memory_order_release);
    }
    bool popHit(HitEvent& e) {
        const unsigned t = hitTail.load(std::memory_order_relaxed);
        if (t == hitHead.load(std::memory_order_acquire)) return false;
        e = hitQueue[t & 63];
        hitTail.store(t + 1, std::memory_order_release);
        return true;
    }
};

class Engine {
public:
    EngineParams params;
    EngineStatus status;

    void prepare(double sampleRate) {
        fs = sampleRate;
        bank.build(fs);
        for (int i = 0; i < kNumPads; ++i) { det[i].prepare(fs); det[i].setRecentWindowMs(12.0f); }
        sdet.prepare(fs); aec[0].prepare(1024); aec[1].prepare(1024); prevOut = 0.0f;
        for (auto& h : zoneHold) h = 0;
        calSeen = -1; calN = 0;
        eq.prepare(fs); for (auto& q : chEq) q.prepare(fs); for (auto& v : chEqSet) v[0] = v[1] = v[2] = 0.0f; comp.prepare(fs); reverb.prepare(fs); meq.prepare(fs); mcomp.prepare(fs); delay.prepare(fs);
        for (auto& v : voices) v = Voice{};
        voiceCounter = 0;
        outMeter = 0.0f;
    }

    const SoundBank& sounds() const { return bank; }
    SoundBank& soundBank() { return bank; }
    const SoundBank& soundBank() const { return bank; }

    // inputs: um ponteiro por canal de entrada da interface (nIn canais). Saída estéreo.
    // extra: buffers das saídas 3..8 (kMaxOutputs-2 ponteiros; pode ser nullptr). Os sons com saída "1" e "2" vão para L e R.
    void processBlock(const float* const* inputs, int nIn, float* outL, float* outR, int n, float* const* extra = nullptr) {
        const Mode mode = static_cast<Mode>(params.mode.load(std::memory_order_relaxed));
        const bool hybrid = mode == Mode::Hybrid;
        nIn = std::min(nIn, kMaxInputs);

        const int active = std::min(kNumPads, std::max(1, params.activePads.load()));
        int chan[kNumPads];
        for (int p = 0; p < kNumPads; ++p) {
            chan[p] = std::min(kMaxInputs - 1, std::max(0, params.pad[p].inputChannel.load()));
            det[p].configure(settingsOf(params.pad[p]));
        }
        {   // EAD: reconhecedor espectral
            SpectralSettings ss;
            float minThr = 0.0f;
            for (int z = 0; z < kNumZones; ++z) minThr = std::min(minThr, params.zone[z].thresholdDb.load() + std::min(0.0f, params.zone[z].inputGainDb.load()));
            ss.minThresholdDb = std::max(-60.0f, minThr - 6.0f);
            ss.relReject = 0.2f + 0.5f * params.bleedReject.load();
            sdet.configure(ss);
            float pr[kNumZones][kBands];
            for (int z = 0; z < kNumZones; ++z) {
                float nrm = 0.0f;
                for (int b = 0; b < kBands; ++b) { pr[z][b] = params.profile[z][b].load(); nrm += pr[z][b] * pr[z][b]; }
                if (nrm < 1e-9f) { const float* d = defaultProfile(z); for (int b = 0; b < kBands; ++b) pr[z][b] = d[b]; }
            }
            sdet.setProfiles(pr, kNumZones);
        }

        const float bleed = params.bleedReject.load();
        eq.set(params.eqLowDb.load(), params.eqMidDb.load(), params.eqHighDb.load());
        comp.set(params.compThresholdDb.load(), params.compRatio.load(), params.compAttackMs.load(),
                 params.compReleaseMs.load(), params.compMakeupDb.load());
        reverb.set(params.reverbRoom.load(), params.reverbDamp.load(), params.reverbWet.load());
        const bool mEqOn = params.mEqOn.load(), mCompOn = params.mCompOn.load();
        meq.set(params.mEqLowDb.load(), params.mEqMidDb.load(), params.mEqHighDb.load());
        mcomp.set(params.mCompThresholdDb.load(), params.mCompRatio.load(), params.mCompAttackMs.load(), params.mCompReleaseMs.load(), params.mCompMakeupDb.load());
        delay.set(params.delayMs.load(), params.delayFeedback.load(), params.delayMix.load());
        for (int c = 0; c < kChannels; ++c) {
            const PadParams& cp = c < kNumPads ? params.pad[c] : params.zone[c - kNumPads];
            const float lo = cp.chEqLowDb.load(), mi = cp.chEqMidDb.load(), hi = cp.chEqHighDb.load();
            chEqOn[c] = (lo != 0.0f || mi != 0.0f || hi != 0.0f);
            if (chEqOn[c] && (lo != chEqSet[c][0] || mi != chEqSet[c][1] || hi != chEqSet[c][2])) {
                chEq[c].set(lo, mi, hi); chEqSet[c][0] = lo; chEqSet[c][1] = mi; chEqSet[c][2] = hi;
            }
            chRev[c] = cp.revSend.load(); chDly[c] = cp.dlySend.load();
        }
        const float acRev = params.acRevSend.load(), acDly = params.acDlySend.load();
        anySolo = false;
        for (int p = 0; p < kNumPads; ++p) if (params.pad[p].solo.load()) anySolo = true;
        anySoloZone = false;
        for (int z = 0; z < kNumZones; ++z) if (params.zone[z].solo.load()) anySoloZone = true;
        const bool compOn = params.compOn.load();
        const float blend = params.blend.load();
        const bool aecOn = hybrid && params.antiFeedback.load();
        const float elec = params.electronicLevel.load() * (hybrid ? std::min(1.0f, 2.0f * blend) : 1.0f);
        const float master = params.master.load();
        const float acLvl = hybrid ? params.acousticLevel.load() * std::min(1.0f, 2.0f * (1.0f - blend)) : 0.0f;
        const float width = params.stereoWidth.load();

        // comandos da interface
        for (int p = 0; p < kNumPads; ++p) {
            const int m = params.manualHit[p].exchange(0);
            if (m > 0) fire(params.pad[p], p, static_cast<float>(m) / 1000.0f, false);
        }
        for (int z = 0; z < kNumZones; ++z) {
            const int m = params.manualZone[z].exchange(0);
            if (m > 0) fire(params.zone[z], z, static_cast<float>(m) / 1000.0f, false);
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
            if (params.learn.load(std::memory_order_relaxed) && !hybrid) {
                for (int c = 0; c < kMaxInputs; ++c)
                    if (std::fabs(x[c]) > 0.03f) { status.learnedInput.store(c); params.learn.store(false); break; }
            }
            if (aecOn) { for (int c = 0; c < 2; ++c) x[c] = aec[c].process(prevOut, x[c], true); }

            // 1) detecção
            if (hybrid) {
                // EAD: os 2 microfones viram um par estéreo; a soma é analisada pelo espectro (bumbo, caixa, chimbal, tons, prato)
                const float m = nIn >= 2 ? 0.5f * (x[0] + x[1]) : x[0];
                for (auto& h : zoneHold) if (h > 0) --h;
                SpectralHit sh;
                if (sdet.process(m, sh)) onSpectral(sh);
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
            float eL = 0.0f, eR = 0.0f, rvL = 0.0f, rvR = 0.0f, dyL = 0.0f, dyR = 0.0f;
            float bus[kMaxOutputs] = {};
            float cL[kChannels] = {}, cR[kChannels] = {};
            renderVoices(cL, cR, bus);
            for (int c = 0; c < kChannels; ++c) {
                if (cL[c] == 0.0f && cR[c] == 0.0f && !chEqOn[c]) continue;
                float l = cL[c] * elec, r = cR[c] * elec;
                if (chEqOn[c]) chEq[c].process(l, r);
                eL += l; eR += r;
                rvL += l * chRev[c]; rvR += r * chRev[c];
                dyL += l * chDly[c]; dyR += r * chDly[c];
            }

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
            rvL += aL * acRev; rvR += aR * acRev;
            dyL += aL * acDly; dyR += aR * acDly;

            // 4) reverb e eco por envio (cada peça manda a quantidade dela)
            float wL, wR, xL, xR;
            reverb.process(rvL, rvR, wL, wR);
            delay.processSend(dyL, dyR, xL, xR);
            L += wL + xL; R += wR + xR;
            if (mEqOn) meq.process(L, R);
            if (mCompOn) mcomp.process(L, R);
            L = softLimit(L * master);
            R = softLimit(R * master);
            // saídas independentes (sem reverb): 1 -> L, 2 -> R, 3..8 -> canais extras
            L = softLimit(L + softLimit(bus[0] * elec * master));
            R = softLimit(R + softLimit(bus[1] * elec * master));
            if (extra) for (int k = 2; k < kMaxOutputs; ++k) if (extra[k - 2]) extra[k - 2][i] = softLimit(bus[k] * elec * master);
            outL[i] = L; outR[i] = R;
            prevOut = 0.5f * (L + R);
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
        s.retriggerMs = pp.retriggerMs.load(); s.scanMs = pp.holdMs.load(); s.hpHz = pp.hpHz.load(); s.lpHz = pp.lpHz.load();
        s.dynamics = pp.dynamics.load();
        return s;
    }

    struct Voice {
        const float* dL = nullptr; const float* dR = nullptr; size_t len = 0;
        double pos = 0.0, inc = 1.0;
        float gainL = 0.0f, gainR = 0.0f;
        int choke = 0, route = 0, owner = 0;
        float fade = 1.0f, fadeStep = 0.0f; // fadeStep>0: em fade-out
        unsigned long long age = 0;
        bool active = false;
    };

    void onSpectral(const SpectralHit& sh) {
        if (params.learn.load() && params.calibZone.load() < 0) {
            int best = -1; float bl = 0.0f;
            for (int j = 0; j < sh.count; ++j) if (sh.level[j] > bl) { bl = sh.level[j]; best = sh.zone[j]; }
            if (best >= 0) { status.learnedZone.store(best); params.learn.store(false); }
        }
        const int cal = params.calibZone.load();
        if (cal != calSeen) { calSeen = cal; calN = 0; for (auto& v : calSum) v = 0.0f; status.calibCount.store(0); }
        if (cal >= 0 && cal < kNumZones) {
            float nrm = 0.0f; for (int b = 0; b < kBands; ++b) nrm += sh.observed[b] * sh.observed[b];
            nrm = std::sqrt(nrm);
            if (nrm < 1e-5f) return;
            for (int b = 0; b < kBands; ++b) calSum[b] += sh.observed[b] / nrm;
            ++calN; status.calibCount.store(calN);
            fire(params.zone[cal], cal, 0.8f);
            if (calN >= 8) {
                for (int b = 0; b < kBands; ++b) params.profile[cal][b].store(calSum[b] / static_cast<float>(calN));
                params.calibZone.store(-1); status.calibDone.store(cal); calSeen = -1; calN = 0;
            }
            return;
        }
        for (int j = 0; j < sh.count; ++j) {
            const int z = sh.zone[j];
            PadParams& zp = params.zone[z];
            if (zoneHold[z] > 0) continue;
            const float lvl = sh.level[j] * dbToLin(zp.inputGainDb.load());
            const float thrDb = zp.thresholdDb.load();
            if (linToDb(lvl) < thrDb) continue;
            const float range = std::max(6.0f, -thrDb);
            float v = std::min(1.0f, std::max(0.0f, (linToDb(lvl) - thrDb) / range));
            v = std::pow(v, std::max(0.2f, zp.dynamics.load()));
            zoneHold[z] = std::max(1, static_cast<int>(zp.retriggerMs.load() * 0.001 * fs));
            fire(zp, z, std::max(0.04f, v));
        }
    }

    void fire(PadParams& pp, int statusIdx, float velocity, bool announce = true) {
        const bool isZone = (&pp >= &params.zone[0] && &pp < &params.zone[0] + kNumZones);
        status.lastVelocity[statusIdx].store(velocity, std::memory_order_relaxed);
        status.hitCount[statusIdx].fetch_add(1u, std::memory_order_relaxed);
        if (announce) status.pushHit(statusIdx, isZone, velocity);
        if (pp.mute.load() || (isZone ? (anySoloZone && !pp.solo.load()) : (anySolo && !pp.solo.load()))) return;
        const float sens = pp.velocitySens.load();
        const float vel = 1.0f - sens + sens * velocity; // sens=0 -> sempre 1.0
        const float pitch = std::pow(2.0f, pp.pitchSemis.load() / 12.0f);
        startVoice(pp.sound.load(), pp.level.load(), pitch, pp.pan.load(), std::min(kMaxOutputs, std::max(0, pp.outRoute.load())),
                   isZone ? kNumPads + statusIdx : statusIdx, vel);
    }

    void startVoice(int soundIdx, float gain, float pitch, float pan, int route = 0, int owner = 0, float vel = 1.0f) {
        if (bank.size() == 0) return;
        const Sound& s = bank.get(soundIdx);
        // som gravado: a força escolhe a camada (a própria gravação já é mais fraca/forte) e as variações se alternam
        const float* dL = s.data.data(); const float* dR = nullptr; size_t len = s.data.size();
        if (s.sampled()) {
            const int nl = static_cast<int>(s.layers->size());
            const float v = std::min(1.0f, std::max(0.0f, vel));
            const int li = std::min(nl - 1, static_cast<int>(v * static_cast<float>(nl)));
            const auto& takes = (*s.layers)[static_cast<size_t>(li)];
            const size_t si = static_cast<size_t>(std::min(std::max(soundIdx, 0), SoundBank::kMaxSounds - 1));
            const Take& t = takes[(rrCounter[si]++) % takes.size()];
            dL = t.L.data(); dR = t.R.empty() ? nullptr : t.R.data(); len = t.L.size();
            // suaviza o salto entre camadas: dentro da camada, varia um pouco o volume
            if (nl < 3) gain *= vel; // poucas camadas: a força ainda controla o volume
            else {
                const float within = v * static_cast<float>(nl) - static_cast<float>(li);
                gain *= 0.82f + 0.18f * std::min(1.0f, within);
            }
        } else {
            gain *= vel;
        }
        if (len < 2) return;
        if (s.chokeGroup > 0) // chimbal: novo golpe corta o anterior
            for (auto& v : voices) if (v.active && v.choke == s.chokeGroup && v.fadeStep == 0.0f) v.fadeStep = static_cast<float>(1.0 / (fs * 0.006));
        Voice* slot = nullptr;
        for (auto& v : voices) if (!v.active) { slot = &v; break; }
        if (!slot) { slot = &voices[0]; for (auto& v : voices) if (v.age < slot->age) slot = &v; } // rouba a mais antiga
        const float p = std::min(1.0f, std::max(-1.0f, pan));
        const float ang = (p + 1.0f) * 0.25f * static_cast<float>(kPi);
        slot->dL = dL; slot->dR = dR; slot->len = len; slot->pos = 0.0; slot->inc = pitch * (s.sampled() ? s.rate : 1.0);
        slot->gainL = gain * std::cos(ang) * 1.414f; slot->gainR = gain * std::sin(ang) * 1.414f;
        slot->route = route; slot->owner = std::min(kChannels - 1, std::max(0, owner));
        if (route > 0) { slot->gainL = slot->gainR = gain; }
        slot->choke = s.chokeGroup; slot->fade = 1.0f; slot->fadeStep = 0.0f; slot->age = ++voiceCounter; slot->active = true;
    }

    inline void renderVoices(float* L, float* R, float* bus) {
        for (auto& v : voices) {
            if (!v.active) continue;
            const size_t i0 = static_cast<size_t>(v.pos);
            if (i0 + 1 >= v.len) { v.active = false; continue; }
            const float fr = static_cast<float>(v.pos - static_cast<double>(i0));
            const float sl = (v.dL[i0] * (1.0f - fr) + v.dL[i0 + 1] * fr) * v.fade;
            const float sr = v.dR ? (v.dR[i0] * (1.0f - fr) + v.dR[i0 + 1] * fr) * v.fade : sl;
            if (v.route > 0) bus[v.route - 1] += 0.5f * (sl + sr) * v.gainL;
            else { L[v.owner] += sl * v.gainL; R[v.owner] += sr * v.gainR; }
            v.pos += v.inc;
            if (v.fadeStep > 0.0f) { v.fade -= v.fadeStep; if (v.fade <= 0.0f) v.active = false; }
        }
    }

    double fs = 48000.0;
    SoundBank bank;
    unsigned rrCounter[SoundBank::kMaxSounds] = {};
    TriggerDetector det[kNumPads];
    SpectralDetector sdet;
    EchoCanceller aec[2];
    float prevOut = 0.0f;
    int zoneHold[kNumZones] = {};
    int calSeen = -1, calN = 0;
    float calSum[kBands] = {};
    Eq3 eq; Compressor comp; Reverb reverb;
    Eq3 chEq[kChannels];
    bool chEqOn[kChannels] = {};
    float chEqSet[kChannels][3] = {};
    float chRev[kChannels] = {}, chDly[kChannels] = {};
    Eq3 meq;
    Compressor mcomp;
    Delay delay;
    bool anySolo = false, anySoloZone = false;
    std::array<Voice, 32> voices{};
    unsigned long long voiceCounter = 0;
    float outMeter = 0.0f;
};

} // namespace nexus
