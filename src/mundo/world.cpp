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

// classifica o bioma a partir de elevação, umidade e frio
Biome World::classifyBiome(float e, float m, float cold) const {
    if (e < 0.34f) return Biome::Ocean;
    if (cold > 0.72f) return Biome::Tundra;
    if (e > 0.68f) return Biome::Mountain;
    // deserto: seco e não-tropical
    if (m < 0.30f && e > 0.42f && cold < 0.5f) return Biome::Desert;
    // pântano: muito úmido e baixo
    if (m > 0.60f && e < 0.50f) return Biome::Wetland;
    // floresta: úmido
    if (m > 0.50f) return Biome::Forest;
    // costa: baixo e arenoso
    if (e < 0.40f) return Biome::Coast;
    return Biome::Grassland;
}

float World::biomeMoisture(Biome b, Tile t) const {
    float base = equilibriumMoisture(t);
    switch (b) {
        case Biome::Ocean: return 1.f;
        case Biome::Coast: return clampf(base * 0.85f, 0.f, 1.f);
        case Biome::Wetland: return clampf(base * 1.35f + 0.15f, 0.f, 1.f);
        case Biome::Desert: return clampf(base * 0.35f, 0.f, 1.f);
        case Biome::Mountain: return clampf(base * 0.6f, 0.f, 1.f);
        case Biome::Tundra: return clampf(base * 0.7f + 0.1f, 0.f, 1.f);
        case Biome::Forest: return clampf(base * 1.1f, 0.f, 1.f);
        case Biome::Grassland: return base;
        default: return base;
    }
}

float World::biomeFertility(Biome b, Tile t) const {
    float base = equilibriumFertility(t);
    switch (b) {
        case Biome::Wetland: return clampf(base * 1.25f, 0.f, 1.f);
        case Biome::Forest: return clampf(base * 1.15f, 0.f, 1.f);
        case Biome::Desert: return clampf(base * 0.3f, 0.f, 1.f);
        case Biome::Mountain: return clampf(base * 0.4f, 0.f, 1.f);
        case Biome::Tundra: return clampf(base * 0.35f, 0.f, 1.f);
        case Biome::Grassland: return clampf(base * 1.05f, 0.f, 1.f);
        default: return base;
    }
}

Biome World::biomeAt(int x, int y) const {
    if (!inBounds(x, y)) return Biome::None;
    return at(x, y).biome;
}

int World::countBiome(Biome b) const {
    int n = 0;
    for (const auto& t : tiles_) if (t.biome == b) n++;
    return n;
}

