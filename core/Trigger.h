#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace nexus {

inline float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
inline float linToDb(float lin) { return 20.0f * std::log10(std::max(lin, 1.0e-9f)); }

struct TriggerEvent {
    bool fired = false;
    float velocity = 0.0f; // 0..1
    float peak = 0.0f;     // pico linear medido na janela de leitura
};

// Detector de batida para um microfone.
//  - filtro passa-banda (HP + LP) para isolar a peça (ex.: grave no bumbo)
//  - janela de leitura ("scan") para medir o pico real e obter a dinâmica
//  - tempo de retrigger (máscara) para não disparar duas vezes na mesma batida
class TriggerDetector {
public:
    struct Settings {
        float inputGainDb = 0.0f;
        float thresholdDb = -36.0f;
        float retriggerMs = 70.0f;
        float scanMs = 2.5f;
        float hpHz = 60.0f;
        float lpHz = 6000.0f;
        float dynamics = 1.0f; // expoente da curva de velocity (<1 = mais sensível a toques leves)
    };

    void prepare(double sampleRate) { fs = sampleRate; applyFilters(); reset(); }
    void reset() { hp.reset(); lp.reset(); scanning = false; scanLeft = 0; holdLeft = 0; peak = 0; recentPeak = 0; recentLeft = 0; }

    void configure(const Settings& s) {
        const bool filtersChanged = (s.hpHz != cfg.hpHz) || (s.lpHz != cfg.lpHz);
        cfg = s;
        gain = dbToLin(s.inputGainDb);
        thr = dbToLin(s.thresholdDb);
        scanSamples = std::max(1, static_cast<int>(s.scanMs * 0.001 * fs));
        holdSamples = std::max(scanSamples + 1, static_cast<int>(s.retriggerMs * 0.001 * fs));
        if (filtersChanged) applyFilters();
    }

    // Maior pico filtrado nos últimos 'ms' milissegundos (usado para rejeitar "vazamento" entre microfones).
    void setRecentWindowMs(float ms) { recentSamples = std::max(1, static_cast<int>(ms * 0.001 * fs)); }
    float recentLevel() const { return std::max(recentPeak, scanning ? peak : 0.0f); }
    float lastLevel() const { return level; }

    // Processa 1 amostra. 'rejectBelow' > 0 descarta a batida se o pico ficar abaixo desse valor (anti-bleed).
    inline TriggerEvent process(float x, float rejectBelow = 0.0f) {
        const float y = lp.process(hp.process(x * gain));
        const float a = std::fabs(y);
        level = a;

        if (a >= recentPeak) { recentPeak = a; recentLeft = recentSamples; }
        else if (recentLeft > 0) { --recentLeft; }
        else { recentPeak *= 0.9995f; }

        TriggerEvent ev;
        if (holdLeft > 0) { --holdLeft; return ev; }

        if (!scanning) {
            if (a > thr) { scanning = true; scanLeft = scanSamples; peak = a; }
            return ev;
        }
        peak = std::max(peak, a);
        if (--scanLeft <= 0) {
            scanning = false;
            holdLeft = holdSamples;
            if (rejectBelow > 0.0f && peak < rejectBelow) return ev; // era vazamento do outro microfone
            const float thrDb = linToDb(thr);
            const float range = std::max(6.0f, -thrDb); // de thr até 0 dBFS
            float v = (linToDb(peak) - thrDb) / range;
            v = std::min(1.0f, std::max(0.0f, v));
            v = std::pow(v, std::max(0.2f, cfg.dynamics));
            ev.fired = true;
            ev.peak = peak;
            ev.velocity = std::max(0.04f, v);
        }
        return ev;
    }

private:
    void applyFilters() { hp.setHighpass(fs, cfg.hpHz); lp.setLowpass(fs, cfg.lpHz); }

    double fs = 48000.0;
    Settings cfg;
    Biquad hp, lp;
    float gain = 1.0f, thr = 0.015f, level = 0.0f;
    int scanSamples = 120, holdSamples = 3360;
    bool scanning = false;
    int scanLeft = 0, holdLeft = 0;
    float peak = 0.0f;
    float recentPeak = 0.0f;
    int recentLeft = 0, recentSamples = 480;
};

} // namespace nexus
