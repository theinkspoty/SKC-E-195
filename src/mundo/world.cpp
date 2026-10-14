#include "world.hpp"
#include "rng.hpp"
#include <cmath>
#include <algorithm>

namespace skc {

static unsigned hash2(int x, int y, unsigned seed) {
    unsigned h = seed;
    h ^= (unsigned)x * 374761393u;
    h ^= (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float hash01(int x, int y, unsigned seed) {
    return (hash2(x, y, seed) & 0xFFFFFF) / (float)0xFFFFFF;
}

static float smoothstep(float t) {
    return t * t * (3.f - 2.f * t);
}

static float valueNoise(float x, float y, unsigned seed) {
    int xi = (int)std::floor(x);
    int yi = (int)std::floor(y);
    float xf = x - xi;
    float yf = y - yi;
    float v00 = hash01(xi, yi, seed);
    float v10 = hash01(xi + 1, yi, seed);
    float v01 = hash01(xi, yi + 1, seed);
    float v11 = hash01(xi + 1, yi + 1, seed);
    float sx = smoothstep(xf);
    float sy = smoothstep(yf);
    float a = v00 + (v10 - v00) * sx;
    float b = v01 + (v11 - v01) * sx;
    return a + (b - a) * sy;
}

static float fbm(float x, float y, unsigned seed) {
    float v = 0.f, amp = 0.5f, f = 1.f;
    for (int i = 0; i < 5; i++) {
        v += amp * valueNoise(x * f, y * f, seed + i * 101u);
        f *= 2.f;
        amp *= 0.5f;
    }
    return v;
}

World::World(int w, int h) : w_(w), h_(h), tiles_(w * h), fire_(w * h, 0) {}

bool World::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < w_ && y < h_;
}

TileData& World::at(int x, int y) {
    return tiles_[y * w_ + x];
}

const TileData& World::at(int x, int y) const {
    return tiles_[y * w_ + x];
}

float World::equilibriumMoisture(Tile t) const {
    switch (t) {
        case Tile::DeepWater: return 1.f;
        case Tile::Water: return 1.f;
        case Tile::Sand: return 0.22f;
        case Tile::Grass: return 0.5f;
        case Tile::Forest: return 0.68f;
        case Tile::Rock: return 0.12f;
        case Tile::Ice: return 0.15f;
        case Tile::Soil: return 0.42f;
        default: return 0.4f;
    }
}

float World::equilibriumFertility(Tile t) const {
    switch (t) {
        case Tile::Grass: return 0.6f;
        case Tile::Forest: return 0.65f;
        case Tile::Soil: return 0.85f;
        case Tile::Sand: return 0.1f;
        case Tile::Rock: return 0.05f;
        case Tile::Ice: return 0.02f;
        case Tile::Water: return 0.05f;
        case Tile::DeepWater: return 0.f;
        default: return 0.3f;
    }
}

bool World::canHostPlant(Tile t) const {
    return t == Tile::Grass || t == Tile::Forest || t == Tile::Soil;
}

void World::generate(unsigned seed) {
    float cx = w_ * 0.5f;
    float cy = h_ * 0.5f;
    for (int y = 0; y < h_; y++) {
        for (int x = 0; x < w_; x++) {
            float e = fbm(x * 0.035f, y * 0.035f, seed);
            float dx = (x - cx) / (w_ * 0.5f);
            float dy = (y - cy) / (h_ * 0.5f);
            float d = std::sqrt(dx * dx + dy * dy);
            e -= smoothstep(clampf(d, 0.f, 1.f)) * 0.38f;

            float m = fbm(x * 0.055f + 40.f, y * 0.055f + 40.f, seed + 7u);
            float lat = std::fabs(y - cy) / (h_ * 0.5f);
            float cold = lat * lat * 1.1f + std::max(0.f, e - 0.65f) * 0.9f;

            TileData t;
            if (e < 0.26f) t.type = Tile::DeepWater;
            else if (e < 0.34f) t.type = Tile::Water;
            else if (e < 0.385f) t.type = Tile::Sand;
            else if (cold > 0.72f) t.type = Tile::Ice;
            else if (e > 0.7f) t.type = Tile::Rock;
            else if (m > 0.56f) t.type = Tile::Forest;
            else if (m < 0.34f && e > 0.48f) t.type = Tile::Soil;
            else t.type = Tile::Grass;

            t.moisture = clampf(equilibriumMoisture(t.type) * (0.7f + m * 0.6f), 0.f, 1.f);
            t.fertility = clampf(equilibriumFertility(t.type) * (0.6f + m * 0.7f), 0.f, 1.f);
            t.fert_bonus = 0.f;
            t.nutrients = clampf(0.2f + hash01(x, y, seed + 99u) * 0.5f, 0.f, 1.f);
            t.biofilm = 0;
            t.plankton = 0;
            t.plant = Plant{};
            at(x, y) = t;
        }
    }

    int groveAttempts = w_ * h_ / 40;
    for (int i = 0; i < groveAttempts; i++) {
        int x = irand(2, w_ - 3);
        int y = irand(2, h_ - 3);
        TileData& t = at(x, y);
        if (!canHostPlant(t.type)) continue;
        if (t.moisture < 0.25f || t.fertility < 0.3f) continue;
        if (chance(0.45f)) {
            PlantKind k = chance(0.28f) ? PlantKind::Toxic : PlantKind::Fruit;
            spawnPlant(x, y, k, irand(1, 2));
        }
    }
}

bool World::spawnPlant(int x, int y, PlantKind kind, int stage) {
    if (!inBounds(x, y) || kind == PlantKind::None) return false;
    TileData& t = at(x, y);
    if (!canHostPlant(t.type)) return false;
    t.plant.kind = kind;
    t.plant.stage = (uint8_t)clampf((float)stage, 0.f, 2.f);
    return true;
}

float World::lightLevel() const {
    // timeOfDay: 0..1 ciclo completo
    // 0.00-0.08 amanhecer, 0.08-0.42 dia, 0.42-0.50 entardecer, 0.50-1.00 noite
    float t = timeOfDay_;
    if (t < 0.08f) return 0.25f + (t / 0.08f) * 0.75f;           // amanhecer
    if (t < 0.42f) return 1.f;                                     // dia
    if (t < 0.50f) return 1.f - ((t - 0.42f) / 0.08f) * 0.75f;    // entardecer
    if (t < 0.58f) return 0.25f - ((t - 0.50f) / 0.08f) * 0.2f;   // caindo
    if (t < 0.92f) return 0.05f;                                   // noite cheia
    return 0.05f + ((t - 0.92f) / 0.08f) * 0.2f;                   // madrugada
}

const char* World::phaseName() const {
    float t = timeOfDay_;
    if (t < 0.08f) return "Amanhecer";
    if (t < 0.42f) return "Dia";
    if (t < 0.50f) return "Entardecer";
    if (t < 0.92f) return "Noite";
    return "Madrugada";
}

const char* World::seasonName() const {
    switch (season_) {
        case 0: return "Primavera";
        case 1: return "Verao";
        case 2: return "Outono";
        case 3: return "Inverno";
        default: return "?";
    }
}

float World::seasonTemp() const {
    switch (season_) {
        case 0: return 0.2f;  // primavera amena
        case 1: return 0.8f;  // verão quente
        case 2: return -0.1f; // outono fresco
        case 3: return -0.8f; // inverno frio
        default: return 0.f;
    }
}

float World::seasonRain() const {
    switch (season_) {
        case 0: return 0.7f;  // primavera chuvosa
        case 1: return 0.25f; // verão seco
        case 2: return 0.5f;  // outono médio
        case 3: return 0.35f; // inverno (nevado)
        default: return 0.5f;
    }
}

void World::advanceSeason(int day) {
    // cada estação dura 15 dias
    int newSeason = (day / 15) % 4;
    if (newSeason != season_) {
        season_ = newSeason;
    }
}

bool World::onFire(int x, int y) const {
    return inBounds(x, y) && fire_[y * w_ + x] > 0;
}

void World::startFire(int x, int y) {
    if (inBounds(x, y)) fire_[y * w_ + x] = 12;
}

int World::countFireTiles() const {
    int n = 0;
    for (auto v : fire_) if (v > 0) n++;
    return n;
}

void World::updateFires(float dt) {
    // fogo se espalha pra vizinho com planta/grama, e apaga sozinho
    std::vector<int> toLight;
    for (int y = 0; y < h_; y++) {
        for (int x = 0; x < w_; x++) {
            int i = y * w_ + x;
            if (fire_[i] == 0) continue;
            fire_[i]--;
            TileData& t = tiles_[i];
            // queima planta
            if (t.plant.kind != PlantKind::None && chance(0.15f)) {
                t.plant = Plant{};
                t.fertility = clampf(t.fertility + 0.08f, 0.f, 1.f);
                t.nutrients = clampf(t.nutrients + 0.06f, 0.f, 1.f);
            }
            // fogo espalha pra vizinho com combustível
            if (chance(0.12f)) {
                int nx = x + irand(-1, 1);
                int ny = y + irand(-1, 1);
                if (inBounds(nx, ny)) {
                    int ni = ny * w_ + nx;
                    if (fire_[ni] == 0) {
                        Tile tt = tiles_[ni].type;
                        bool fuel = (tt == Tile::Forest || tt == Tile::Grass || tt == Tile::Soil);
                        if (fuel && tiles_[ni].plant.kind != PlantKind::None) {
                            toLight.push_back(ni);
                        }
                    }
                }
            }
            // fogo morre
            if (fire_[i] == 0) {
                t.fertility = clampf(t.fertility + 0.05f, 0.f, 1.f);
            }
        }
    }
    for (int i : toLight) fire_[i] = 10;
}

// relâmpago natural: raro, mas acontece em tempestades
static bool g_lightning = false;
void triggerLightning() { g_lightning = true; }

PlantGene World::averagePlantGene() const {
    PlantGene avg;
    double g = 0, y = 0, tox = 0, hard = 0;
    int n = 0;
    for (const auto& t : tiles_) {
        if (t.plant.kind == PlantKind::None) continue;
        g += t.plant.gene.growth;
        y += t.plant.gene.yield;
        tox += t.plant.gene.toxicity;
        hard += t.plant.gene.hardiness;
        n++;
    }
    if (n == 0) return avg;
    avg.growth = (float)(g / n);
    avg.yield = (float)(y / n);
    avg.toxicity = (float)(tox / n);
    avg.hardiness = (float)(hard / n);
    return avg;
}

int World::plantCount() const {
    int n = 0;
    for (const auto& t : tiles_) if (t.plant.kind != PlantKind::None) n++;
    return n;
}

int World::curePlantCount() const {
    int n = 0;
    for (const auto& t : tiles_) {
        if (t.plant.kind != PlantKind::None && t.plant.gene.cure > 0.25f) n++;
    }
    return n;
}

void World::update() {
    float light = lightLevel();
    float temp = seasonTemp();    // estação
    float rain = seasonRain();

    // chuva natural da estação
    if (chance(0.0008f * rain)) {
        int rx = irand(0, w_ - 1);
        int ry = irand(0, h_ - 1);
        // apaga fogo e molha
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++)
                if (inBounds(rx + dx, ry + dy)) {
                    fire_[(ry + dy) * w_ + (rx + dx)] = 0;
                    TileData& t = at(rx + dx, ry + dy);
                    t.moisture = clampf(t.moisture + 0.15f, 0.f, 1.f);
                    t.nutrients = clampf(t.nutrients + 0.02f, 0.f, 1.f);
                }
    }

    // relâmpago natural (raro, mais em verão seco)
    float lightningChance = 0.00002f + (temp > 0.5f ? 0.00004f : 0.f);
    if (chance(lightningChance)) {
        for (int i = 0; i < 40; i++) {
            int x = irand(0, w_ - 1);
            int y = irand(0, h_ - 1);
            if (at(x, y).type == Tile::Forest && at(x, y).plant.kind != PlantKind::None) {
                startFire(x, y);
                break;
            }
        }
    }

    updateFires(1.f);

    // polinização: plantas maduras liberam polinizadores
    if (season_ == 0 || season_ == 1) { // primavera/verão
        pollination_ = clampf(pollination_ + 0.0001f, 0.f, 1.f);
    } else {
        pollination_ = clampf(pollination_ - 0.0002f, 0.f, 1.f);
    }
    for (int y = 0; y < h_; y++) {
        for (int x = 0; x < w_; x++) {
            TileData& t = at(x, y);
            float eq = equilibriumMoisture(t.type);
            t.moisture += (eq - t.moisture) * 0.0015f;
            t.fertility += (equilibriumFertility(t.type) - t.fertility) * 0.0008f;
            t.fert_bonus *= 0.9996f;
            t.fertility = clampf(t.fertility + t.fert_bonus * 0.0004f, 0.f, 1.f);

            // intemperismo + chuva: minerais chegam à água
            if (t.type == Tile::Rock && chance(0.003f)) {
                t.nutrients = clampf(t.nutrients + 0.04f, 0.f, 1.f);
            }
            // água recebe minerais da chuva/escoamento sempre
            if (t.type == Tile::Water || t.type == Tile::DeepWater) {
                t.nutrients = clampf(t.nutrients + 0.0003f * (0.4f + light * 0.3f), 0.f, 1.f);
            } else {
                t.nutrients += (0.3f - t.nutrients) * 0.0002f;
            }

            // cadáver apodrece: devolve nutrientes ao solo; vira ossos
            if (t.corpse > 0) {
                if (chance(0.01f)) {
                    t.corpse--;
                    t.nutrients = clampf(t.nutrients + 0.05f, 0.f, 1.f);
                    t.fertility = clampf(t.fertility + 0.01f, 0.f, 1.f);
                    if (t.corpse == 0) t.bones = 10; // resto vira osso
                }
            }
            // ossos apodrecem muito devagar
            if (t.bones > 0 && chance(0.001f)) {
                t.bones--;
                t.nutrients = clampf(t.nutrients + 0.02f, 0.f, 1.f);
            }

            // ---- oceano vivo: biofilme e plancton em TODA a água ----
            if (t.type == Tile::Water || t.type == Tile::DeepWater) {
                // biofilme cresce em água rasa E profunda (devagar no abismo)
                float bioLight = (t.type == Tile::Water) ? (0.3f + light * 0.7f) : (0.15f + light * 0.25f);
                if (t.biofilm < 3 && t.nutrients > 0.08f) {
                    if (chance(0.02f * t.nutrients * bioLight)) t.biofilm++;
                }
                // plancton: se multiplica sozinho quando tem alimento e luz
                if (t.biofilm > 0) {
                    t.nutrients = clampf(t.nutrients - 0.00004f * t.biofilm, 0.f, 1.f);
                    float lit = (t.type == Tile::Water) ? light : light * 0.35f;
                    if (t.plankton < 3 && t.nutrients > 0.03f) {
                        // REPRODUÇÃO: chance alta — o plancton se multiplica
                        float grow = 0.035f * lit * (0.5f + t.nutrients) * t.biofilm;
                        if (t.type == Tile::DeepWater) grow *= 0.5f;
                        if (chance(grow)) t.plankton++;
                    }
                }
                // plancton vivo também se divide se houver luz e nutrientes
                if (t.plankton > 0 && t.plankton < 3 && t.nutrients > 0.05f) {
                    if (chance(0.015f * light * t.nutrients)) t.plankton++;
                }
                if (t.plankton > 0) {
                    t.nutrients = clampf(t.nutrients - 0.00005f * t.plankton, 0.f, 1.f);
                    if (chance(0.001f * t.plankton)) {
                        t.fertility = clampf(t.fertility + 0.002f, 0.f, 1.f);
                    }
                }
            } else if (t.biofilm > 0 || t.plankton > 0) {
                if (t.type != Tile::Ice) {
                    if (chance(0.015f)) { if (t.plankton) t.plankton--; else t.biofilm--; }
                }
            }

            // plancton se espalha para a água vizinha (colonização oceânica)
            if (t.plankton >= 2 && chance(0.003f * light)) {
                int nx = x + irand(-1, 1);
                int ny = y + irand(-1, 1);
                if (inBounds(nx, ny)) {
                    TileData& n = at(nx, ny);
                    if ((n.type == Tile::Water || n.type == Tile::DeepWater) &&
                        n.plankton < 3 && n.biofilm > 0) {
                        n.plankton++;
                    }
                }
            }

            Plant& p = t.plant;
            if (p.kind != PlantKind::None) {
                // hardiness protege da seca
                float dry = 0.08f * (2.f - p.gene.hardiness);
                if (t.moisture < dry && chance(0.012f * (2.f - p.gene.hardiness))) {
                    if (p.stage > 0) p.stage--;
                    else p.kind = PlantKind::None;
                } else if (p.stage < 2 && t.moisture > 0.2f && t.fertility > 0.15f) {
                    // fotossíntese: cresce de dia, afetada por estação
                    float g = 0.00022f * t.moisture * (0.4f + t.fertility + t.fert_bonus);
                    g *= p.gene.growth;
                    g *= (0.05f + light * 0.95f);
                    g *= (1.f + t.plankton * 0.15f);
                    // estação: inverno quase não cresce, verão acelera
                    g *= clampf(0.3f + temp * 0.7f, 0.1f, 1.5f);
                    // polinização ajuda frutificar
                    g *= (1.f + pollination_ * 0.3f);
                    if (chance(g)) p.stage++;
                }
                if (p.stage == 2 && chance(0.00003f * p.gene.growth)) {
                    p.kind = PlantKind::None;
                    p.stage = 0;
                }
                // semente com genoma mutado
                float spreadBoost = 1.f + pollination_ * 0.5f; // polinizadores ajudam
                if (p.stage == 2 && chance((0.00005f + p.gene.expansion * 0.0001f) * spreadBoost)) {
                    int sx = x + irand(-4, 4);
                    int sy = y + irand(-4, 4);
                    if (inBounds(sx, sy)) {
                        TileData& s = at(sx, sy);
                        if ((s.type == Tile::Grass || s.type == Tile::Forest || s.type == Tile::Soil) &&
                            s.moisture > 0.2f && s.fertility > 0.2f) {
                            // expansão: invade se vazio, ou se o vizinho for mais fraco
                            bool empty = s.plant.kind == PlantKind::None;
                            bool weaker = s.plant.kind != PlantKind::None &&
                                          s.plant.gene.expansion < p.gene.expansion * 0.7f;
                            if (empty || weaker) {
                                s.plant.gene = p.gene;
                                s.plant.gene.growth = clampf(gauss(p.gene.growth, 0.08f), 0.3f, 2.5f);
                                s.plant.gene.yield = clampf(gauss(p.gene.yield, 0.1f), 0.3f, 2.5f);
                                s.plant.gene.toxicity = clampf(gauss(p.gene.toxicity, 0.06f), 0.f, 1.f);
                                s.plant.gene.hardiness = clampf(gauss(p.gene.hardiness, 0.08f), 0.3f, 2.f);
                                s.plant.gene.cure = clampf(gauss(p.gene.cure, 0.05f), 0.f, 1.f);
                                s.plant.gene.animal_spread = clampf(gauss(p.gene.animal_spread, 0.06f), 0.f, 1.f);
                                s.plant.gene.expansion = clampf(gauss(p.gene.expansion, 0.06f), 0.f, 1.f);
                                s.plant.gene.spore_range = clampf(gauss(p.gene.spore_range, 0.06f), 0.f, 1.f);
                                PlantKind k = PlantKind::Fruit;
                                if (chance(p.gene.toxicity)) k = PlantKind::Toxic;
                                s.plant.kind = k;
                                s.plant.stage = 0;
                            }
                        }
                    }
                }
            } else if (canHostPlant(t.type) && t.moisture > 0.22f && t.fertility > 0.28f) {
                // plantas nascem de semente de planta-mãe por perto
                float shore = (t.plankton > 0 || t.biofilm > 0) ? 0.2f : 0.f;
                float nearParent = 0.f;
                for (int dy = -5; dy <= 5 && nearParent < 0.5f; dy += 2) {
                    for (int dx = -5; dx <= 5; dx += 2) {
                        int nx = x + dx, ny = y + dy;
                        if (!inBounds(nx, ny)) continue;
                        if (at(nx, ny).plant.kind != PlantKind::None) {
                            nearParent = 1.f - std::sqrt((float)(dx * dx + dy * dy)) / 7.f;
                            break;
                        }
                    }
                }
                if (nearParent <= 0.f) continue;
                float g = 0.000012f * nearParent * t.moisture * t.fertility * (1.f + t.fert_bonus * 3.f);
                g *= (0.2f + light * 0.8f);
                g *= (1.f + shore);
                g *= (1.f + pollination_ * 0.4f); // polinização facilita
                g *= clampf(0.3f + temp * 0.7f, 0.1f, 1.5f);
                if (chance(g)) {
                    PlantGene gene;
                    gene.growth = clampf(gauss(1.f, 0.1f), 0.4f, 2.f);
                    gene.yield = clampf(gauss(1.f, 0.12f), 0.4f, 2.f);
                    gene.toxicity = clampf(gauss(0.3f, 0.1f), 0.f, 0.9f);
                    gene.hardiness = clampf(gauss(1.f, 0.1f), 0.4f, 1.8f);
                    gene.cure = chance(0.06f) ? clampf(gauss(0.5f, 0.2f), 0.2f, 1.f) : clampf(gauss(0.05f, 0.05f), 0.f, 0.3f);
                    // estratégia de conquista
                    gene.animal_spread = clampf(gauss(0.35f, 0.2f), 0.05f, 0.95f);
                    gene.expansion = clampf(gauss(0.35f, 0.2f), 0.05f, 0.95f);
                    gene.spore_range = clampf(gauss(0.4f, 0.2f), 0.05f, 0.95f);
                    // venenosas tendem a espalhar mais barato (via morte)
                    if (gene.toxicity > 0.5f) {
                        gene.animal_spread = clampf(gene.animal_spread + 0.2f, 0.f, 1.f);
                        gene.expansion = clampf(gene.expansion + 0.15f, 0.f, 1.f);
                    }
                    p.gene = gene;
                    p.kind = chance(gene.toxicity) ? PlantKind::Toxic : PlantKind::Fruit;
                    p.stage = 0;
                }
            }
        }
    }
}

float World::avgBiofilm() const {
    double s = 0;
    for (const auto& t : tiles_) s += t.biofilm;
    return (float)(s / tiles_.size()) / 3.f;
}

float World::avgPlankton() const {
    double s = 0;
    for (const auto& t : tiles_) s += t.plankton;
    return (float)(s / tiles_.size()) / 3.f;
}

float World::plantDensity(int cx, int cy, int radius) const {
    int n = 0, tot = 0;
    for (int y = cy - radius; y <= cy + radius; y += 2) {
        for (int x = cx - radius; x <= cx + radius; x += 2) {
            if (!inBounds(x, y)) continue;
            tot++;
            if (at(x, y).plant.kind != PlantKind::None) n++;
        }
    }
    return tot > 0 ? (float)n / tot : 0.f;
}

int World::countPlanktonRich(int minStage) const {
    int n = 0;
    for (const auto& t : tiles_) {
        if (t.plankton >= minStage) n++;
    }
    return n;
}

float World::globalHumidity() const {
    double sum = 0.0;
    for (const auto& t : tiles_) sum += t.moisture;
    return (float)(sum / tiles_.size());
}

float World::averageMoisture(int cx, int cy, int radius) const {
    double sum = 0.0;
    int n = 0;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (!inBounds(x, y)) continue;
            sum += at(x, y).moisture;
            n++;
        }
    }
    return n > 0 ? (float)(sum / n) : 0.f;
}

