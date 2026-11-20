// NEXUS DRUM TRIGGER - núcleo de áudio (sem dependência de JUCE)
#pragma once
#include <cmath>

namespace nexus {

constexpr double kPi = 3.14159265358979323846;

// Filtro biquad (forma transposta II), coeficientes do "Audio EQ Cookbook".
class Biquad {
public:
    void reset() { z1 = z2 = 0.0; }

    void setLowpass(double fs, double f, double q = 0.7071) {
        const double w = 2.0 * kPi * clampF(f, fs) / fs, c = std::cos(w), al = std::sin(w) / (2.0 * q);
        set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setHighpass(double fs, double f, double q = 0.7071) {
        const double w = 2.0 * kPi * clampF(f, fs) / fs, c = std::cos(w), al = std::sin(w) / (2.0 * q);
        set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setBandpass(double fs, double f, double q = 1.0) {
        const double w = 2.0 * kPi * clampF(f, fs) / fs, c = std::cos(w), al = std::sin(w) / (2.0 * q);
        set(al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void setPeaking(double fs, double f, double q, double gainDb) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * clampF(f, fs) / fs, c = std::cos(w), al = std::sin(w) / (2.0 * q);
        set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLowShelf(double fs, double f, double gainDb) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * clampF(f, fs) / fs, c = std::cos(w);
        const double beta = 2.0 * std::sqrt(A) * (std::sin(w) / 2.0 * std::sqrt(2.0));
        set(A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c),
            A * ((A + 1) - (A - 1) * c - beta), (A + 1) + (A - 1) * c + beta,
            -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
    }
    void setHighShelf(double fs, double f, double gainDb) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * clampF(f, fs) / fs, c = std::cos(w);
        const double beta = 2.0 * std::sqrt(A) * (std::sin(w) / 2.0 * std::sqrt(2.0));
        set(A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c),
            A * ((A + 1) + (A - 1) * c - beta), (A + 1) - (A - 1) * c + beta,
            2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
    }

    inline float process(float xf) {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float>(y);
    }

private:
    static double clampF(double f, double fs) {
        const double lo = 5.0, hi = fs * 0.45;
        return f < lo ? lo : (f > hi ? hi : f);
    }
    void set(double B0, double B1, double B2, double A0, double A1, double A2) {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

} // namespace nexus
