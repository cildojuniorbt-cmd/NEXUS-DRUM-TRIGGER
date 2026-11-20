#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus {

// EQ de 3 bandas (grave / médio / agudo), estéreo.
class Eq3 {
public:
    void prepare(double sampleRate) { fs = sampleRate; set(0, 0, 0); for (auto& b : f) b.reset(); }
    void set(float lowDb, float midDb, float highDb) {
        for (int c = 0; c < 2; ++c) {
            f[c * 3 + 0].setLowShelf(fs, 120.0, lowDb);
            f[c * 3 + 1].setPeaking(fs, 1200.0, 0.9, midDb);
            f[c * 3 + 2].setHighShelf(fs, 7000.0, highDb);
        }
    }
    inline void process(float& l, float& r) {
        for (int i = 0; i < 3; ++i) { l = f[i].process(l); r = f[3 + i].process(r); }
    }
private:
    double fs = 48000.0;
    Biquad f[6];
};

// Compressor estéreo com detecção ligada (feed-forward).
class Compressor {
public:
    void prepare(double sampleRate) { fs = sampleRate; env = 0.0f; grDb = 0.0f; }
    void set(float thresholdDb, float ratioIn, float attackMs, float releaseMs, float makeupDb) {
        thr = thresholdDb; ratio = std::max(1.0f, ratioIn); makeup = std::pow(10.0f, makeupDb / 20.0f);
        att = static_cast<float>(std::exp(-1.0 / (std::max(0.1f, attackMs) * 0.001 * fs)));
        rel = static_cast<float>(std::exp(-1.0 / (std::max(1.0f, releaseMs) * 0.001 * fs)));
    }
    inline void process(float& l, float& r) {
        const float lvl = std::max(std::fabs(l), std::fabs(r));
        const float c = lvl > env ? att : rel;
        env = c * env + (1.0f - c) * lvl;
        const float db = 20.0f * std::log10(std::max(env, 1e-6f));
        const float over = db - thr;
        grDb = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
        const float g = std::pow(10.0f, -grDb / 20.0f) * makeup;
        l *= g; r *= g;
    }
    float gainReductionDb() const { return grDb; }
private:
    double fs = 48000.0;
    float thr = -18.0f, ratio = 3.0f, makeup = 1.0f, att = 0.99f, rel = 0.999f, env = 0.0f, grDb = 0.0f;
};

