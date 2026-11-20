#pragma once
// Anti-retorno: tira dos microfones o som que o próprio app está tocando nas caixas (NLMS adaptativo).
#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus {

class EchoCanceller {
public:
    void prepare(int tapsCount = 1024) {
        taps = tapsCount;
        w.assign(static_cast<size_t>(taps), 0.0f);
        hist.assign(static_cast<size_t>(taps) * 2, 0.0f);
        pos = 0; energy = 1e-6f;
    }
    void reset() { std::fill(w.begin(), w.end(), 0.0f); std::fill(hist.begin(), hist.end(), 0.0f); pos = 0; energy = 1e-6f; }
    float estimate() const { // eco estimado com o histórico atual (antes de empurrar a nova referência)
        const float* h = &hist[static_cast<size_t>(pos)];
        float s = 0.0f;
        for (int i = 0; i < taps; ++i) s += w[static_cast<size_t>(i)] * h[i];
        return s;
    }
    // ref = sinal que saiu nas caixas (amostra atual); mic = microfone. Retorna o microfone limpo.
    inline float process(float ref, float mic, bool adapt) {
        // empurra a referência (janela deslizante duplicada para leitura contígua)
        pos = pos == 0 ? taps - 1 : pos - 1;
        const float old = hist[static_cast<size_t>(pos)];
        hist[static_cast<size_t>(pos)] = ref; hist[static_cast<size_t>(pos) + static_cast<size_t>(taps)] = ref;
        energy += ref * ref - old * old;
        if (energy < 1e-6f) energy = 1e-6f;
        const float est = estimate();
        const float e = mic - est;
        if (adapt && energy > 1e-4f) {
            const float g = mu * e / (energy + 1e-3f * static_cast<float>(taps));
            const float* h = &hist[static_cast<size_t>(pos)];
            for (int i = 0; i < taps; ++i) w[static_cast<size_t>(i)] += g * h[i];
        }
        return e;
    }
    float mu = 0.35f;
private:
    int taps = 0, pos = 0;
    float energy = 1e-6f;
    std::vector<float> w, hist;
};

} // namespace nexus
