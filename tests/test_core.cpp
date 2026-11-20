// Testes automáticos do núcleo de áudio (rodam sem placa de som).
#include "../core/Engine.h"
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
    { // zonas por frequência: bumbo (grave), caixa (médio), chimbal (agudo) a partir do par estéreo
        auto zoneTest = [&](float freq, float amp) {
            Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid); e.params.bleedReject = 0.8f;
            for (int z = 0; z < kNumZones; ++z) e.params.zone[z].retriggerMs = 100.0f; // a rajada sintética decai devagar
            auto c = silence(2); addHit(c[0], 4800, amp, freq); addHit(c[1], 4800, amp, freq);
            return run(e, c);
        };
        Run kick = zoneTest(80.0f, 0.6f), snare = zoneTest(1500.0f, 0.5f), hat = zoneTest(10000.0f, 0.5f);
        CHECK(kick.hits[0] == 1 && kick.hits[1] == 0 && kick.hits[2] == 0, "EAD: som grave vira só BUMBO");
        CHECK(snare.hits[1] == 1 && snare.hits[0] == 0 && snare.hits[2] == 0, "EAD: som médio vira só CAIXA");
        CHECK(hat.hits[2] == 1 && hat.hits[0] == 0 && hat.hits[1] == 0, "EAD: som agudo vira só CHIMBAL");
    }
    { // kick + chimbal ao mesmo tempo (grave e agudo) disparam as duas zonas
        Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid); e.params.bleedReject = 0.8f;
        for (int z = 0; z < kNumZones; ++z) e.params.zone[z].retriggerMs = 100.0f;
        auto c = silence(2);
        for (int k = 0; k < 2; ++k) { addHit(c[k], 4800, 0.6f, 80.0f); addHit(c[k], 4800, 0.5f, 10000.0f); }
        Run r = run(e, c);
        CHECK(r.hits[0] == 1 && r.hits[2] == 1, "EAD: bumbo e chimbal simultâneos disparam as duas zonas");
    }
    { // EAD funciona com 1 entrada só (mono)
        Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid);
        auto c = silence(1); addHit(c[0], 4800, 0.6f, 80.0f);
        Run r = run(e, c);
        CHECK(r.hits[0] == 1 && r.finite, "EAD com microfone mono também detecta");
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

    std::printf("\n%s (%d falha%s)\n", failures ? "TESTES FALHARAM" : "TODOS OS TESTES PASSARAM", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
