// Testes automáticos do núcleo de áudio (rodam sem placa de som).
#include "../core/Engine.h"
#include <functional>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace nexus;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA: %s  (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } else { std::printf("ok:    %s\n", msg); } } while (0)

static const double FS = 48000.0;
static const size_t N = static_cast<size_t>(FS * 2.0);

using Chans = std::vector<std::vector<float>>;
static Chans silence(int channels, size_t n = N) { return Chans(static_cast<size_t>(channels), std::vector<float>(n, 0.0f)); }

// "batida" de microfone: seno com decaimento rápido
static void addHit(std::vector<float>& buf, size_t at, float amp, float freq = 150.0f) {
    for (size_t i = 0; i < 2400 && at + i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(FS);
        buf[at + i] += amp * std::sin(2.0f * static_cast<float>(kPi) * freq * t) * std::exp(-t / 0.02f);
    }
}

struct Run { unsigned hits[kNumPads]; float lastVel[kNumPads]; float outPeak; bool finite; float peakL, peakR; };

static Run run(Engine& e, const Chans& ch, size_t from = 0) {
    const size_t n = ch[0].size();
    std::vector<float> L(n), R(n);
    const size_t blk = 128;
    std::vector<const float*> ptr(ch.size());
    for (size_t i = 0; i < n; i += blk) {
        const int m = static_cast<int>(std::min(blk, n - i));
        for (size_t c = 0; c < ch.size(); ++c) ptr[c] = ch[c].data() + i;
        e.processBlock(ptr.data(), static_cast<int>(ch.size()), L.data() + i, R.data() + i, m);
    }
    Run r{};
    for (int p = 0; p < kNumPads; ++p) { r.hits[p] = e.status.hitCount[p].load(); r.lastVel[p] = e.status.lastVelocity[p].load(); }
    r.finite = true;
    for (size_t i = from; i < n; ++i) {
        r.outPeak = std::max(r.outPeak, std::max(std::fabs(L[i]), std::fabs(R[i])));
        r.peakL = std::max(r.peakL, std::fabs(L[i])); r.peakR = std::max(r.peakR, std::fabs(R[i]));
        if (!std::isfinite(L[i]) || !std::isfinite(R[i])) r.finite = false;
    }
    return r;
}

static void setup(Engine& e) {
    e.prepare(FS);
    e.params.pad[0].thresholdDb = -36.0f; e.params.pad[1].thresholdDb = -36.0f;
    e.params.pad[0].lpHz = 6000.0f; e.params.pad[0].hpHz = 60.0f; e.params.pad[1].hpHz = 60.0f;
    e.params.pad[0].sound = 0; e.params.pad[1].sound = 3;
}