void World::paint(int cx, int cy, int radius, Tile type) {
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (!inBounds(x, y)) continue;
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy > radius * radius) continue;
            TileData& t = at(x, y);
            t.type = type;
            t.moisture = equilibriumMoisture(type);
            t.fertility = equilibriumFertility(type);
            if (!canHostPlant(type)) {
                t.plant = Plant{};
            }
        }
    }
}

bool World::walkable(float x, float y) const {
    int tx = (int)x, ty = (int)y;
    if (!inBounds(tx, ty)) return false;
    Tile t = at(tx, ty).type;
    return t != Tile::DeepWater && t != Tile::Water;
}

bool World::nearWater(float x, float y, float radius) const {
    int r = (int)radius;
    int cx = (int)x, cy = (int)y;
    for (int ty = cy - r; ty <= cy + r; ty++) {
        for (int tx = cx - r; tx <= cx + r; tx++) {
            if (!inBounds(tx, ty)) continue;
            Tile t = at(tx, ty).type;
            if (t == Tile::Water || t == Tile::DeepWater) return true;
        }
    }
    return false;
}

const char* tileName(Tile t) {
    switch (t) {
        case Tile::DeepWater: return "Abismo";
        case Tile::Water: return "Agua";
        case Tile::Sand: return "Areia";
        case Tile::Grass: return "Grama";
        case Tile::Forest: return "Floresta";
        case Tile::Rock: return "Rocha";
        case Tile::Ice: return "Gelo";
        case Tile::Soil: return "Terra";
        default: return "?";
    }
}

}
