# SKC E-195

Simulação de vida independente em C++ — um god game onde você influencia, mas não controla.

## O que é

Um mundo pixel onde a vida **emerge sozinha** quando as condições existem, em **eras geológicas** como na Terra:

```
Silêncio → Microbioma → Explosão Cambriana → Oceanos
→ Terra Verde (Devoniano) → Caça → Mente → Cultura
```

- **Explosão Cambriana**: o mar ferve, formas novas aparecem de uma vez, predadores gigantes surgem
- **Evolução por habitat**: quem vive na água vira água, quem vive na terra vira terra — os filhos herdam onde os pais viveram e acharam comida
- **Pressão evolutiva** cresce com as gerações: quanto mais tempo, mais os genes mudam

Você é um deus **limitado**: pode pedir chuva, fruta, asteroide, geada — mas o mundo decide se aceita.

## Compilar

```bash
cmake -B build -S .
cmake --build build
./build/skc_e195
```

Dependências: SDL2, SDL2_ttf, CMake, g++.

Debug de UI: `SKC_SHOT=/tmp/ui.bmp SKC_SMOKE_FRAMES=200 ./build/skc_e195` salva um frame.

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
- **Evolução do nado**: terrestres perto da água mutam o gene de nado — urso pesca, baleia volta pro mar
- **Plantas com genoma**: evoluem por sementes, veneno vs fruta, expansão de território
- **Criaturas pensantes**: personalidade, emoções, ideias, moral, mentira, justiça
- **Comunidades**: fogueira, toca, troca de ideias entre tribos, casamento
- **Família**: sobrenome herdado, laços fortes, filhos herdam cultura
- **Dia/noite**: mamíferos dormem, lobos e tubarões caçam de noite, falcões pousam
- **Ciclo da morte**: carne → ossos → nutrientes → nova vida

## Sprites

Criaturas e plantas são **quadrados coloridos** (pixel art procedural). Cada espécie tem sua cor:

| Ser | Cor |
|---|---|
| Peixe | azul-violeta |
| Medusa | ciano |
| Anfíbio | verde |
| Tartaruga | verde-oliva |
| Herbívoro | marrom claro |
| Lobo | vermelho |
| Urso | marrom escuro |
| Falcão | branco |
| Piscador | azul escuro |
| Tubarão | cinza-azulado |
| Baleia | azul-petróleo |
| Pensante ♂ / ♀ | azul / rosa |
| Fruta / Veneno / Cura | amarelo / roxo / dourado |

Terrestres perto da água evoluem o **gene de nado** — alguns voltam pro mar e viram Baleias.

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
