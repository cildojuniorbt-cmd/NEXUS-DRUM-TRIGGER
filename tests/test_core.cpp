// Testes automáticos do núcleo de áudio (rodam sem placa de som).
#include "../core/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace nexus;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA: %s  (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } else { std::printf("ok:    %s\n", msg); } } while (0)

static const double FS = 48000.0;

// "batida" de microfone: rajada de ruído/seno com decaimento rápido
static void addHit(std::vector<float>& buf, size_t at, float amp, float freq = 150.0f) {
    for (size_t i = 0; i < 2400 && at + i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(FS);
        buf[at + i] += amp * std::sin(2.0f * static_cast<float>(kPi) * freq * t) * std::exp(-t / 0.02f);
    }
}

struct Run { unsigned hits[2]; float lastVel[2]; float outPeak; bool finite; };

static Run run(Engine& e, const std::vector<float>& a, const std::vector<float>& b, size_t from = 0) {
    const size_t n = a.size();
    std::vector<float> L(n), R(n);
    const size_t blk = 128;
    for (size_t i = 0; i < n; i += blk) {
        const int m = static_cast<int>(std::min(blk, n - i));
        e.processBlock(a.data() + i, b.data() + i, L.data() + i, R.data() + i, m);
    }
    Run r{};
    for (int p = 0; p < 2; ++p) { r.hits[p] = e.status.hitCount[p].load(); r.lastVel[p] = e.status.lastVelocity[p].load(); }
    r.finite = true;
    for (size_t i = from; i < n; ++i) { r.outPeak = std::max(r.outPeak, std::max(std::fabs(L[i]), std::fabs(R[i]))); if (!std::isfinite(L[i]) || !std::isfinite(R[i])) r.finite = false; }
    return r;
}

static void setup(Engine& e) {
    e.prepare(FS);
    e.params.pad[0].thresholdDb = -36.0f; e.params.pad[1].thresholdDb = -36.0f;
    e.params.pad[0].sound = 0; e.params.pad[1].sound = 3;
}

