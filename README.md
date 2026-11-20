# Nexus Drum Trigger

App de bateria eletrônica para uso ao vivo (Nexus Works Digital): 2 microfones viram gatilhos de bateria.

## Abas
- **Módulo de bateria** – Mic 1 dispara o Pad 1, Mic 2 dispara o Pad 2. Cada pad tem som, volume, afinação, pan,
  ganho do microfone, limiar, retrigger, filtros (HP/LP), curva de dinâmica e resposta à força. Anti-vazamento entre os microfones.
- **EAD · Acústico + Eletrônico** (estilo Yamaha EAD10) – os 2 microfones formam um **par estéreo** (Mic 1 = esquerda, Mic 2 = direita,
  com controle de largura). Capta a bateria acústica, mistura com sons eletrônicos disparados pelo gatilho e aplica EQ, compressor e reverb.
- **Áudio** – escolha da interface, canais, taxa e tamanho do buffer (use 64–128 amostras para 2–5 ms).

Kits: "Salvar kit" / "Abrir kit" (.ndtkit). A última sessão é restaurada automaticamente.

## Compilar
Precisa de CMake 3.22+ e um compilador C++17. O JUCE 8 é baixado automaticamente.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release   # testes do núcleo de áudio

No GitHub, o workflow `Build` gera o app do Windows em Actions > Artifacts.
Sem placa de som/ASIO SDK o JUCE usa WASAPI no Windows; para ASIO defina `JUCE_ASIO=1` e informe o caminho do SDK da Steinberg.

## Estrutura
- `core/` – motor de áudio sem dependências (detector de batida, sons sintetizados, EQ, compressor, reverb)
- `app/` – interface JUCE
- `tests/` – testes automáticos do núcleo
