#pragma once
// Reconhecedor de peças por "impressão digital" espectral (EAD):
// os 2 microfones viram um sinal único; a cada batida medimos a energia em 8 faixas de frequência
// e descobrimos quais peças (bumbo, caixa, chimbal, tons, prato) formam aquela batida.
// Isso funciona mesmo com 2 peças tocando juntas (decomposição não-negativa).
#include "Biquad.h"
#include "Trigger.h"
#include <array>
#include <atomic>

namespace nexus {

constexpr int kBands = 8;
constexpr int kMaxZones = 6;
constexpr float kBaseSub = 0.6f;
constexpr float kOnsetRatio = 2.5f;   // a banda precisa subir tantas vezes acima do "rabo" anterior para contar como nova batida   // quanto do "rabo" da batida anterior é descontado da nova

// Perfis padrão (antes da calibração): bumbo, caixa, chimbal, tom 1, tom 2 (surdo), prato
inline const float* defaultProfile(int z) {
    static const float P[kMaxZones][kBands] = {
        {1.00f, 0.80f, 0.25f, 0.10f, 0.05f, 0.05f, 0.03f, 0.02f},
        {0.10f, 0.30f, 1.00f, 0.60f, 0.50f, 0.70f, 0.80f, 0.50f},
        {0.00f, 0.00f, 0.00f, 0.02f, 0.05f, 0.20f, 0.70f, 1.00f},
        {0.10f, 0.40f, 1.00f, 0.70f, 0.30f, 0.15f, 0.08f, 0.05f},
        {0.40f, 1.00f, 0.70f, 0.30f, 0.12f, 0.06f, 0.04f, 0.02f},
        {0.02f, 0.05f, 0.15f, 0.30f, 0.50f, 0.80f, 1.00f, 0.90f}};
    return P[z < 0 ? 0 : (z >= kMaxZones ? kMaxZones - 1 : z)];
}

struct SpectralSettings {
    float inputGainDb = 0.0f;
    float minThresholdDb = -40.0f;   // menor limiar entre as zonas (dispara a captura)
    float captureMs = 8.0f;
    float onsetLockMs = 12.0f;
    float relReject = 0.25f;         // descarta peças com menos de X% da peça mais forte
};

struct SpectralHit {
    int count = 0;
    int zone[kMaxZones] = {};
    float level[kMaxZones] = {};     // amplitude estimada de cada peça
    bool captured = false;           // houve captura (mesmo sem disparo)
    float observed[kBands] = {};     // vetor medido (para calibração)
};

class SpectralDetector {
public:
    void prepare(double sampleRate) {
        fs = sampleRate;
        static const double fc[kBands] = {60, 120, 250, 500, 1000, 2200, 4500, 9000};
        for (int b = 0; b < kBands; ++b) { bp[b].setBandpass(fs, fc[b], 1.1); }
        hp[0].setHighpass(fs, 35.0, 0.5412); hp[1].setHighpass(fs, 35.0, 1.3066);
        reset();
    }
    void reset() {
        for (auto& f : bp) f.reset();
        for (auto& f : bp2) f.reset();
        for (auto& f : hp) f.reset();
        capturing = false; capLeft = 0; lockLeft = 0;
        for (auto& v : peaks) v = 0.0f;
        for (auto& v : env) v = 0.0f;
        for (auto& v : curMax) v = 0.0f;
        for (auto& v : cache) v = 0.0f;
        for (auto& r : ring) for (auto& v : r) v = 0.0f;
        subCount = 0; ringIdx = 0;
        for (auto& v : base) v = 0.0f;
    }
    void configure(const SpectralSettings& s) {
        cfg = s; gain = dbToLin(s.inputGainDb); bandThr = dbToLin(s.minThresholdDb) * 0.5f;
        capSamples = std::max(8, static_cast<int>(s.captureMs * 0.001 * fs));
        lockSamples = std::max(capSamples + 1, static_cast<int>(s.onsetLockMs * 0.001 * fs));
        subLen = std::max(1, static_cast<int>(0.005 * fs));
    }
    void setProfiles(const float (*p)[kBands], int n) {
        nz = std::min(n, kMaxZones);
        for (int z = 0; z < nz; ++z) {
            float norm = 0.0f; for (int b = 0; b < kBands; ++b) norm += p[z][b] * p[z][b];
            norm = std::sqrt(std::max(norm, 1e-12f));
            for (int b = 0; b < kBands; ++b) prof[z][b] = p[z][b] / norm;
        }
    }