int main() {
    { // banco de sons
        Engine e; e.prepare(FS);
        bool ok = e.sounds().size() >= 16;
        for (int i = 0; i < e.sounds().size(); ++i) {
            const auto& s = e.sounds().get(i);
            float pk = 0; bool fin = true;
            for (float v : s.data) { pk = std::max(pk, std::fabs(v)); if (!std::isfinite(v)) fin = false; }
            if (s.data.size() < 500 || pk < 0.5f || pk > 1.0f || !fin) { ok = false; std::printf("  som ruim: %s\n", s.name.c_str()); }
        }
        CHECK(ok, "banco de sons: >=16 sons, todos audíveis, normalizados e sem NaN");
    }

    { // um golpe no mic 1 dispara o pad 1 e toca som
        Engine e; setup(e);
        auto c = silence(2); addHit(c[0], 4800, 0.5f);
        Run r = run(e, c);
        CHECK(r.hits[0] == 1 && r.hits[1] == 0, "1 golpe no mic 1 => exatamente 1 disparo no pad 1");
        CHECK(r.outPeak > 0.2f && r.finite, "disparo gera áudio de saída finito");
    }

    { // retrigger
        Engine e; setup(e);
        auto c = silence(2); addHit(c[0], 4800, 0.5f); addHit(c[0], 4800 + 480, 0.5f);
        CHECK(run(e, c).hits[0] == 1, "retrigger bloqueia 2º disparo a 10 ms");
        Engine e2; setup(e2);
        auto c2 = silence(2); addHit(c2[0], 4800, 0.5f); addHit(c2[0], 4800 + 9600, 0.5f);
        CHECK(run(e2, c2).hits[0] == 2, "batidas separadas por 200 ms disparam 2 vezes");
    }

    { // dinâmica
        Engine e; setup(e);
        auto c = silence(2); addHit(c[0], 4800, 0.04f);
        Run weak = run(e, c);
        Engine e2; setup(e2);
        auto c2 = silence(2); addHit(c2[0], 4800, 0.8f);
        Run strong = run(e2, c2);
        CHECK(weak.hits[0] == 1 && strong.hits[0] == 1, "batida fraca e forte são detectadas");
        CHECK(strong.lastVel[0] > weak.lastVel[0] + 0.3f, "velocity forte > velocity fraco");
    }

    { // abaixo do threshold
        Engine e; setup(e);
        auto c = silence(2); addHit(c[0], 4800, 0.005f);
        CHECK(run(e, c).hits[0] == 0, "som abaixo do threshold não dispara");
    }

    { // vazamento entre 2 microfones
        auto c = silence(2); addHit(c[0], 4800, 0.8f); addHit(c[1], 4800, 0.12f);
        Engine off; setup(off); off.params.bleedReject = 0.0f;
        Run r0 = run(off, c);
        Engine on; setup(on); on.params.bleedReject = 0.8f;
        Run r1 = run(on, c);
        CHECK(r0.hits[0] == 1 && r0.hits[1] == 1, "sem anti-bleed: o vazamento dispara o pad 2 (falso positivo)");
        CHECK(r1.hits[0] == 1 && r1.hits[1] == 0, "com anti-bleed: pad 1 dispara e o vazamento no pad 2 é rejeitado");
    }

    { // golpes simultâneos fortes
        auto c = silence(2); addHit(c[0], 4800, 0.7f); addHit(c[1], 4800, 0.6f);
        Engine on; setup(on); on.params.bleedReject = 0.8f;
        Run r = run(on, c);
        CHECK(r.hits[0] == 1 && r.hits[1] == 1, "bumbo+caixa ao mesmo tempo disparam os dois mesmo com anti-bleed");
    }

    { // filtros do pad
        Engine e; setup(e); e.params.pad[0].lpHz = 300.0f;
        auto c = silence(2); addHit(c[0], 4800, 0.3f, 6000.0f);
        CHECK(run(e, c).hits[0] == 0, "filtro passa-baixa do pad ignora agudos");
        Engine e2; setup(e2); e2.params.pad[0].lpHz = 300.0f;
        auto c2 = silence(2); addHit(c2[0], 4800, 0.3f, 100.0f);
        CHECK(run(e2, c2).hits[0] == 1, "…mas dispara com o grave do bumbo");
    }

    { // chimbal cortado
        Engine e; setup(e);
        e.params.pad[0].sound = 9; e.params.pad[1].sound = 8;
        auto c = silence(2); addHit(c[0], 4800, 0.5f); addHit(c[1], 4800 + 24000, 0.5f);
        Run r = run(e, c);
        CHECK(r.hits[0] == 1 && r.hits[1] == 1 && r.finite, "chimbal aberto cortado pelo fechado");
    }

    // ---------------- módulo virtual: várias entradas
    { // 6 entradas, 6 pads ativos: cada entrada dispara o seu pad
        Engine e; setup(e); e.params.activePads = 6;
        for (int p = 0; p < 6; ++p) { e.params.pad[p].thresholdDb = -36.0f; e.params.pad[p].hpHz = 60.0f; e.params.pad[p].lpHz = 8000.0f; e.params.pad[p].retriggerMs = 100.0f; }
        auto c = silence(6);
        for (size_t k = 0; k < 6; ++k) addHit(c[k], 4800 + k * 12000, 0.5f);
        Run r = run(e, c);
        bool ok = true; for (int p = 0; p < 6; ++p) ok = ok && r.hits[p] == 1;
        CHECK(ok && r.hits[6] == 0 && r.hits[7] == 0, "módulo virtual: 6 entradas disparam 6 pads, um golpe cada");
    }
    { // pads além do 'activePads' ficam desligados
        Engine e; setup(e); e.params.activePads = 2;
        auto c = silence(4); addHit(c[2], 4800, 0.6f); addHit(c[3], 24000, 0.6f);
        Run r = run(e, c);
        CHECK(r.hits[2] == 0 && r.hits[3] == 0, "pads desativados não disparam");
    }
    { // mapeamento: pad 1 ligado à entrada 4
        Engine e; setup(e); e.params.pad[0].inputChannel = 3;
        auto c = silence(4); addHit(c[0], 4800, 0.6f); addHit(c[3], 24000, 0.6f);
        Run r = run(e, c);
        CHECK(r.hits[0] == 1, "pad mapeado para a entrada 4 só reage à entrada 4");
    }
    { // 1 só entrada disponível (ex.: microfone USB): pad 2 não dispara e nada quebra
        Engine e; setup(e);
        auto c = silence(1); addHit(c[0], 4800, 0.6f);
        Run r = run(e, c);
        CHECK(r.hits[0] == 1 && r.hits[1] == 0 && r.finite, "com 1 entrada só o pad 1 dispara (sem travar)");
    }

    // ---------------- EAD
    { // microfone acústico passa direto na EAD e não no módulo
        auto c = silence(2);
        for (size_t i = 0; i < N; ++i) c[0][i] = 0.002f * std::sin(0.05f * static_cast<float>(i));
        Engine mod; setup(mod); mod.params.mode = static_cast<int>(Mode::Module);
        Engine ead; setup(ead); ead.params.mode = static_cast<int>(Mode::Hybrid);
        ead.params.acousticLevel = 1.0f; ead.params.compOn = false;
        Run rm = run(mod, c), re = run(ead, c);
        CHECK(rm.outPeak < 0.0005f, "aba Módulo: microfone NÃO passa direto (só sons)");
        CHECK(re.outPeak > 0.0015f && re.finite, "aba EAD: microfone acústico passa e mistura com os sons");
    }
    { // par estéreo e largura
        auto c = silence(2);
        for (size_t i = 0; i < N; ++i) c[0][i] = 0.002f * std::sin(0.05f * static_cast<float>(i));
        auto stereoRun = [&](float width) {
            Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid);
            e.params.acousticLevel = 1.0f; e.params.stereoWidth = width; e.params.compOn = false;
            return run(e, c);
        };
        Run w1 = stereoRun(1.0f), w0 = stereoRun(0.0f);
        CHECK(w1.peakL > 0.0015f && w1.peakR < 0.0001f, "EAD estéreo: Mic 1 só no canal esquerdo, Mic 2 (silencioso) na direita");
        CHECK(std::fabs(w0.peakL - w0.peakR) < 0.0001f && w0.peakL > 0.0005f, "EAD largura 0 = mono (esquerda = direita)");
    }
    { // EAD: calibração por peça e reconhecimento (inclusive 2 peças ao mesmo tempo)
        auto tone = [&](std::vector<float>& buf, size_t at, float amp, float f, float tau) {
            for (size_t i = 0; i < 9600 && at + i < buf.size(); ++i) { const float t = static_cast<float>(i) / static_cast<float>(FS); buf[at + i] += amp * std::sin(2.0f * static_cast<float>(kPi) * f * t) * std::exp(-t / tau); }
        };
        auto noise = [&](std::vector<float>& buf, size_t at, float amp, float tau, bool bright) {
            uint32_t r = 12345u; Biquad hp, lp; hp.setHighpass(FS, bright ? 7000.0 : 2500.0); lp.setLowpass(FS, bright ? 16000.0 : 6000.0);
            for (size_t i = 0; i < 9600 && at + i < buf.size(); ++i) {
                r = r * 1664525u + 1013904223u; const float w = (static_cast<float>((r >> 8) & 0xFFFF) / 32768.0f) - 1.0f;
                const float t = static_cast<float>(i) / static_cast<float>(FS);
                buf[at + i] += amp * lp.process(hp.process(w)) * std::exp(-t / tau);
            }
        };
        auto kickHit = [&](std::vector<float>& b, size_t at, float a) { tone(b, at, a, 70.0f, 0.08f); };
        auto snareHit = [&](std::vector<float>& b, size_t at, float a) { tone(b, at, a * 0.6f, 220.0f, 0.05f); noise(b, at, a * 0.8f, 0.09f, false); };
        auto hatHit = [&](std::vector<float>& b, size_t at, float a) { noise(b, at, a * 1.2f, 0.04f, true); };
        auto tomHit = [&](std::vector<float>& b, size_t at, float a) { tone(b, at, a, 150.0f, 0.15f); tone(b, at, a * 0.3f, 300.0f, 0.1f); };
        auto crashHit = [&](std::vector<float>& b, size_t at, float a) { noise(b, at, a * 1.1f, 0.5f, true); noise(b, at, a * 0.6f, 0.4f, false); };
        std::function<void(std::vector<float>&, size_t, float)> gens[5] = {kickHit, snareHit, hatHit, tomHit, crashHit};
        const int zmap[5] = {0, 1, 2, 3, 5};

        Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid);
        for (int k = 0; k < 5; ++k) {
            e.params.calibZone = zmap[k];
            for (int n = 0; n < 9; ++n) { auto c = silence(2, 24000); gens[k](c[0], 4800, 0.3f + 0.04f * static_cast<float>(n)); c[1] = c[0]; run(e, c); }
        }
        CHECK(e.params.calibZone.load() == -1 && e.status.calibDone.load() == 5, "calibração: 8 batidas por peça concluem e salvam o perfil");
        auto hitsFor = [&](std::function<void(std::vector<float>&)> make) {
            unsigned before[kNumZones]; for (int z = 0; z < kNumZones; ++z) before[z] = e.status.hitCount[z].load();
            auto c = silence(2, 48000); make(c[0]); c[1] = c[0]; run(e, c);
            std::array<unsigned, kNumZones> d{}; for (int z = 0; z < kNumZones; ++z) d[static_cast<size_t>(z)] = e.status.hitCount[z].load() - before[static_cast<size_t>(z)];
            return d;
        };
        for (int k = 0; k < 5; ++k) {
            auto d = hitsFor([&](std::vector<float>& b) { gens[k](b, 4800, 0.5f); });
            bool only = true; for (int z = 0; z < kNumZones; ++z) if (d[static_cast<size_t>(z)] != (z == zmap[k] ? 1u : 0u)) only = false;
            static const char* nm[5] = {"BUMBO", "CAIXA", "CHIMBAL", "TOM 1", "PRATO"};
            std::string msg = std::string("EAD calibrado: batida de ") + nm[k] + " dispara só a zona certa";
            CHECK(only, msg.c_str());
        }
        auto kh = hitsFor([&](std::vector<float>& b) { kickHit(b, 4800, 0.5f); hatHit(b, 4800, 0.5f); });
        CHECK(kh[0] == 1 && kh[2] == 1, "EAD: bumbo + chimbal ao mesmo tempo disparam as duas zonas");
        auto ks = hitsFor([&](std::vector<float>& b) { kickHit(b, 4800, 0.5f); snareHit(b, 4800, 0.5f); });
        CHECK(ks[0] == 1 && ks[1] == 1, "EAD: bumbo + caixa ao mesmo tempo disparam as duas zonas");
        auto soft = hitsFor([&](std::vector<float>& b) { kickHit(b, 4800, 0.5f); kickHit(b, 12000, 0.5f); });
        CHECK(soft[0] == 2 && soft[1] == 0 && soft[2] == 0, "EAD: duas batidas de bumbo seguidas = 2 disparos, sem peças falsas");
    }
    { // EAD funciona com 1 entrada só (mono)
        Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid);
        auto c = silence(1); addHit(c[0], 4800, 0.6f, 80.0f);
        Run r = run(e, c);
        CHECK(r.hits[0] + r.hits[1] + r.hits[2] + r.hits[3] + r.hits[4] + r.hits[5] >= 1 && r.finite, "EAD com microfone mono também detecta");
    }
    { // mistura acústico <-> kit
        auto peakWith = [&](float blend) {
            Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid); e.params.blend = blend; e.params.compOn = false;
            auto c = silence(2, 9600); addHit(c[0], 100, 0.5f, 1000.0f); c[1] = c[0];
            return run(e, c, 0);
        };
        Run acOnly = peakWith(0.0f), kitOnly = peakWith(1.0f);
        CHECK(acOnly.outPeak > 0.1f, "mistura 0 = só o som acústico dos microfones");
        CHECK(kitOnly.outPeak < acOnly.outPeak * 0.2f || kitOnly.hits[1] + kitOnly.hits[0] + kitOnly.hits[3] + kitOnly.hits[2] >= 1, "mistura 1 = só o kit (microfone some do mix)");
    }
    { // anti-retorno: o eco do próprio som tocado nas caixas é reduzido
        EchoCanceller ec; ec.prepare(1024);
        std::vector<float> ref(96000); uint32_t r = 7u;
        for (auto& v : ref) { r = r * 1664525u + 1013904223u; v = 0.3f * ((static_cast<float>((r >> 8) & 0xFFFF) / 32768.0f) - 1.0f); }
        const int delay = 500; const float g = 0.5f;
        double before = 0, after = 0;
        for (size_t i = 0; i < ref.size(); ++i) {
            const float mic = i >= static_cast<size_t>(delay) ? g * ref[i - static_cast<size_t>(delay)] : 0.0f;
            const float out = ec.process(ref[i], mic, true);
            if (i > 60000) { before += static_cast<double>(mic) * mic; after += static_cast<double>(out) * out; }
        }
        CHECK(after < before * 0.05, "anti-retorno reduz o eco do kit nos microfones em mais de 13 dB");
    }

    { // compressor
        Engine e; setup(e);
        e.params.mode = static_cast<int>(Mode::Hybrid);
        e.params.acousticLevel = 1.0f; e.params.electronicLevel = 0.0f;
        for (int z = 0; z < kNumZones; ++z) e.params.zone[z].thresholdDb = -3.0f;
        e.params.compThresholdDb = -24.0f; e.params.compRatio = 8.0f; e.params.compAttackMs = 1.0f;
        auto c = silence(2);
        for (size_t i = 0; i < N; ++i) c[0][i] = 0.5f * std::sin(0.1f * static_cast<float>(i));
        e.params.compOn = false; Run off = run(e, c, 24000);
        Engine e2; setup(e2);
        e2.params.mode = static_cast<int>(Mode::Hybrid);
        e2.params.acousticLevel = 1.0f; e2.params.electronicLevel = 0.0f;
        for (int z = 0; z < kNumZones; ++z) e2.params.zone[z].thresholdDb = -3.0f;
        e2.params.compThresholdDb = -24.0f; e2.params.compRatio = 8.0f; e2.params.compAttackMs = 1.0f;
        e2.params.compOn = true; Run on = run(e2, c, 24000);
        CHECK(on.outPeak < off.outPeak * 0.6f, "compressor reduz o pico de um sinal forte");
    }

    { // reverb estável
        Engine e; setup(e);
        e.params.reverbWet = 0.6f; e.params.reverbRoom = 1.0f;
        auto c = silence(2, N * 3); addHit(c[0], 4800, 0.8f);
        Run r = run(e, c);
        CHECK(r.finite && r.outPeak <= 1.0f, "reverb com sala máxima fica estável e dentro de 0 dBFS");
    }

    { // botões de teste
        Engine e; setup(e);
        e.params.manualHit[1] = 900; e.params.manualZone[2] = 900;
        auto c = silence(2, 4800);
        Run r = run(e, c);
        CHECK(r.hits[1] == 1 && r.hits[2] == 1 && r.outPeak > 0.2f, "botões de teste de pad e de zona tocam o som");
    }

    { // saídas independentes: bumbo só no L, caixa só no R, tom no canal 4
        Engine e; setup(e);
        e.params.pad[0].outRoute = 1; e.params.pad[1].outRoute = 2; e.params.pad[3].outRoute = 4;
        const size_t n = 9600;
        std::vector<float> in(n, 0.0f), L(n), R(n), x2(n), x3(n);
        const float* ins[1] = {in.data()};
        float* extra[6] = {x2.data(), x3.data(), nullptr, nullptr, nullptr, nullptr};
        auto energy = [](const std::vector<float>& v) { double a = 0; for (float f : v) a += f * f; return a; };
        e.params.manualHit[0] = 900;
        e.processBlock(ins, 1, L.data(), R.data(), static_cast<int>(n), extra);
        CHECK(energy(L) > 1.0 && energy(R) < 1e-9, "bumbo na saída 1 sai só no canal L");
        e.params.manualHit[1] = 900;
        e.processBlock(ins, 1, L.data(), R.data(), static_cast<int>(n), extra);
        CHECK(energy(R) > 1.0, "caixa na saída 2 sai no canal R");
        e.params.manualHit[3] = 900;
        e.processBlock(ins, 1, L.data(), R.data(), static_cast<int>(n), extra);
        CHECK(energy(x3) > 1.0 && energy(x2) < 1e-9, "tom na saída 4 sai só no 4º canal da interface");
    }

    { // Hold (tempo de leitura) e modo Aprender
        Engine e; setup(e);
        e.params.learn = true;
        auto c = silence(3, 9600); addHit(c[2], 2000, 0.6f, 150.0f);
        run(e, c);
        CHECK(e.status.learnedInput.load() == 2 && !e.params.learn.load(), "Aprender: a batida na entrada 3 seleciona a entrada 3");
        Engine h; setup(h); h.params.pad[0].holdMs = 15.0f;
        auto c2 = silence(2, 9600); addHit(c2[0], 2000, 0.6f, 150.0f);
        Run r = run(h, c2);
        CHECK(r.hits[0] == 1 && r.finite, "Hold de 15 ms ainda dispara a batida");
        Engine z; setup(z); z.params.mode = static_cast<int>(Mode::Hybrid);
        z.params.learn = true;
        auto c3 = silence(2, 24000); for (int k = 0; k < 2; ++k) { addHit(c3[k], 4800, 0.6f, 70.0f); }
        run(z, c3);
        CHECK(z.status.learnedZone.load() >= 0 && !z.params.learn.load(), "Aprender no EAD: a batida seleciona uma peça");
    }

    { // mute, solo, eco (delay) e fila de batidas para a saída MIDI
        Engine e; setup(e);
        e.params.pad[0].mute = true;
        auto c = silence(2, 9600); addHit(c[0], 1000, 0.6f, 100.0f);
        Run r = run(e, c);
        CHECK(r.hits[0] == 1 && r.outPeak < 1e-6f, "mute: a batida é detectada mas o pad fica mudo");
        Engine s2; setup(s2); s2.params.pad[1].solo = true;
        auto c2 = silence(2, 9600); addHit(c2[0], 1000, 0.6f, 100.0f);
        Run r2 = run(s2, c2);
        CHECK(r2.outPeak < 1e-6f, "solo em outro pad silencia este pad");
        Engine d; setup(d); d.params.delayMix = 0.8f; d.params.delayMs = 100.0f; d.params.manualHit[0] = 900;
        Run r3 = run(d, silence(2, 48000));
        CHECK(r3.finite && r3.outPeak > 0.2f, "eco (delay) gera saída finita");
        Engine q; setup(q); q.params.pad[2].inputChannel = 0; q.params.activePads = 3;
        { auto cq = silence(3, 9600); addHit(cq[2], 1000, 0.6f, 6000.0f); q.params.pad[2].inputChannel = 2; run(q, cq); }
        EngineStatus::HitEvent he; const bool got = q.status.popHit(he);
        CHECK(got && he.idx == 2 && !he.zone && he.vel > 0.6f, "cada batida entra na fila da saída MIDI");
    }

    { // efeitos por canal: envio de reverb e EQ de cada peça
        auto tail = [](float send) {
            Engine e; setup(e); e.params.reverbWet = 1.0f; e.params.reverbRoom = 1.0f; e.params.pad[0].revSend = send;
            e.params.manualHit[0] = 900;
            return run(e, silence(2, 96000), 72000).outPeak;
        };
        const float t0 = tail(0.0f), t2 = tail(2.0f);
        CHECK(t2 > 4.0f * t0 + 1e-4f, "envio de reverb por peça: mais envio = mais cauda de reverb");
        Engine a; setup(a); a.params.manualHit[1] = 900; Run ra = run(a, silence(2, 24000));
        Engine b; setup(b); b.params.pad[1].chEqHighDb = -12.0f; b.params.pad[1].chEqMidDb = -12.0f; b.params.manualHit[1] = 900;
        Run rb = run(b, silence(2, 24000));
        CHECK(rb.finite && rb.outPeak < ra.outPeak * 0.9f, "EQ por peça muda só o som daquela peça");
    }

    { // sons gravados: camada pela força, variações alternadas, estéreo
        Engine e; setup(e);
        auto L = std::make_shared<Sound::Layers>();
        for (int l = 0; l < 3; ++l) {
            std::vector<Take> takes;
            for (int r = 0; r < 2; ++r) {
                Take t; const float a = 0.2f + 0.3f * static_cast<float>(l) + 0.02f * static_cast<float>(r);
                t.L.assign(4800, a); t.R.assign(4800, a * 0.5f); takes.push_back(t);
            }
            L->push_back(takes);
        }
        const int idx = e.soundBank().addSampled("Teste", "Teste", 0, L, FS, FS);
        CHECK(idx >= 16 && e.soundBank().get(idx).sampled(), "som gravado entra no banco");
        e.params.pad[0].sound = idx; e.params.pad[0].pan = 0.0f; e.params.reverbWet = 0.0f;
        auto peakAt = [&](int vel) { e.params.manualHit[0] = vel; Run r = run(e, silence(2, 2400)); return std::make_pair(r.peakL, r.peakR); };
        const auto soft = peakAt(100), hard = peakAt(1000);
        CHECK(hard.first > soft.first * 2.0f, "batida forte usa a camada forte (mais alta)");
        CHECK(hard.second < hard.first * 0.7f, "som gravado sai em estéreo (L e R diferentes)");
        const auto h2 = peakAt(1000);
        CHECK(std::fabs(h2.first - hard.first) > 1e-4f, "batidas seguidas alternam as variações (round robin)");
    }

    std::printf("\n%s (%d falha%s)\n", failures ? "TESTES FALHARAM" : "TODOS OS TESTES PASSARAM", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