int main() {
    const size_t N = static_cast<size_t>(FS * 2.0);

    { // banco de sons
        Engine e; e.prepare(FS);
        bool ok = e.sounds().size() >= 12;
        for (int i = 0; i < e.sounds().size(); ++i) {
            const auto& s = e.sounds().get(i);
            float pk = 0; bool fin = true;
            for (float v : s.data) { pk = std::max(pk, std::fabs(v)); if (!std::isfinite(v)) fin = false; }
            if (s.data.size() < 500 || pk < 0.5f || pk > 1.0f || !fin) { ok = false; std::printf("  som ruim: %s\n", s.name.c_str()); }
        }
        CHECK(ok, "banco de sons: >=12 sons, todos audíveis, normalizados e sem NaN");
    }

    { // um golpe no mic 1 dispara o pad 1 e toca som
        Engine e; setup(e);
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(a, 4800, 0.5f);
        Run r = run(e, a, b);
        CHECK(r.hits[0] == 1 && r.hits[1] == 0, "1 golpe no mic 1 => exatamente 1 disparo no pad 1");
        CHECK(r.outPeak > 0.2f && r.finite, "disparo gera áudio de saída finito");
    }

    { // retrigger: 2 golpes separados por 10 ms contam como 1; por 200 ms contam como 2
        Engine e; setup(e);
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(a, 4800, 0.5f); addHit(a, 4800 + 480, 0.5f);
        CHECK(run(e, a, b).hits[0] == 1, "retrigger bloqueia 2º disparo a 10 ms");
        Engine e2; setup(e2);
        std::vector<float> a2(N, 0.0f);
        addHit(a2, 4800, 0.5f); addHit(a2, 4800 + 9600, 0.5f);
        CHECK(run(e2, a2, b).hits[0] == 2, "batidas separadas por 200 ms disparam 2 vezes");
    }

    { // dinâmica: batida forte => velocity maior que batida fraca
        Engine e; setup(e);
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(a, 4800, 0.04f);
        Run weak = run(e, a, b);
        Engine e2; setup(e2);
        std::vector<float> a2(N, 0.0f);
        addHit(a2, 4800, 0.8f);
        Run strong = run(e2, a2, b);
        CHECK(weak.hits[0] == 1 && strong.hits[0] == 1, "batida fraca e forte são detectadas");
        CHECK(strong.lastVel[0] > weak.lastVel[0] + 0.3f, "velocity forte > velocity fraco");
    }

    { // ruído abaixo do threshold não dispara
        Engine e; setup(e);
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(a, 4800, 0.005f); // ~ -46 dB
        CHECK(run(e, a, b).hits[0] == 0, "som abaixo do threshold não dispara");
    }

    { // vazamento: batida forte no mic 1 aparece fraca no mic 2
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(a, 4800, 0.8f); addHit(b, 4800, 0.12f); // bleed ~ -16 dB no outro mic
        Engine off; setup(off); off.params.bleedReject = 0.0f;
        Run r0 = run(off, a, b);
        Engine on; setup(on); on.params.bleedReject = 0.8f;
        Run r1 = run(on, a, b);
        CHECK(r0.hits[0] == 1 && r0.hits[1] == 1, "sem anti-bleed: o vazamento dispara o pad 2 (falso positivo)");
        CHECK(r1.hits[0] == 1 && r1.hits[1] == 0, "com anti-bleed: pad 1 dispara e o vazamento no pad 2 é rejeitado");
    }

    { // dois golpes reais simultâneos (ambos fortes) continuam disparando os dois
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(a, 4800, 0.7f); addHit(b, 4800, 0.6f);
        Engine on; setup(on); on.params.bleedReject = 0.8f;
        Run r = run(on, a, b);
        CHECK(r.hits[0] == 1 && r.hits[1] == 1, "bumbo+caixa ao mesmo tempo disparam os dois mesmo com anti-bleed");
    }

    { // filtro: bumbo com HP/LP ignora agudos (ex.: chimbal no mic do bumbo)
        Engine e; setup(e);
        e.params.pad[0].lpHz = 300.0f;
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(a, 4800, 0.3f, 6000.0f);
        CHECK(run(e, a, b).hits[0] == 0, "filtro passa-baixa do pad ignora agudos");
        Engine e2; setup(e2); e2.params.pad[0].lpHz = 300.0f;
        std::vector<float> a2(N, 0.0f);
        addHit(a2, 4800, 0.3f, 100.0f);
        CHECK(run(e2, a2, b).hits[0] == 1, "…mas dispara com o grave do bumbo");
    }

    { // chimbal: golpe novo corta o aberto (grupo de corte) sem estourar
        Engine e; setup(e);
        e.params.pad[0].sound = 9; // chimbal aberto
        e.params.pad[1].sound = 8; // chimbal fechado
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(a, 4800, 0.5f); addHit(b, 4800 + 24000, 0.5f);
        Run r = run(e, a, b);
        CHECK(r.hits[0] == 1 && r.hits[1] == 1 && r.finite, "chimbal aberto cortado pelo fechado");
    }

    { // modo EAD (híbrido): microfone acústico passa direto, e no módulo não passa
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        for (size_t i = 0; i < N; ++i) a[i] = 0.002f * std::sin(0.05f * static_cast<float>(i)); // som baixo, não dispara
        Engine mod; setup(mod); mod.params.mode = static_cast<int>(Mode::Module);
        Engine ead; setup(ead); ead.params.mode = static_cast<int>(Mode::Hybrid);
        ead.params.acousticLevel = 1.0f; ead.params.compOn = false;
        Run rm = run(mod, a, b), re = run(ead, a, b);
        CHECK(rm.outPeak < 0.0005f, "aba Módulo: microfone NÃO passa direto (só sons)");
        CHECK(re.outPeak > 0.0015f && re.finite, "aba EAD: microfone acústico passa e mistura com os sons");
    }

    { // EAD: Mic 1 vai para a esquerda e Mic 2 para a direita (par estéreo) e a largura controla a abertura
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        for (size_t i = 0; i < N; ++i) a[i] = 0.002f * std::sin(0.05f * static_cast<float>(i)); // só no mic 1
        auto stereoRun = [&](float width, float& pkL, float& pkR) {
            Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid);
            e.params.acousticLevel = 1.0f; e.params.stereoWidth = width; e.params.compOn = false;
            std::vector<float> L(N), R(N);
            for (size_t i = 0; i < N; i += 128) { int m = static_cast<int>(std::min<size_t>(128, N - i)); e.processBlock(a.data() + i, b.data() + i, L.data() + i, R.data() + i, m); }
            pkL = pkR = 0; for (size_t i = 0; i < N; ++i) { pkL = std::max(pkL, std::fabs(L[i])); pkR = std::max(pkR, std::fabs(R[i])); }
        };
        float l1, r1, l0, r0;
        stereoRun(1.0f, l1, r1); stereoRun(0.0f, l0, r0);
        CHECK(l1 > 0.0015f && r1 < 0.0001f, "EAD estéreo: Mic 1 só no canal esquerdo, Mic 2 (silencioso) na direita");
        CHECK(std::fabs(l0 - r0) < 0.0001f && l0 > 0.0005f, "EAD largura 0 = mono (esquerda = direita)");
    }

    { // EAD: o gatilho ouve a soma dos dois microfones e dispara só o pad 1
        Engine e; setup(e); e.params.mode = static_cast<int>(Mode::Hybrid);
        std::vector<float> a(N, 0.0f), b(N, 0.0f);
        addHit(b, 4800, 0.6f); // golpe só no mic 2
        Run r = run(e, a, b);
        CHECK(r.hits[0] == 1 && r.hits[1] == 0, "EAD: golpe captado pelo par estéreo dispara o gatilho único (pad 1)");
    }

    { // compressor reduz sinal alto
        Engine e; setup(e);
        e.params.mode = static_cast<int>(Mode::Hybrid);
        e.params.acousticLevel = 1.0f; e.params.electronicLevel = 0.0f;
        e.params.pad[0].thresholdDb = -6.0f; // evita disparos
        e.params.compThresholdDb = -24.0f; e.params.compRatio = 8.0f; e.params.compAttackMs = 1.0f;
        std::vector<float> a(N), b(N, 0.0f);
        for (size_t i = 0; i < N; ++i) a[i] = 0.5f * std::sin(0.1f * static_cast<float>(i));
        e.params.compOn = false; Run off = run(e, a, b, 24000);
        Engine e2; setup(e2);
        e2.params.mode = static_cast<int>(Mode::Hybrid);
        e2.params.acousticLevel = 1.0f; e2.params.electronicLevel = 0.0f;
        e2.params.pad[0].thresholdDb = -6.0f;
        e2.params.compThresholdDb = -24.0f; e2.params.compRatio = 8.0f; e2.params.compAttackMs = 1.0f;
        e2.params.compOn = true; Run on = run(e2, a, b, 24000);
        CHECK(on.outPeak < off.outPeak * 0.6f, "compressor reduz o pico de um sinal forte");
    }

    { // reverb estável: não explode e decai
        Engine e; setup(e);
        e.params.reverbWet = 0.6f; e.params.reverbRoom = 1.0f;
        std::vector<float> a(N * 3, 0.0f), b(N * 3, 0.0f);
        addHit(a, 4800, 0.8f);
        Run r = run(e, a, b);
        CHECK(r.finite && r.outPeak <= 1.0f, "reverb com sala máxima fica estável e dentro de 0 dBFS");
    }

    { // botões de teste da interface
        Engine e; setup(e);
        e.params.manualHit[1] = 900;
        std::vector<float> a(4800, 0.0f), b(4800, 0.0f);
        Run r = run(e, a, b);
        CHECK(r.hits[1] == 1 && r.outPeak > 0.2f, "botão de teste do pad toca o som");
    }

    std::printf("\n%s (%d falha%s)\n", failures ? "TESTES FALHARAM" : "TODOS OS TESTES PASSARAM", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
