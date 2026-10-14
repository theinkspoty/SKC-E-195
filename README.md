# SKC E-195

Simulação de vida independente em C++ — um god game onde você influencia, mas não controla.

## O que é

Um mundo pixel onde a vida **emerge sozinha** quando as condições existem:

```
Minerais + água → Microbioma → Plâncton → Medusa → Peixe
→ Anfíbio → Herbívoro → Lobo / Carroneiro / Falcon
→ Pensantes → Comunidades → Cultura
```

Você é um deus **limitado**: pode pedir chuva, fruta, asteroide, geada — mas o mundo decide se aceita.

## Compilar

```bash
cmake -B build -S .
cmake --build build
./build/skc_e195
```

Dependências: SDL2, SDL2_ttf, CMake, g++.

## Controles

| Tecla | Ação |
|---|---|
| WASD / setas | mover câmera |
| Scroll | zoom |
| Espaço | pausar |
| Z / X | velocidade |
| Clique | inspecionar ser |
| Tab | próximo ser |
| 1 2 3 4 | influência divina (no mouse) |
| E | modo edição de mundo |
| R | gerar mundo novo |
| L | Escada da Vida |
| H | ajuda |
| ESC | sair |

## Sistemas

- **Ecossistema**: peixes em cardume, anfíbios na margem, predadores de mar/terra/ar
- **Plantas com genoma**: evoluem por sementes, veneno vs fruta, expansão de território
- **Criaturas pensantes**: personalidade, emoções, ideias, moral, mentira, justiça
- **Comunidades**: fogueira, toca, troca de ideias entre tribos, casamento
- **Família**: sobrenome herdado, laços fortes, filhos herdam cultura
- **Dia/noite**: mamíferos dormem, lobos caçam de noite, falcões pousam
- **Ciclo da morte**: carne → ossos → nutrientes → nova vida

## Sprites

Criaturas e plantas são **quadrados coloridos** (pixel art procedural). Cada espécie tem sua cor:

| Ser | Cor |
|---|---|
| Peixe | azul-violeta |
| Medusa | ciano |
| Anfíbio | verde |
| Herbívoro | marrom claro |
| Lobo | vermelho |
| Falcão | branco |
| Pensante ♂ / ♀ | azul / rosa |
| Fruta / Veneno / Cura | amarelo / roxo / dourado |

## Estrutura

```
src/
  main.cpp          loop, render, input
  rng.hpp           aleatoriedade
  mundo/            terreno, clima, plantas, ciclo da morte
  animais/          fauna: cardume, caça, nichos mar/terra/ar
  pensantes/        criaturas: emoções, ideias, moral, família
  deus/             influência divina
  ui/               HUD + sprites pixel art
```
