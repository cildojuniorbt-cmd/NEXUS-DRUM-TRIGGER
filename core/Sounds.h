#pragma once
#include <atomic>
#include "Biquad.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus {

struct Sound {
    std::string name;
    std::string category;
    int chokeGroup = 0; // >0: sons do mesmo grupo se cortam (chimbal)
    std::vector<float> data;
};

// Banco de sons sintetizados (gerados no código; sem arquivos externos).
class SoundBank {
public:
    static constexpr int kMaxSounds = 160;   // capacidade fixa: o vetor nunca realoca, então a interface pode importar sons com o áudio rodando
    SoundBank() { sounds.reserve(kMaxSounds); }
    int builtInCount() const { return 16; }

    // Som importado pelo usuário (mono, já na taxa de amostragem do motor). Retorna o índice ou -1 se a biblioteca estiver cheia.
    int addCustom(const std::string& name, std::vector<float> data) {
        if (data.size() < 64 || static_cast<int>(sounds.size()) >= kMaxSounds) return -1;
        add(name.c_str(), "Meus sons", 0, std::move(data));
        return static_cast<int>(sounds.size()) - 1;
    }

    void build(double fs) {
        count.store(0, std::memory_order_release);
        sounds.clear(); sounds.reserve(kMaxSounds);
        add("Bumbo 808", "Bumbo", 0, kick(fs, 150, 42, 0.060, 0.80, 0.25f));
        add("Bumbo Seco", "Bumbo", 0, kick(fs, 130, 58, 0.030, 0.22, 0.60f));
        add("Bumbo Rock", "Bumbo", 0, kick(fs, 105, 62, 0.040, 0.34, 0.45f, true));
        add("Caixa 909", "Caixa", 0, snare(fs, 185, 0.11, 0.20, 0.85f));
        add("Caixa Rock", "Caixa", 0, snare(fs, 205, 0.09, 0.28, 0.65f));
        add("Caixa Seca", "Caixa", 0, snare(fs, 230, 0.05, 0.12, 0.55f));
        add("Aro (Rim)", "Caixa", 0, rim(fs));
        add("Palma", "Percussão", 0, clap(fs));
        add("Chimbal Fechado", "Chimbal", 1, hat(fs, 0.045));
        add("Chimbal Aberto", "Chimbal", 1, hat(fs, 0.38));
        add("Tom Agudo", "Tom", 0, tom(fs, 210, 0.30));
        add("Tom Médio", "Tom", 0, tom(fs, 150, 0.38));
        add("Tom Grave", "Tom", 0, tom(fs, 100, 0.48));
        add("Prato Ataque", "Prato", 0, crash(fs));
        add("Sino (Cowbell)", "Percussão", 0, cowbell(fs));
        add("Prato Condução (Ride)", "Prato", 0, ride(fs));
    }

    int size() const { return count.load(std::memory_order_acquire); }
    const Sound& get(int i) const { return sounds[static_cast<size_t>(std::min(std::max(i, 0), size() - 1))]; }

private:
    std::vector<Sound> sounds;
    std::atomic<int> count{0};

    // ruído determinístico (mesmo som em toda execução)
    struct Rng {
        uint32_t s = 22222u;
        float next() { s = s * 1664525u + 1013904223u; return (static_cast<float>((s >> 8) & 0xFFFF) / 32768.0f) - 1.0f; }
    };

    void add(const char* name, const char* cat, int choke, std::vector<float> d) {
        float pk = 0.0f;
        for (float v : d) pk = std::max(pk, std::fabs(v));
        const float g = pk > 1e-6f ? 0.9f / pk : 1.0f;
        for (float& v : d) v *= g;
        // fade-out curto no final para não estalar
        const size_t fade = std::min<size_t>(d.size(), 64);
        for (size_t i = 0; i < fade; ++i) d[d.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);
        Sound s; s.name = name; s.category = cat; s.chokeGroup = choke; s.data = std::move(d);
        sounds.push_back(std::move(s));
        count.store(static_cast<int>(sounds.size()), std::memory_order_release);
    }