void World::rebiome() {
    for (int y = 0; y < h_; y++) {
        for (int x = 0; x < w_; x++) {
            TileData& t = at(x, y);
            // estima elevação/umidade/frio a partir do tile existente
            float e = 0.5f, m = 0.5f, cold = 0.f;
            switch (t.type) {
                case Tile::DeepWater: e = 0.2f; m = 1.f; break;
                case Tile::Water: e = 0.32f; m = 1.f; break;
                case Tile::Sand: e = 0.4f; m = 0.25f; break;
                case Tile::Grass: e = 0.5f; m = t.moisture; break;
                case Tile::Forest: e = 0.55f; m = 0.65f; break;
                case Tile::Soil: e = 0.55f; m = 0.35f; break;
                case Tile::Rock: e = 0.75f; m = 0.1f; break;
                case Tile::Ice: e = 0.4f; m = 0.2f; cold = 0.9f; break;
                default: break;
            }
            // frio por latitude aproximada
            float lat = std::fabs(y - h_ * 0.5f) / (h_ * 0.5f);
            cold = std::max(cold, lat * lat * 1.1f);
            t.biome = classifyBiome(e, m, cold);
        }
    }
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
            // continente mais largo: menos falloff = mais terra para biomas
            e = e * 1.15f - smoothstep(clampf(d, 0.f, 1.f)) * 0.30f;

            float m = fbm(x * 0.055f + 40.f, y * 0.055f + 40.f, seed + 7u);
            float lat = std::fabs(y - cy) / (h_ * 0.5f);
            float cold = lat * lat * 1.1f + std::max(0.f, e - 0.65f) * 0.9f;

            // 1) BIOMA primeiro — define o caráter da região
            Biome b = classifyBiome(e, m, cold);

            // 2) tile dentro do bioma (com variação local)
            float local = hash01(x, y, seed + 42u) - 0.5f; // -0.5..0.5
            TileData t;
            t.biome = b;
            switch (b) {
                case Biome::Ocean:
                    t.type = (e < 0.26f) ? Tile::DeepWater : Tile::Water;
                    break;
                case Biome::Coast:
                    // praias: areia + grama rala perto da água
                    t.type = (e < 0.39f || local < -0.2f) ? Tile::Sand
                           : (local > 0.3f ? Tile::Grass : Tile::Sand);
                    break;
                case Biome::Grassland:
                    // pradaria: grama dominante, arbusto, terra seca
                    t.type = (local < -0.35f) ? Tile::Soil
                           : (local > 0.42f ? Tile::Forest : Tile::Grass); // bosques raros
                    break;
                case Biome::Forest:
                    // floresta: densa, com clareiras e terra
                    t.type = (local > 0.35f) ? Tile::Grass
                           : (local < -0.3f ? Tile::Soil : Tile::Forest);
                    break;
                case Biome::Wetland:
                    // pântano: grama encharcada, lagos rasos, terra
                    t.type = (local < -0.25f) ? Tile::Water
                           : (local > 0.4f ? Tile::Forest
                           : (local < 0.05f ? Tile::Soil : Tile::Grass));
                    break;
                case Biome::Desert:
                    // deserto: areia e rocha, quase nada vivo
                    t.type = (local > 0.25f) ? Tile::Rock
                           : (local < -0.3f ? Tile::Soil : Tile::Sand);
                    break;
                case Biome::Mountain:
                    // montanha: rocha, neve no topo, terra na base
                    t.type = (cold > 0.6f) ? Tile::Ice
                           : (local > 0.15f) ? Tile::Rock
                           : (local < -0.2f ? Tile::Soil : Tile::Rock);
                    break;
                case Biome::Tundra:
                    // tundra: gelo e grama rala
                    t.type = (local > 0.2f) ? Tile::Ice
                           : (local < -0.15f ? Tile::Soil : Tile::Grass);
                    break;
                default:
                    t.type = Tile::Grass;
                    break;
            }

            t.moisture = clampf(biomeMoisture(b, t.type) * (0.75f + m * 0.5f), 0.f, 1.f);
            t.fertility = clampf(biomeFertility(b, t.type) * (0.65f + m * 0.6f), 0.f, 1.f);
            t.fert_bonus = 0.f;
            t.nutrients = clampf(0.2f + hash01(x, y, seed + 99u) * 0.5f, 0.f, 1.f);
            t.biofilm = 0;
            t.plankton = 0;
            t.plant = Plant{};
            // árvores: floresta nasce densa, pradaria com bosques raros
            if (t.type == Tile::Forest) {
                t.trees = (uint8_t)clampf(hash01(x, y, seed + 55u) * 4.f + 1.5f, 1.f, 5.f);
            } else if (t.type == Tile::Grass && b == Biome::Grassland && hash01(x, y, seed + 66u) > 0.92f) {
                t.trees = 1; // bosque solto na pradaria
            } else if (t.type == Tile::Soil && hash01(x, y, seed + 77u) > 0.95f) {
                t.trees = 1;
            }
            t.wood = 0;
            t.cave = false;
            t.caveFire = false;
            // CAVERNAS: na montanha e rocha, algumas aberturas
            if (t.type == Tile::Rock && b == Biome::Mountain && hash01(x, y, seed + 88u) > 0.88f) {
                t.cave = true;
            } else if (t.type == Tile::Rock && hash01(x, y, seed + 88u) > 0.96f) {
                t.cave = true; // raras cavernas fora da montanha
            }
            at(x, y) = t;
        }
    }

    // plantas iniciais — densidade e tipo por bioma
    int groveAttempts = w_ * h_ / 30; // mais tentativas: biomas pedem mais variedade
    for (int i = 0; i < groveAttempts; i++) {
        int x = irand(2, w_ - 3);
        int y = irand(2, h_ - 3);
        TileData& t = at(x, y);
        if (!canHostPlant(t.type)) continue;
        Biome b = t.biome;
        // chance de ter planta varia por bioma
        float p = 0.f;
        switch (b) {
            case Biome::Forest: p = 0.65f; break;
            case Biome::Wetland: p = 0.5f; break;
            case Biome::Grassland: p = 0.4f; break;
            case Biome::Coast: p = 0.2f; break;
            case Biome::Mountain: p = 0.08f; break;
            case Biome::Desert: p = 0.05f; break;
            case Biome::Tundra: p = 0.06f; break;
            default: p = 0.15f; break;
        }
        if (!chance(p)) continue;
        if (t.moisture < 0.15f || t.fertility < 0.2f) continue;
        // tipo de planta por bioma: deserto/pântano têm mais veneno, floresta mais fruta
        float toxicChance = 0.28f;
        if (b == Biome::Desert) toxicChance = 0.55f;
        if (b == Biome::Wetland) toxicChance = 0.45f;
        if (b == Biome::Forest) toxicChance = 0.18f;
        PlantKind k = chance(toxicChance) ? PlantKind::Toxic : PlantKind::Fruit;
        spawnPlant(x, y, k, irand(1, 2));
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

int World::totalTrees() const {
    int n = 0;
    for (const auto& t : tiles_) n += t.trees;
    return n;
}

bool World::chopTree(int x, int y, int& woodOut) {
    woodOut = 0;
    if (!inBounds(x, y)) return false;
    TileData& t = at(x, y);
    if (t.trees == 0) return false;
    t.trees--;
    // madeira: árvore grande dá mais
    woodOut = 2 + (int)(t.trees > 2 ? 1 : 0) + irand(0, 1);
    t.wood = (uint8_t)std::min(15, (int)t.wood + woodOut);
    // derrubar abre espaço: nutrientes pro solo
    t.nutrients = clampf(t.nutrients + 0.04f, 0.f, 1.f);
    if (t.trees == 0 && chance(0.4f)) t.type = Tile::Grass; // clareira
    return true;
}

int World::countCaves() const {
    int n = 0;
    for (const auto& t : tiles_) if (t.cave) n++;
    return n;
}

bool World::isCave(int x, int y) const {
    if (!inBounds(x, y)) return false;
    return at(x, y).cave;
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
            float eq = biomeMoisture(t.biome, t.type);
            // estação modula umidade do bioma
            eq = clampf(eq * (0.7f + seasonRain() * 0.5f), 0.f, 1.f);
            t.moisture += (eq - t.moisture) * 0.0015f;
            t.fertility += (biomeFertility(t.biome, t.type) - t.fertility) * 0.0008f;
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

            // cadáver apodrece — MÁTERIA ORGÂNICA
            // na água apodrece 3x mais rápido (e alimenta o mar)
            // no verão (quente) 2x; no inverno (gelo) quase para
            if (t.corpse > 0) {
                float rotRate = 0.01f;
                if (t.type == Tile::Water || t.type == Tile::DeepWater) rotRate = 0.035f;
                else if (t.type == Tile::Ice) rotRate = 0.002f;
                else if (season_ == 1) rotRate = 0.02f; // verão acelera
                if (chance(rotRate)) {
                    t.corpse--;
                    float n = 0.05f;
                    if (t.type == Tile::Water || t.type == Tile::DeepWater) {
                        // na água: nutriente vira biofilme/plâncton direto
                        t.biofilm = (uint8_t)std::min(3, (int)t.biofilm + 1);
                        if (chance(0.4f)) t.plankton = (uint8_t)std::min(3, (int)t.plankton + 1);
                        t.nutrients = clampf(t.nutrients + n * 0.6f, 0.f, 1.f);
                    } else {
                        t.nutrients = clampf(t.nutrients + n, 0.f, 1.f);
                        t.fertility = clampf(t.fertility + 0.01f, 0.f, 1.f);
                    }
                    if (t.corpse == 0) t.bones = 10; // resto vira osso
                }
            }
            // ossos se degradam MUITO devagar (intemperismo) — nunca somem sem motivo
            // viram poeira mineral: nutrientes no solo, cálcio
            if (t.bones > 0 && chance(0.0005f)) {
                t.bones--;
                t.nutrients = clampf(t.nutrients + 0.03f, 0.f, 1.f);
                t.fertility = clampf(t.fertility + 0.008f, 0.f, 1.f); // cálcio melhora o solo
                // no fim, um último grão de poeira branca some
            }
            // fruta caída apodrece e vira nutriente (terra) ou plâncton (água)
            if (t.fallenFruit > 0) {
                float fruitRot = 0.012f;
                if (t.type == Tile::Water || t.type == Tile::DeepWater) fruitRot = 0.03f;
                if (chance(fruitRot)) {
                    t.fallenFruit--;
                    t.nutrients = clampf(t.nutrients + 0.04f, 0.f, 1.f);
                    if ((t.type == Tile::Water || t.type == Tile::DeepWater) && chance(0.3f))
                        t.plankton = (uint8_t)std::min(3, (int)t.plankton + 1);
                }
            }
            // fruta madura cai da planta para o chão
            if (t.plant.kind == PlantKind::Fruit && t.plant.stage >= 2 && t.fallenFruit < 7) {
                if (chance(0.0008f * (1.f + pollination_ + t.trees * 0.15f))) {
                    t.fallenFruit = (uint8_t)std::min(7, (int)t.fallenFruit + 1);
                }
            }
            // ÁRVORES DÃO FRUTA: floresta densa sem planta pode brotar fruta sozinha
            if (t.trees >= 2 && t.plant.kind == PlantKind::None && canHostPlant(t.type) &&
                t.moisture > 0.25f && t.fertility > 0.25f) {
                if (chance(0.0004f * t.trees * (1.f + pollination_))) {
                    t.plant.kind = PlantKind::Fruit;
                    t.plant.stage = 2; // árvore já madura, fruta pronta
                    t.plant.gene.yield = clampf(gauss(1.2f, 0.2f), 0.5f, 2.f);
                    t.plant.gene.growth = clampf(gauss(1.3f, 0.15f), 0.5f, 2.f);
                }
            }
            // planta seca/morta devolve um pouco de nutrientes
            // (já tratado no decay de plantas abaixo, quando kind vira None)

            // GRAMA ORGÂNICA: grama úmida e fértil pode virar floresta (sucessão);
            // floresta seca vira grama; deserto não muda
            if (t.type == Tile::Grass && t.moisture > 0.55f && t.fertility > 0.5f &&
                t.biome != Biome::Desert && chance(0.0003f)) {
                t.type = Tile::Forest;
            } else if (t.type == Tile::Forest && (t.moisture < 0.2f || t.fertility < 0.15f) &&
                       chance(0.0004f)) {
                t.type = Tile::Grass;
            }

            // ---- ÁRVORES VIVAS ----
            if (t.trees > 0) {
                // fogo mata árvore (abre clareira)
                if (fire_[y * w_ + x] > 0 && chance(0.06f)) {
                    t.trees--;
                    t.nutrients = clampf(t.nutrients + 0.05f, 0.f, 1.f); // madeira vira cinza
                    if (t.trees == 0 && chance(0.5f)) t.type = Tile::Grass; // clareira
                }
                // seca derruba árvore fraca
                else if (t.moisture < 0.12f && chance(0.008f * (1.f + seasonTemp() * -0.5f))) {
                    t.trees--;
                    t.nutrients = clampf(t.nutrients + 0.03f, 0.f, 1.f);
                    if (t.trees == 0) t.type = Tile::Grass;
                }
                // velhice: árvore muito velha cai (não temos idade individual, mas
                // tiles densos "envelhecem" — abre espaço pra nova geração)
                else if (t.trees >= 4 && chance(0.0002f)) {
                    t.trees--;
                    t.nutrients = clampf(t.nutrients + 0.04f, 0.f, 1.f);
                }
                // madeira envelhece e cai naturalmente no chão
                if (chance(0.0005f)) {
                    t.wood = (uint8_t)std::max(0, (int)t.wood - 1);
                    t.nutrients = clampf(t.nutrients + 0.01f, 0.f, 1.f);
                }
                // árvore madura alimenta o solo (raízes, folhas)
                if (chance(0.0008f * t.trees)) {
                    t.fertility = clampf(t.fertility + 0.005f, 0.f, 1.f);
                }
            }
            // crescimento: floresta úmida ganha árvore; solo bom perto de árvore também
            if (t.type == Tile::Forest && t.trees < 5 && t.moisture > 0.35f && t.fertility > 0.3f) {
                float grow = 0.00015f * t.moisture * (0.5f + seasonRain());
                if (t.biome == Biome::Forest) grow *= 1.5f;
                if (chance(grow)) t.trees = (uint8_t)std::min(5, (int)t.trees + 1);
            }
            // grama com árvore solitária pode virar floresta (o bosque cresce)
            if (t.trees >= 2 && t.type == Tile::Grass && t.moisture > 0.4f && chance(0.0002f)) {
                t.type = Tile::Forest;
            }

            // ---- oceano vivo: biofilme e plancton em TODA a água ----
            // abismo também produz (menos luz, mas o suficiente pra vida se espalhar)
            if (t.type == Tile::Water || t.type == Tile::DeepWater) {
                float bioLight = (t.type == Tile::Water) ? (0.3f + light * 0.7f)
                                                         : (0.22f + light * 0.4f);
                if (t.biofilm < 3 && t.nutrients > 0.08f) {
                    if (chance(0.02f * t.nutrients * bioLight)) t.biofilm++;
                }
                // plancton: se multiplica sozinho quando tem alimento e luz
                if (t.biofilm > 0) {
                    t.nutrients = clampf(t.nutrients - 0.00004f * t.biofilm, 0.f, 1.f);
                    float lit = (t.type == Tile::Water) ? light : light * 0.55f; // abismo não é deserto
                    if (t.plankton < 3 && t.nutrients > 0.03f) {
                        // REPRODUÇÃO: chance alta — o plancton se multiplica
                        float grow = 0.035f * lit * (0.5f + t.nutrients) * t.biofilm;
                        if (t.type == Tile::DeepWater) grow *= 0.8f; // era 0.5 — abismo vivo
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
                    else {
                        // planta morre e devolve nutrientes ao solo
                        t.nutrients = clampf(t.nutrients + 0.04f + p.gene.yield * 0.03f, 0.f, 1.f);
                        p.kind = PlantKind::None;
                    }
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
            t.moisture = biomeMoisture(t.biome, type);
            t.fertility = biomeFertility(t.biome, type);
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

const char* biomeName(Biome b) {
    switch (b) {
        case Biome::Ocean: return "Oceano";
        case Biome::Coast: return "Costa";
        case Biome::Grassland: return "Pradaria";
        case Biome::Forest: return "Floresta";
        case Biome::Wetland: return "Pantano";
        case Biome::Desert: return "Deserto";
        case Biome::Mountain: return "Montanha";
        case Biome::Tundra: return "Tundra";
        default: return "-";
    }
}

}
