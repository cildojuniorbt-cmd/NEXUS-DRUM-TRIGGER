# Nexus Drum Trigger

App de bateria eletrônica para uso ao vivo (Nexus Works Digital): 2 microfones viram gatilhos de bateria.

## Abas
- **Trigger** (2 canais) – Entrada 1 e 2 disparam 2 pads. Cada pad escolhe a saída: mix estéreo, somente L ou somente R (para separar na mesa).
- **Bateria eletrônica** (até 8 canais) – 8 pads (bumbo, caixa, chimbal, tom 1, tom 2, surdo, prato 1 e 2), cada um ligado a uma entrada da
  interface e, se quiser, a uma saída própria (até 8) para gravar as peças separadas. Anti-vazamento entre as entradas.
- **EAD · Acústico + Eletrônico** – os 2 microfones formam um par estéreo. O app reconhece cada peça (bumbo, caixa, chimbal, tom 1, tom 2, prato)
  pela "impressão digital" do espectro e funciona com 2 peças juntas. **Calibrar** ensina ao app o som da sua bateria (8 batidas por peça).
  Botão **Acústico↔Kit** mistura o som real com o kit; kits prontos já mixados (EQ, compressor, reverb); **Anti-retorno** (experimental)
  tira dos microfones o som que o app toca nas caixas.
- **Áudio** – interface, canais (até 8 entradas e 8 saídas), taxa e buffer (use 64–128 amostras).
- **Aparência** – cores e fundo.

**Aprender**: aperte o botão e bata na peça — o app seleciona o pad (ou a peça do EAD) correspondente. **Hold**: tempo de leitura do pico de cada batida (mais tempo = força mais precisa, com um pouco mais de atraso).

Biblioteca de sons: botão **Importar sons (WAV)** (WAV, AIFF, FLAC). Kits: "Salvar kit" / "Abrir kit" (.ndtkit). A última sessão é restaurada.

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