    static std::vector<float> kick(double fs, double f0, double f1, double sweepSec, double decaySec, float click, bool thump = false) {
        const size_t n = static_cast<size_t>(fs * (decaySec * 1.6 + 0.05));
        std::vector<float> o(n);
        Rng r; double ph = 0.0; Biquad lp; lp.setLowpass(fs, 3500.0);
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            const double f = f1 + (f0 - f1) * std::exp(-t / sweepSec);
            ph += 2.0 * kPi * f / fs;
            double s = std::sin(ph) * std::exp(-t / (decaySec * 0.45));
            if (t < 0.006) s += click * lp.process(r.next()) * std::exp(-t / 0.0015);
            if (thump && t < 0.05) s += 0.25 * lp.process(r.next()) * std::exp(-t / 0.02);
            o[i] = static_cast<float>(s);
        }
        return o;
    }

    static std::vector<float> snare(double fs, double tone, double toneDecay, double noiseDecay, float noiseMix) {
        const size_t n = static_cast<size_t>(fs * (noiseDecay * 2.2 + 0.05));
        std::vector<float> o(n);
        Rng r; Biquad hp; hp.setHighpass(fs, 1800.0); Biquad bp; bp.setBandpass(fs, 5500.0, 0.6);
        double p1 = 0, p2 = 0;
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            p1 += 2.0 * kPi * (tone + 60.0 * std::exp(-t / 0.01)) / fs;
            p2 += 2.0 * kPi * (tone * 1.78) / fs;
            const double body = (std::sin(p1) + 0.45 * std::sin(p2)) * std::exp(-t / toneDecay);
            const double nz = (hp.process(r.next()) * 0.6 + bp.process(r.next()) * 0.8) * std::exp(-t / noiseDecay);
            o[i] = static_cast<float>(body * (1.0 - 0.5 * noiseMix) + nz * noiseMix);
        }
        return o;
    }

    static std::vector<float> rim(double fs) {
        const size_t n = static_cast<size_t>(fs * 0.09);
        std::vector<float> o(n);
        Rng r; Biquad bp; bp.setBandpass(fs, 2800.0, 1.2);
        double p1 = 0, p2 = 0;
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            p1 += 2.0 * kPi * 1700.0 / fs; p2 += 2.0 * kPi * 480.0 / fs;
            o[i] = static_cast<float>((0.6 * std::sin(p1) + 0.8 * std::sin(p2)) * std::exp(-t / 0.012) +
                                      0.5 * bp.process(r.next()) * std::exp(-t / 0.006));
        }
        return o;
    }

    static std::vector<float> clap(double fs) {
        const size_t n = static_cast<size_t>(fs * 0.34);
        std::vector<float> o(n);
        Rng r; Biquad bp; bp.setBandpass(fs, 1500.0, 0.9);
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            double env = std::exp(-t / 0.11) * 0.8;
            for (int k = 0; k < 3; ++k) { // 3 "palmas" rápidas antes do corpo
                const double tk = t - 0.011 * k;
                if (tk >= 0.0 && tk < 0.009) env = std::max(env, std::exp(-tk / 0.003));
            }
            o[i] = static_cast<float>(bp.process(r.next()) * env);
        }
        return o;
    }

    static std::vector<float> hat(double fs, double decay) {
        const size_t n = static_cast<size_t>(fs * (decay * 2.5 + 0.03));
        std::vector<float> o(n);
        Biquad hp; hp.setHighpass(fs, 7000.0, 0.9);
        const double f[6] = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0};
        double ph[6] = {0, 0, 0, 0, 0, 0};
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            double s = 0.0;
            for (int k = 0; k < 6; ++k) { ph[k] += 2.0 * kPi * f[k] * 7.0 / fs; s += ph[k] - 2.0 * kPi * std::floor(ph[k] / (2.0 * kPi)) < kPi ? 1.0 : -1.0; }
            o[i] = hp.process(static_cast<float>(s / 6.0)) * static_cast<float>(std::exp(-t / decay));
        }
        return o;
    }

    static std::vector<float> tom(double fs, double f, double decay) {
        const size_t n = static_cast<size_t>(fs * (decay * 2.0 + 0.05));
        std::vector<float> o(n);
        Rng r; Biquad lp; lp.setLowpass(fs, 2500.0); double ph = 0;
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            ph += 2.0 * kPi * (f * (1.0 + 0.35 * std::exp(-t / 0.04))) / fs;
            double s = std::sin(ph) * std::exp(-t / decay);
            if (t < 0.01) s += 0.4 * lp.process(r.next()) * std::exp(-t / 0.003);
            o[i] = static_cast<float>(s);
        }
        return o;
    }

    static std::vector<float> crash(double fs) {
        const size_t n = static_cast<size_t>(fs * 1.8);
        std::vector<float> o(n);
        Rng r; Biquad hp; hp.setHighpass(fs, 3500.0);
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            o[i] = static_cast<float>(hp.process(r.next()) * (0.6 * std::exp(-t / 0.9) + 0.4 * std::exp(-t / 0.08)));
        }
        return o;
    }

    static std::vector<float> ride(double fs) {
        const size_t n = static_cast<size_t>(fs * 2.2);
        std::vector<float> o(n);
        Rng r; Biquad hp; hp.setHighpass(fs, 4500.0);
        const double f[4] = {3150.0, 4720.0, 6310.0, 8870.0};
        double ph[4] = {0, 0, 0, 0};
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            double s = 0.0;
            for (int k = 0; k < 4; ++k) { ph[k] += 2.0 * kPi * f[k] / fs; s += std::sin(ph[k]); }
            const double ping = 0.5 * s * std::exp(-t / 0.55);
            const double wash = hp.process(r.next()) * (0.35 * std::exp(-t / 0.9) + 0.2 * std::exp(-t / 0.05));
            o[i] = static_cast<float>(ping + wash);
        }
        return o;
    }

    static std::vector<float> cowbell(double fs) {
        const size_t n = static_cast<size_t>(fs * 0.5);
        std::vector<float> o(n);
        Biquad bp; bp.setBandpass(fs, 700.0, 1.4);
        double p1 = 0, p2 = 0;
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            p1 += 2.0 * kPi * 540.0 / fs; p2 += 2.0 * kPi * 800.0 / fs;
            const double sq = (std::sin(p1) > 0 ? 1.0 : -1.0) + (std::sin(p2) > 0 ? 1.0 : -1.0);
            o[i] = bp.process(static_cast<float>(sq * 0.5)) * static_cast<float>(std::exp(-t / 0.16));
        }
        return o;
    }
};

} // namespace nexus