    // 1 amostra. Retorna true quando uma batida foi capturada e analisada.
    inline bool process(float xin, SpectralHit& out) {
        const float x = hp[1].process(hp[0].process(xin * gain));
        out.captured = false; out.count = 0;
        float yb[kBands];
        for (int b = 0; b < kBands; ++b) yb[b] = std::fabs(bp[b].process(x));
        if (lockLeft > 0) --lockLeft;
        bool done = false;
        if (!capturing) {
            if (lockLeft == 0) {
                bool onset = false;
                for (int b = 0; b < kBands; ++b) if (yb[b] > bandThr && (yb[b] > kOnsetRatio * env[b] + 1e-6f || env[b] < bandThr)) { onset = true; break; }
                if (onset) {
                    capturing = true; capLeft = capSamples;
                    for (int b = 0; b < kBands; ++b) { base[b] = env[b]; peaks[b] = yb[b]; }
                }
            }
        } else {
            for (int b = 0; b < kBands; ++b) peaks[b] = std::max(peaks[b], yb[b]);
            if (--capLeft <= 0) {
                capturing = false; lockLeft = lockSamples;
                out.captured = true;
                for (int b = 0; b < kBands; ++b) { peaks[b] = std::max(0.0f, peaks[b] - base[b] * kBaseSub); out.observed[b] = peaks[b]; }
                decompose(out);
                done = true;
            }
        }
        // janela deslizante de ~40 ms (8 sub-janelas de 5 ms): o "rabo" medido é o pico real recente
        for (int b = 0; b < kBands; ++b) { curMax[b] = std::max(curMax[b], yb[b]); env[b] = std::max(cache[b], curMax[b]); }
        if (++subCount >= subLen) {
            subCount = 0; ringIdx = (ringIdx + 1) & 7;
            for (int b = 0; b < kBands; ++b) { ring[b][ringIdx] = curMax[b]; curMax[b] = 0.0f; float m = 0.0f; for (int k = 0; k < 8; ++k) m = std::max(m, ring[b][k]); cache[b] = m; }
        }
        return done;
    }

private:
    // mínimos quadrados não-negativos (coordenada a coordenada): observado ≈ soma(a_k * perfil_k)
    void decompose(SpectralHit& out) {
        float amp[kMaxZones] = {};
        float r[kBands];
        for (int b = 0; b < kBands; ++b) r[b] = peaks[b];
        for (int it = 0; it < 300; ++it) {
            for (int k = 0; k < nz; ++k) {
                float dot = 0.0f;
                for (int b = 0; b < kBands; ++b) dot += prof[k][b] * r[b];
                const float na = std::max(0.0f, amp[k] + dot);
                const float d = na - amp[k];
                if (d != 0.0f) for (int b = 0; b < kBands; ++b) r[b] -= d * prof[k][b];
                amp[k] = na;
            }
        }
        float mx = 0.0f; for (int k = 0; k < nz; ++k) mx = std::max(mx, amp[k]);
        out.count = 0;
        for (int k = 0; k < nz; ++k)
            if (amp[k] > 0.0f && amp[k] >= mx * cfg.relReject) { out.zone[out.count] = k; out.level[out.count] = amp[k]; ++out.count; }
    }

    double fs = 48000.0;
    SpectralSettings cfg;
    Biquad bp[kBands], bp2[kBands], hp[2];
    float gain = 1.0f, bandThr = 0.005f, lastTot = 0.0f;
    float env[kBands] = {}, base[kBands] = {}, curMax[kBands] = {}, cache[kBands] = {};
    float ring[kBands][8] = {};
    int subLen = 240, subCount = 0, ringIdx = 0;
    int capSamples = 288, lockSamples = 576, capLeft = 0, lockLeft = 0;
    bool capturing = false;
    int nz = 0;
    float prof[kMaxZones][kBands] = {};
    float peaks[kBands] = {};
};

} // namespace nexus