// Reverb estilo Freeverb (8 filtros comb + 4 allpass por canal).
class Reverb {
public:
    void prepare(double sampleRate) {
        const double sc = sampleRate / 44100.0;
        static const int combT[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int apT[4] = {556, 441, 341, 225};
        for (int c = 0; c < 2; ++c) {
            const int spread = c * 23;
            for (int i = 0; i < 8; ++i) comb[c][i].init(static_cast<int>((combT[i] + spread) * sc));
            for (int i = 0; i < 4; ++i) ap[c][i].init(static_cast<int>((apT[i] + spread) * sc));
        }
        set(0.5f, 0.5f, 0.0f);
    }
    void set(float room, float damp, float wetAmount) {
        fb = std::min(0.98f, std::max(0.0f, room) * 0.28f + 0.7f);
        dm = std::min(1.0f, std::max(0.0f, damp)) * 0.4f;
        wet = std::min(1.0f, std::max(0.0f, wetAmount));
    }
    // Retorna só o sinal "molhado" (wet). O mix com o seco é feito por quem chama.
    inline void process(float inL, float inR, float& wetL, float& wetR) {
        const float in = (inL + inR) * 0.015f;
        float oL = 0, oR = 0;
        for (int i = 0; i < 8; ++i) { oL += comb[0][i].process(in, fb, dm); oR += comb[1][i].process(in, fb, dm); }
        for (int i = 0; i < 4; ++i) { oL = ap[0][i].process(oL); oR = ap[1][i].process(oR); }
        wetL = oL * 3.0f * wet; wetR = oR * 3.0f * wet;
    }
    float amount() const { return wet; }
private:
    struct Comb {
        std::vector<float> b; int idx = 0; float store = 0.0f;
        void init(int n) { b.assign(static_cast<size_t>(std::max(1, n)), 0.0f); idx = 0; store = 0.0f; }
        inline float process(float x, float feedback, float damp) {
            const float o = b[static_cast<size_t>(idx)];
            store = o * (1.0f - damp) + store * damp;
            b[static_cast<size_t>(idx)] = x + store * feedback;
            if (++idx >= static_cast<int>(b.size())) idx = 0;
            return o;
        }
    };
    struct Allpass {
        std::vector<float> b; int idx = 0;
        void init(int n) { b.assign(static_cast<size_t>(std::max(1, n)), 0.0f); idx = 0; }
        inline float process(float x) {
            const float bo = b[static_cast<size_t>(idx)];
            const float o = -x + bo;
            b[static_cast<size_t>(idx)] = x + bo * 0.5f;
            if (++idx >= static_cast<int>(b.size())) idx = 0;
            return o;
        }
    };
    Comb comb[2][8];
    Allpass ap[2][4];
    float fb = 0.84f, dm = 0.2f, wet = 0.0f;
};

// Limitador suave de saída (protege a P.A.).
// Eco estéreo (delay) com realimentação e filtro de agudos no laço.
class Delay {
public:
    void prepare(double sampleRate) {
        fs = sampleRate;
        size = static_cast<int>(fs * 1.6) + 4;
        bufL.assign(static_cast<size_t>(size), 0.0f); bufR.assign(static_cast<size_t>(size), 0.0f);
        pos = 0; lpL = lpR = 0.0f;
    }
    void set(float timeMs, float feedbackIn, float mixIn) {
        d = std::min(size - 2, std::max(1, static_cast<int>(timeMs * 0.001 * fs)));
        fb = std::min(0.92f, std::max(0.0f, feedbackIn)); mix = std::min(1.0f, std::max(0.0f, mixIn));
    }
    // envio: entra só o que cada canal mandar; devolve o eco (molhado)
    inline void processSend(float inL, float inR, float& wetL, float& wetR) {
        wetL = wetR = 0.0f;
        if (mix <= 0.0001f) return;
        const int rp = (pos - d + size) % size;
        const float dl = bufL[static_cast<size_t>(rp)], dr = bufR[static_cast<size_t>(rp)];
        lpL += 0.45f * (dl - lpL); lpR += 0.45f * (dr - lpR);
        bufL[static_cast<size_t>(pos)] = inL + lpR * fb;
        bufR[static_cast<size_t>(pos)] = inR + lpL * fb;
        pos = (pos + 1) % size;
        wetL = dl * mix; wetR = dr * mix;
    }
    inline void process(float& l, float& r) {
        if (mix <= 0.0001f) return;
        const int rp = (pos - d + size) % size;
        const float dl = bufL[static_cast<size_t>(rp)], dr = bufR[static_cast<size_t>(rp)];
        lpL += 0.45f * (dl - lpL); lpR += 0.45f * (dr - lpR);
        // ping-pong leve: a repetição de um lado alimenta o outro
        bufL[static_cast<size_t>(pos)] = l + lpR * fb;
        bufR[static_cast<size_t>(pos)] = r + lpL * fb;
        pos = (pos + 1) % size;
        l += dl * mix; r += dr * mix;
    }
private:
    double fs = 48000.0;
    int size = 4, pos = 0, d = 1;
    float fb = 0.3f, mix = 0.0f, lpL = 0.0f, lpR = 0.0f;
    std::vector<float> bufL, bufR;
};

inline float softLimit(float x) {
    const float t = 0.85f;
    const float a = std::fabs(x);
    if (a <= t) return x;
    const float s = x < 0 ? -1.0f : 1.0f;
    return s * (t + (1.0f - t) * std::tanh((a - t) / (1.0f - t)));
}

} // namespace nexus
