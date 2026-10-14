#include "fauna.hpp"
#include "rng.hpp"
#include <cmath>
#include <algorithm>

namespace skc {

const char* faunaName(FaunaType t) {
    switch (t) {
        case FaunaType::Medusa: return "Medusa";
        case FaunaType::Fish: return "Peixe";
        case FaunaType::Amphibian: return "Anfibio";
        case FaunaType::Herbivore: return "Herbivoro";
        case FaunaType::Scavenger: return "Carroneiro";
        case FaunaType::SeaPredator: return "Piscador";
        case FaunaType::LandPredator: return "Lobo";
        case FaunaType::SkyPredator: return "Falcon";
        case FaunaType::SmallBird: return "Passarinho";
        default: return "?";
    }
}

const char* faunaGoal(FaunaState s, FaunaType t) {
    switch (t) {
        case FaunaType::Medusa:
            if (s == FaunaState::Forage) return "absorver plancton";
            if (s == FaunaState::Migrate) return "ir aonde ha plancton";
            return "flutuar e crescer";
        case FaunaType::Fish:
            if (s == FaunaState::Forage) return "comer plancton";
            if (s == FaunaState::Flee) return "fugir do piscador";
            if (s == FaunaState::Migrate) return "ir aonde ha comida";
            return "nadar";
        case FaunaType::Amphibian:
            if (s == FaunaState::Forage) return "comer na margem";
            if (s == FaunaState::Drink) return "manter a pele umida";
            if (s == FaunaState::Migrate) return "conquistar a terra";
            return "viver entre dois mundos";
        case FaunaType::Herbivore:
            if (s == FaunaState::Forage) return "forragear";
            if (s == FaunaState::Flee) return "fugir dos caçadores";
            if (s == FaunaState::Migrate) return "ir aonde ha plantas";
            return "vagar";
        case FaunaType::Scavenger:
            if (s == FaunaState::Forage) return "açucar carcaças";
            if (s == FaunaState::Flee) return "fugir do falcao";
            return "esperar a morte alheia";
        case FaunaType::SeaPredator:
            if (s == FaunaState::Forage) return "caçar no mar";
            if (s == FaunaState::Rest) return "saciedade";
            return "patrulhar";
        case FaunaType::LandPredator:
            if (s == FaunaState::Forage) return "caçar na terra";
            if (s == FaunaState::Rest) return "saciedade";
            return "esperar presas";
        case FaunaType::SkyPredator:
            if (s == FaunaState::Forage) return "caçar do céu";
            if (s == FaunaState::Rest) return "pousado";
            return "patrulhar";
        case FaunaType::SmallBird:
            if (s == FaunaState::Forage) return "comer sementes";
            if (s == FaunaState::Flee) return "fugir em bando";
            if (s == FaunaState::Rest) return "pousado no galho";
            return "voar em bando";
        default: return "existir";
    }
}

FaunaType Ecosystem::hunterOf(FaunaType prey) {
    switch (prey) {
        case FaunaType::Medusa: return FaunaType::Fish;
        case FaunaType::Fish: return FaunaType::SeaPredator;
        case FaunaType::Amphibian: return FaunaType::SeaPredator;
        case FaunaType::Herbivore: return FaunaType::LandPredator;
        case FaunaType::Scavenger: return FaunaType::SkyPredator;
        default: return FaunaType::LandPredator;
    }
}

const char* eraName(Era e) {
    switch (e) {
        case Era::Silence: return "Silencio Mineral";
        case Era::Microbial: return "Era Microbiana";
        case Era::Ocean: return "Era dos Oceanos";
        case Era::Land: return "Era da Terra Verde";
        case Era::Hunt: return "Era da Caca";
        case Era::Mind: return "Era da Mente";
        case Era::Culture: return "Era da Cultura";
        default: return "?";
    }
}

const char* eraStory(Era e) {
    switch (e) {
        case Era::Silence: return "So rocha, agua e silencio.";
        case Era::Microbial: return "O microbioma pulsa na agua rica.";
        case Era::Ocean: return "Plancton e peixes abrem os oceanos.";
        case Era::Land: return "Plantas e herbivoros conquistam a terra.";
        case Era::Hunt: return "Piscador, Lobo e Falcon caçam cada um seu nicho.";
        case Era::Mind: return "Sob pressao, um animal se ergue.";
        case Era::Culture: return "Ferramentas e fogo nascem.";
        default: return "";
    }
}

std::string LifeLadder::nextHint() const {
    if (!microbe) return "Proxima: microbioma (agua + minerais)";
    if (!plankton) return "Proxima: plancton (luz + nutrientes)";
    if (!plants) return "Proxima: plantas (terra umida)";
    if (!medusa) return "Proxima: Medusa (mar com plancton)";
    if (!fish) return "Proxima: Peixe (medusas + plancton)";
    if (!amphibian) return "Proxima: Anfibio (peixe + margem)";
    if (!herbivores) return "Proxima: Herbivoro (anfibio + plantas)";
    if (!seaPred) return "Proxima: Piscador (peixes)";
    if (!landPred) return "Proxima: Lobo (herbivoros)";
    if (!skyPred) return "Proxima: Falcon (carroneiros)";
    if (!scavengers) return "Proxima: Carroneiro (mortes)";
    if (!sages) return "Proxima: pensantes (pressao + tempo)";
    return "Toda a arvore da vida esta viva.";
}

static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

Ecosystem::Ecosystem(World& world, CreatureSim& sages) : world_(world), sages_(sages) {}

void Ecosystem::loreAdd(const std::string& s) {
    lore_.push_back(s);
    if (lore_.size() > 40) lore_.erase(lore_.begin());
}

Fauna* Ecosystem::get(int id) {
    for (auto& f : fauna_) if (f.id == id) return &f;
    return nullptr;
}

int Ecosystem::countType(FaunaType t) const {
    int n = 0;
    for (const auto& f : fauna_) if (f.type == t && f.alive()) n++;
    return n;
}

LifeLadder Ecosystem::ladder() const {
    LifeLadder L;
    L.planktonTiles = world_.countPlanktonRich(1) / (float)(world_.width() * world_.height()) * 40.f;
    L.planktonTiles = clampf(L.planktonTiles, 0.f, 1.f);
    L.plantDensity = world_.plantDensity(world_.width() / 2, world_.height() / 2, world_.width() / 2);
    L.medusaCount = countType(FaunaType::Medusa);
    L.fishCount = countType(FaunaType::Fish);
    L.amphibCount = countType(FaunaType::Amphibian);
    L.herbCount = countType(FaunaType::Herbivore);
    L.scavCount = countType(FaunaType::Scavenger);
    L.seaPredCount = countType(FaunaType::SeaPredator);
    L.landPredCount = countType(FaunaType::LandPredator);
    L.skyPredCount = countType(FaunaType::SkyPredator);
    L.sageCount = sages_.aliveCount();

    bool anyBio = false, anyPlan = false;
    for (int y = 0; y < world_.height(); y += 4) {
        for (int x = 0; x < world_.width(); x += 4) {
            const TileData& t = world_.at(x, y);
            if (t.biofilm > 0) anyBio = true;
            if (t.plankton > 0) anyPlan = true;
        }
    }
    L.microbe = anyBio;
    L.plankton = anyPlan;
    L.plants = L.plantDensity > 0.005f;
    L.medusa = L.medusaCount > 0 || pioneerDone_[(int)FaunaType::Medusa];
    L.fish = L.fishCount > 0 || pioneerDone_[(int)FaunaType::Fish];
    L.amphibian = L.amphibCount > 0 || pioneerDone_[(int)FaunaType::Amphibian];
    L.herbivores = L.herbCount > 0 || pioneerDone_[(int)FaunaType::Herbivore];
    L.seaPred = L.seaPredCount > 0 || pioneerDone_[(int)FaunaType::SeaPredator];
    L.landPred = L.landPredCount > 0 || pioneerDone_[(int)FaunaType::LandPredator];
    L.skyPred = L.skyPredCount > 0 || pioneerDone_[(int)FaunaType::SkyPredator];
    L.scavengers = L.scavCount > 0 || pioneerDone_[(int)FaunaType::Scavenger];
    L.sages = L.sageCount > 0;
    return L;
}

std::string Ecosystem::goalOf(const Fauna& f) {
    return faunaGoal(f.state, f.type);
}

std::string Ecosystem::nicheOf(const Fauna& f) {
    // descreve o nicho baseado no genoma
    std::string diet, hab, act;
    float p = f.g.dietPlankton, pl = f.g.dietPlants, m = f.g.dietMeat;
    if (p > 0.6f && p > pl && p > m) diet = "planctonívoro";
    else if (pl > 0.6f && pl > p && pl > m) diet = "herbívoro";
    else if (m > 0.6f && m > p && m > pl) diet = "carnívoro";
    else if (p > 0.3f && pl > 0.3f) diet = "onívoro";
    else diet = "omnívoro fraco";

    if (f.g.habitatWet > 0.7f) hab = "aquático";
    else if (f.g.habitatDry > 0.7f) hab = "terrestre";
    else hab = "anfíbio";

    if (f.g.nocturnal > 0.6f) act = "noturno";
    else if (f.g.nocturnal < 0.3f) act = "diurno";
    else act = "crepuscular";

    return diet + " · " + hab + " · " + act;
}

// pousa um animal terrestre na terra mais próxima (nunca na água)
static bool snapToLand(World& w, float& x, float& y) {
    int tx = (int)x, ty = (int)y;
    if (w.inBounds(tx, ty)) {
        Tile t = w.at(tx, ty).type;
        if (t != Tile::Water && t != Tile::DeepWater && t != Tile::Ice) return true;
    }
    for (int r = 1; r <= 8; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int nx = tx + dx, ny = ty + dy;
                if (!w.inBounds(nx, ny)) continue;
                Tile t = w.at(nx, ny).type;
                if (t == Tile::Grass || t == Tile::Forest || t == Tile::Soil || t == Tile::Sand) {
                    x = nx + 0.5f;
                    y = ny + 0.5f;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool isLandTile(World& w, float x, float y) {
    int tx = (int)x, ty = (int)y;
    if (!w.inBounds(tx, ty)) return false;
    Tile t = w.at(tx, ty).type;
    return t != Tile::Water && t != Tile::DeepWater;
}

void Ecosystem::spawnFauna(FaunaType t, float x, float y, int parentA, int parentB) {
    // animais terrestres nunca nascem na água
    bool terrestrial = (t == FaunaType::Herbivore || t == FaunaType::Scavenger ||
                        t == FaunaType::LandPredator || t == FaunaType::SkyPredator ||
                        t == FaunaType::Amphibian);
    if (terrestrial) snapToLand(world_, x, y);

    Fauna f;
    f.id = nextId_++;
    f.type = t;
    f.x = x;
    f.y = y;
    f.energy = t == FaunaType::Fish ? 55.f : 60.f;
    f.age = 0.f;
    f.male = chance(0.5f);
    f.state = FaunaState::Idle;
    f.parentA = parentA;
    f.parentB = parentB;
    f.g.speed = clampf(gauss(1.f, 0.12f), 0.3f, 2.f);
    f.g.size = clampf(gauss(
        f.isPredator() ? 1.3f :
        (t == FaunaType::Medusa ? 0.5f :
         (t == FaunaType::Amphibian ? 0.8f : 1.f)), 0.3f), 0.2f, 2.4f);
    f.g.vision = clampf(gauss(f.isPredator() ? 7.f : 5.f, 1.f), 2.f, 12.f);
    f.g.metabolism = clampf(gauss(0.7f + f.g.size * 0.25f, 0.08f), 0.3f, 1.7f);
    f.g.aggression = clampf(gauss(f.isPredator() ? 0.65f : 0.25f, 0.15f), 0.05f, 0.95f);
    f.g.lifespan = clampf(gauss(
        f.isPredator() ? 7.f :
        (t == FaunaType::Medusa ? 5.f :
         (t == FaunaType::Fish ? 6.f :
          (t == FaunaType::Amphibian ? 6.f :
           (t == FaunaType::Scavenger ? 5.f :
            (t == FaunaType::SmallBird ? 3.f : 6.f))))), 1.5f), 2.f, 13.f);
    // nicho inicial
    switch (t) {
        case FaunaType::Fish:
            f.g.dietPlankton = 0.9f; f.g.habitatWet = 1.f; f.g.nocturnal = 0.2f;
            break;
        case FaunaType::Medusa:
            f.g.dietPlankton = 0.95f; f.g.habitatWet = 1.f; f.g.nocturnal = 0.4f;
            break;
        case FaunaType::Amphibian:
            f.g.dietPlankton = 0.5f; f.g.dietPlants = 0.4f; f.g.habitatWet = 0.7f;
            break;
        case FaunaType::Herbivore:
            f.g.dietPlants = 0.9f; f.g.habitatDry = 0.7f; f.g.nocturnal = 0.2f;
            break;
        case FaunaType::Scavenger:
            f.g.dietMeat = 0.9f; f.g.habitatDry = 0.6f; f.g.nocturnal = 0.5f;
            break;
        case FaunaType::SeaPredator:
            f.g.dietMeat = 0.9f; f.g.habitatWet = 0.9f; f.g.nocturnal = 0.6f;
            break;
        case FaunaType::LandPredator:
            f.g.dietMeat = 0.9f; f.g.habitatDry = 0.8f; f.g.nocturnal = 0.7f;
            break;
        case FaunaType::SkyPredator:
            f.g.dietMeat = 0.8f; f.g.habitatDry = 0.5f; f.g.nocturnal = 0.2f;
            break;
        case FaunaType::SmallBird:
            f.g.dietPlants = 0.8f; f.g.dietPlankton = 0.2f; f.g.habitatDry = 0.6f; f.g.nocturnal = 0.1f;
            f.g.size = 0.3f; // bem pequeno
            f.g.speed = 1.3f; // rápido
            break;
        default: break;
    }
    fauna_.push_back(f);
    emergences_++;
}

bool Ecosystem::spawnPioneer(FaunaType t, float x, float y) {
    if (pioneerDone_[(int)t]) return false;
    pioneerDone_[(int)t] = true;
    spawnFauna(t, x, y);
    return true;
}

void Ecosystem::dropCorpse(float x, float y, float size) {
    int tx = (int)x, ty = (int)y;
    if (!world_.inBounds(tx, ty)) return;
    TileData& t = world_.at(tx, ty);
    t.corpse = (uint8_t)clampf(t.corpse + 4.f + size * 3.f, 0.f, 15.f);
    t.bones = (uint8_t)clampf(t.bones + 2.f + size * 1.5f, 0.f, 15.f);
}

void Ecosystem::dropCorpseFrom(const Fauna& f, float remaining) {
    dropCorpse(f.x, f.y, f.g.size * remaining);
}

void Ecosystem::killInRadius(float cx, float cy, float radius) {
    float r2 = radius * radius;
    for (auto& f : fauna_) {
        if (!f.alive()) continue;
        if (dist2(f.x, f.y, cx, cy) <= r2) {
            f.energy = 0.f;
            dropCorpseFrom(f);
        }
    }
}

void Ecosystem::learnFear(Fauna& prey, FaunaType hunter) {
    prey.fearOf[(int)hunter] = clampf(prey.fearOf[(int)hunter] + 0.35f, 0.f, 1.f);
    prey.fear = clampf(prey.fear + 0.4f, 0.f, 1.f);
}

bool Ecosystem::nearestPredatorThreat(const Fauna& prey, float& px, float& py, FaunaType& ptype) const {
    float best = 1e9f;
    bool found = false;
    float r = (prey.g.vision + 3.f) * (prey.g.vision + 3.f);
    for (const auto& p : fauna_) {
        if (!p.alive() || !p.isPredator()) continue;
        if (p.trustSages > 0.45f && prey.type == FaunaType::Herbivore) continue;
        bool hunts = false;
        if (p.type == FaunaType::SeaPredator &&
            (prey.type == FaunaType::Fish || prey.type == FaunaType::Medusa || prey.type == FaunaType::Amphibian)) hunts = true;
        if (p.type == FaunaType::LandPredator &&
            (prey.type == FaunaType::Herbivore || prey.type == FaunaType::Scavenger || prey.type == FaunaType::Amphibian)) hunts = true;
        if (p.type == FaunaType::SkyPredator &&
            (prey.type == FaunaType::Scavenger || (prey.type == FaunaType::Herbivore && prey.g.size < 0.8f))) hunts = true;
        if (!hunts) continue;
        float d = dist2(prey.x, prey.y, p.x, p.y);
        if (d < r && d < best) {
            best = d;
            px = p.x;
            py = p.y;
            ptype = p.type;
            found = true;
        }
    }
    return found;
}

bool Ecosystem::nearestOfTypes(Fauna& f, const FaunaType* types, int nTypes,
                               float& px, float& py, int& pid) const {
    float best = 1e9f;
    bool found = false;
    float r = (f.g.vision + 4.f) * (f.g.vision + 4.f);
    for (const auto& o : fauna_) {
        if (o.id == f.id || !o.alive()) continue;
        bool match = false;
        for (int i = 0; i < nTypes; i++) if (o.type == types[i]) { match = true; break; }
        if (!match) continue;
        // medo aprendido reduz vontade de caçar quem me feriu
        if (f.fearOf[(int)o.type] > 0.5f && f.g.aggression < 0.6f) continue;
        float d = dist2(f.x, f.y, o.x, o.y);
        if (d < r && d < best) {
            best = d;
            px = o.x;
            py = o.y;
            pid = o.id;
            found = true;
        }
    }
    return found;
}

void Ecosystem::moveToward(Fauna& f, float tx, float ty, float dt, float mul, bool waterOnly) {
    float dx = tx - f.x, dy = ty - f.y;
    float d = std::sqrt(dx * dx + dy * dy);
    if (d < 0.05f) return;
    dx /= d; dy /= d;
    float sp = f.g.speed * 2.f * mul;
    if (f.canFly()) sp *= 1.4f;

    float nx = f.x + dx * sp * dt;
    float ny = f.y + dy * sp * dt;

    auto walkable = [&](float x, float y) {
        if (!world_.inBounds((int)x, (int)y)) return false;
        Tile t = world_.at((int)x, (int)y).type;
        if (waterOnly) return t == Tile::Water || t == Tile::DeepWater;
        return t != Tile::DeepWater && t != Tile::Water;
    };

    if (f.canFly()) {
        // falcão voa: só evita sair do mapa
        if (world_.inBounds((int)nx, (int)ny)) { f.x = nx; f.y = ny; }
        return;
    }

    if (waterOnly) {
        auto isW = [&](float x, float y) {
            if (!world_.inBounds((int)x, (int)y)) return false;
            Tile t = world_.at((int)x, (int)y).type;
            return t == Tile::Water || t == Tile::DeepWater;
        };
        if (isW(nx, f.y)) f.x = nx;
        if (isW(f.x, ny)) f.y = ny;
    } else {
        if (walkable(nx, f.y)) f.x = nx;
        if (walkable(f.x, ny)) f.y = ny;
    }
}

void Ecosystem::wanderIn(Fauna& f, float dt, bool waterOnly) {
    f.stateT -= dt;
    if (f.stateT <= 0.f) {
        f.stateT = frand(1.5f, 4.f);
        f.targetX = clampf(f.x + gauss(0.f, 5.f), 1.f, (float)world_.width() - 2.f);
        f.targetY = clampf(f.y + gauss(0.f, 5.f), 1.f, (float)world_.height() - 2.f);
        if (waterOnly) {
            for (int i = 0; i < 8; i++) {
                int tx = (int)f.targetX, ty = (int)f.targetY;
                if (world_.inBounds(tx, ty) &&
                    (world_.at(tx, ty).type == Tile::Water || world_.at(tx, ty).type == Tile::DeepWater))
                    break;
                f.targetX = clampf(f.x + gauss(0.f, 8.f), 1.f, (float)world_.width() - 2.f);
                f.targetY = clampf(f.y + gauss(0.f, 8.f), 1.f, (float)world_.height() - 2.f);
            }
        }
    }
    moveToward(f, f.targetX, f.targetY, dt, 0.5f, waterOnly);
}

Fauna Ecosystem::makeChild(const Fauna& a, const Fauna& b) {
    Fauna c;
    c.id = nextId_++;
    c.type = a.type;
    c.x = (a.x + b.x) * 0.5f + frand(-0.3f, 0.3f);
    c.y = (a.y + b.y) * 0.5f + frand(-0.3f, 0.3f);
    // filhote terrestre nasce na terra, mesmo que os pais estejam na água
    bool terrestrial = (a.type == FaunaType::Herbivore || a.type == FaunaType::Scavenger ||
                        a.type == FaunaType::LandPredator || a.type == FaunaType::SkyPredator ||
                        a.type == FaunaType::Amphibian);
    if (terrestrial) snapToLand(world_, c.x, c.y);
    c.energy = 45.f;
    c.age = 0.f;
    c.male = chance(0.5f);
    c.parentA = a.id;
    c.parentB = b.id;

    // ---- MUTAÇÃO DIRIGIDA PELO AMBIENTE (seleção natural de verdade) ----
    // conta ameaças e recursos no redor dos pais
    int predatorsNear = 0;
    float resourceNear = 0.f;
    for (const auto& o : fauna_) {
        if (!o.alive()) continue;
        if (o.isPredator() && dist2(o.x, o.y, a.x, a.y) < 100.f) predatorsNear++;
    }
    int tx = (int)a.x, ty = (int)a.y;
    for (int y = ty - 8; y <= ty + 8; y += 2) {
        for (int x = tx - 8; x <= tx + 8; x += 2) {
            if (!world_.inBounds(x, y)) continue;
            const TileData& t = world_.at(x, y);
            resourceNear += t.plankton * 0.3f;
            if (t.plant.kind != PlantKind::None) resourceNear += 0.5f;
            resourceNear += t.corpse * 0.1f;
        }
    }
    resourceNear /= 40.f; // ~0..2

    float mutSpeed = 0.08f, mutSize = 0.06f, mutVis = 0.08f, mutMeta = 0.06f, mutLife = 0.6f;
    // pressão de predação: seleção por velocidade e visão
    if (predatorsNear > 0) {
        mutSpeed += predatorsNear * 0.02f;
        mutVis += predatorsNear * 0.015f;
    }
    // escassez: seleção por eficiência (menor metabolismo, mais visão)
    if (resourceNear < 0.5f) {
        mutMeta += 0.04f;
        mutVis += 0.03f;
    }
    // abundância: seleção por porte e reprodução
    if (resourceNear > 1.2f) {
        mutSize += 0.05f;
    }

    c.g.speed = clampf(gauss((a.g.speed + b.g.speed) * 0.5f, mutSpeed), 0.25f, 2.2f);
    c.g.size = clampf(gauss((a.g.size + b.g.size) * 0.5f, mutSize), 0.2f, 2.5f);
    c.g.vision = clampf(gauss((a.g.vision + b.g.vision) * 0.5f, mutVis), 2.f, 13.f);
    c.g.metabolism = clampf(gauss((a.g.metabolism + b.g.metabolism) * 0.5f, mutMeta), 0.3f, 1.9f);
    c.g.lifespan = clampf(gauss((a.g.lifespan + b.g.lifespan) * 0.5f, mutLife), 1.5f, 14.f);
    c.g.aggression = clampf(gauss((a.g.aggression + b.g.aggression) * 0.5f, 0.1f), 0.05f, 0.95f);
    // nicho: herança + mutação (evolução direcional)
    c.g.dietPlankton = clampf(gauss((a.g.dietPlankton + b.g.dietPlankton) * 0.5f, 0.12f), 0.f, 1.f);
    c.g.dietPlants = clampf(gauss((a.g.dietPlants + b.g.dietPlants) * 0.5f, 0.12f), 0.f, 1.f);
    c.g.dietMeat = clampf(gauss((a.g.dietMeat + b.g.dietMeat) * 0.5f, 0.12f), 0.f, 1.f);
    c.g.habitatWet = clampf(gauss((a.g.habitatWet + b.g.habitatWet) * 0.5f, 0.1f), 0.f, 1.f);
    c.g.habitatDry = clampf(gauss((a.g.habitatDry + b.g.habitatDry) * 0.5f, 0.1f), 0.f, 1.f);
    c.g.nocturnal = clampf(gauss((a.g.nocturnal + b.g.nocturnal) * 0.5f, 0.1f), 0.f, 1.f);
    for (int i = 0; i < FAUNA_TYPE_COUNT; i++) {
        c.fearOf[i] = clampf((a.fearOf[i] + b.fearOf[i]) * 0.4f, 0.f, 1.f);
    }
    return c;
}

void Ecosystem::tryReproduce(Fauna& a, std::vector<Fauna>& out) {
        if (a.mateCooldown > 0.f || a.energy < 45.f || a.age < a.g.lifespan * 0.12f) return;
    if (a.satisfaction > 0.5f && a.isPredator() == false) {
        // saciedade social: ainda acasla, ok
    }
    for (auto& b : fauna_) {
        if (b.id == a.id || !b.alive()) continue;
        if (b.type != a.type || b.male == a.male) continue;
        if (b.mateCooldown > 0.f || b.energy < 45.f) continue;
        // distância de acaslar: maior para vida marinha simples
        float maxMateD = a.type == FaunaType::Medusa || a.type == FaunaType::Fish ? 200.f : 4.f;
        if (dist2(a.x, a.y, b.x, b.y) > maxMateD) continue;

        // precisa de alimento no nicho para justificar filhotes
        float dens = 0.f;
        if (a.type == FaunaType::Medusa || a.type == FaunaType::Fish) {
            // média regional — o tile atual pode já ter sido comido
            float sum = 0;
            int n = 0;
            int tx = (int)a.x, ty = (int)a.y;
            for (int y = ty - 6; y <= ty + 6; y++) {
                for (int x = tx - 6; x <= tx + 6; x++) {
                    if (!world_.inBounds(x, y)) continue;
                    sum += world_.at(x, y).plankton;
                    n++;
                }
            }
            dens = n > 0 ? sum / n : 0.f;
            // com energia alta, já provou que há comida por perto
            if (dens < 0.02f && a.energy < 70.f) continue;
        } else if (a.type == FaunaType::Amphibian) {
            if (!world_.nearWater(a.x, a.y, 4.f)) continue;
            dens = world_.plantDensity((int)a.x, (int)a.y, 6) + 0.1f;
        } else if (a.type == FaunaType::SeaPredator) {
            dens = (float)countType(FaunaType::Fish) * 0.2f + countType(FaunaType::Medusa) * 0.1f;
            if (dens < 0.5f) continue;
        } else if (a.type == FaunaType::Herbivore) {
            dens = world_.plantDensity((int)a.x, (int)a.y, 8);
            if (dens < 0.04f) continue;
        } else if (a.type == FaunaType::Scavenger) {
            int corpses = 0;
            for (int y = (int)a.y - 6; y < (int)a.y + 6; y++)
                for (int x = (int)a.x - 6; x < (int)a.x + 6; x++)
                    if (world_.inBounds(x, y) && world_.at(x, y).corpse > 0) corpses++;
            if (corpses < 1 && a.energy < 70.f) continue;
        } else if (a.isPredator()) {
            FaunaType preyT = hunterOf(a.type);
            if (countType(preyT) < 2) continue;
        }

        float p = 0.15f * (0.5f + a.g.aggression * 0.3f);
        int typeCount = countType(a.type);
        // capacidade suave: desacelera, mas nunca para de renovar a geração
        int softCap = 40, hardCap = 80;
        if (a.type == FaunaType::Amphibian) { softCap = 15; hardCap = 30; }
        if (a.type == FaunaType::Herbivore) { softCap = 20; hardCap = 40; }
        if (a.isPredator()) { softCap = 8; hardCap = 16; }
        if (a.type == FaunaType::Scavenger) { softCap = 10; hardCap = 20; }
        if (typeCount > hardCap) continue;
        if (typeCount > softCap) p *= (float)(hardCap - typeCount) / (float)(hardCap - softCap);
        if (typeCount <= 3) p = std::max(p, 0.35f);
        // recurso escasso freia nascimento
        if ((a.type == FaunaType::Medusa || a.type == FaunaType::Fish || a.type == FaunaType::Amphibian) &&
            dens < 0.06f) p *= 0.15f;
        if (p <= 0.005f) continue;
        if (!chance(p)) continue;
        a.energy -= 12.f;
        b.energy -= 12.f;
        a.mateCooldown = 12.f;
        b.mateCooldown = 12.f;
        a.children++;
        b.children++;
        out.push_back(makeChild(a, b));
        return;
    }
}

// ---- comportamentos ----

void Ecosystem::updateMedusa(Fauna& f, float dt) {
    float px, py;
    FaunaType pt;
    if (nearestPredatorThreat(f, px, py, pt) && pt == FaunaType::Fish) {
        learnFear(f, pt);
        f.state = FaunaState::Flee;
        float dx = f.x - px, dy = f.y - py;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) len = 1.f;
        moveToward(f, f.x + dx / len * 2.f, f.y + dy / len * 2.f, dt, 0.6f, true);
        return;
    }

    int tx = (int)f.x, ty = (int)f.y;
    if (!world_.inBounds(tx, ty) ||
        (world_.at(tx, ty).type != Tile::Water && world_.at(tx, ty).type != Tile::DeepWater)) {
        f.energy -= 0.5f * dt;
        // tenta voltar pra água
        float bx, by; bool wf = false;
        int r = 4;
        float best = 1e9f;
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                Tile t = world_.at(x, y).type;
                if (t != Tile::Water && t != Tile::DeepWater) continue;
                float d = dist2(f.x, f.y, x + 0.5f, y + 0.5f);
                if (d < best) { best = d; bx = x + 0.5f; by = y + 0.5f; wf = true; }
            }
        }
        if (wf) moveToward(f, bx, by, dt, 0.9f, false);
        return;
    }

    // come plâncton SEMPRE que houver — prioridade da vida
    TileData& here = world_.at(tx, ty);
    if (here.plankton > 0) {
        f.state = FaunaState::Forage;
        here.plankton--;
        here.nutrients = clampf(here.nutrients + 0.02f, 0.f, 1.f);
        f.energy = clampf(f.energy + 16.f, 0.f, 100.f);
        f.satisfaction = clampf(f.satisfaction + 0.2f, 0.f, 1.f);
        return;
    }
    // limpador oceânico: medusa come carniça se fome
    if (here.corpse > 0 && f.energy < 70.f) {
        f.state = FaunaState::Forage;
        here.corpse--;
        f.energy = clampf(f.energy + 18.f, 0.f, 100.f);
        f.satisfaction = clampf(f.satisfaction + 0.15f, 0.f, 1.f);
        return;
    }
    // caça plâncton
    f.state = FaunaState::Migrate;
    float bx = f.x, by = f.y, best = -1.f;
    int r = (int)(f.g.vision + 4.f);
    for (int y = ty - r; y <= ty + r; y++) {
        for (int x = tx - r; x <= tx + r; x++) {
            if (!world_.inBounds(x, y)) continue;
            const TileData& t = world_.at(x, y);
            if (t.plankton == 0) continue;
            float sc = t.plankton * 4.f - dist2(f.x, f.y, x + 0.5f, y + 0.5f) * 0.04f;
            if (sc > best) { best = sc; bx = x + 0.5f; by = y + 0.5f; }
        }
    }
    if (best > 0.f) moveToward(f, bx, by, dt, 0.85f, true);
    else wanderIn(f, dt * 0.6f, true);

    // acaslar fácil
    if (f.mateCooldown <= 0.f && f.energy > 35.f && f.age > f.g.lifespan * 0.12f) {
        f.state = FaunaState::Mate;
        for (auto& b : fauna_) {
            if (b.id == f.id || !b.alive() || b.type != f.type || b.male == f.male) continue;
            if (b.mateCooldown > 0.f || b.energy < 30.f) continue;
            if (dist2(f.x, f.y, b.x, b.y) < 120.f) {
                moveToward(f, b.x, b.y, dt, 0.75f, true);
                break;
            }
        }
    }
}

void Ecosystem::updateAmphibian(Fauna& f, float dt) {
    auto findNearbyWater = [&](float& wx, float& wy) -> bool {
        float best = 1e9f;
        bool found = false;
        int r = (int)(f.g.vision + 3.f);
        int tx = (int)f.x, ty = (int)f.y;
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                Tile t = world_.at(x, y).type;
                if (t != Tile::Water && t != Tile::DeepWater) continue;
                float d = dist2(f.x, f.y, x + 0.5f, y + 0.5f);
                if (d < best) { best = d; wx = x + 0.5f; wy = y + 0.5f; found = true; }
            }
        }
        return found;
    };

    // anfíbio: vive na margem, precisa de água por perto, come pequenos
    // usa visão como proxy de inteligência: melhor visão = melhor decisão
    float px, py;
    FaunaType pt;
    if (nearestPredatorThreat(f, px, py, pt)) {
        learnFear(f, pt);
        f.state = FaunaState::Flee;
        float dx = f.x - px, dy = f.y - py;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) len = 1.f;
        float fleeBoost = 1.f + f.g.vision * 0.04f;
        if (!world_.nearWater(f.x + dx / len * 3.f, f.y + dy / len * 3.f, 1.5f)) {
            float wx, wy;
            if (findNearbyWater(wx, wy)) moveToward(f, wx, wy, dt, 1.1f * fleeBoost, false);
            else moveToward(f, f.x + dx / len * 4.f, f.y + dy / len * 4.f, dt, fleeBoost, false);
        } else {
            moveToward(f, f.x + dx / len * 4.f, f.y + dy / len * 4.f, dt, 1.1f * fleeBoost, false);
        }
        return;
    }

    // seca mata anfíbio — mas devagar, dá tempo de fugir
    if (!world_.nearWater(f.x, f.y, 6.f)) {
        f.energy -= 0.25f * dt;
        f.state = FaunaState::Migrate;
        float wx, wy;
        if (findNearbyWater(wx, wy)) moveToward(f, wx, wy, dt, 1.f, false);
        return;
    }

    // só busca comida quando realmente precisa — ou de vez em quando explora
    bool hungry = f.energy < 50.f;
    bool curious = f.energy < 85.f && f.stateT <= 0.f && chance(0.08f);
    if (hungry || curious) {
        if (curious && !hungry) f.stateT = frand(3.f, 8.f);
        f.state = FaunaState::Forage;
        int tx = (int)f.x, ty = (int)f.y;
        // come o que estiver no tile
        if (world_.inBounds(tx, ty)) {
            TileData& t = world_.at(tx, ty);
            if (t.plankton > 0) {
                t.plankton--;
                f.energy = clampf(f.energy + 22.f, 0.f, 100.f);
                f.satisfaction = clampf(f.satisfaction + 0.2f, 0.f, 1.f);
                return;
            }
            if (t.corpse > 0) {
                t.corpse--;
                f.energy = clampf(f.energy + 26.f, 0.f, 100.f);
                return;
            }
            if (t.plant.kind != PlantKind::None && t.plant.stage > 0) {
                t.plant.stage--;
                if (chance(0.35f)) t.plant = Plant{};
                f.energy = clampf(f.energy + 16.f, 0.f, 100.f);
                f.satisfaction = clampf(f.satisfaction + 0.12f, 0.f, 1.f);
                return;
            }
        }
        // procura comida: prefere o que combina com o nicho
        float bx = f.x, by = f.y, best = -1.f;
        int r = (int)(f.g.vision + 3.f);
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                const TileData& t = world_.at(x, y);
                float sc = -1.f;
                if (t.plankton > 0) sc = t.plankton * 3.f * f.g.dietPlankton;
                else if (t.corpse > 0) sc = 2.5f * f.g.dietMeat;
                else if (t.plant.kind != PlantKind::None && t.plant.stage > 0)
                    sc = 1.2f * f.g.dietPlants * t.plant.gene.yield;
                if (sc < 0.f) continue;
                sc -= dist2(f.x, f.y, x + 0.5f, y + 0.5f) * 0.03f;
                if (sc > best) { best = sc; bx = x + 0.5f; by = y + 0.5f; }
            }
        }
        if (best > 0.f) {
            // pode entrar na água para comer (anfíbio)
            bool targetWater = false;
            int ttx = (int)bx, tty = (int)by;
            if (world_.inBounds(ttx, tty)) {
                Tile tt = world_.at(ttx, tty).type;
                targetWater = (tt == Tile::Water || tt == Tile::DeepWater);
            }
            moveToward(f, bx, by, dt, 0.95f, false);
            if (targetWater) {
                f.x = bx; f.y = by;
            }
            return;
        }
        if (hungry) wanderIn(f, dt, false);
        return;
    }

    // saciedade: descansa e aproveita
    if (f.satisfaction > 0.3f || f.energy > 80.f) {
        f.state = FaunaState::Rest;
        f.satisfaction = clampf(f.satisfaction - 0.04f * dt, 0.f, 1.f);
        f.energy += 0.4f * dt;
        wanderIn(f, dt * 0.3f, false);
        return;
    }

    if (f.mateCooldown <= 0.f && f.energy > 55.f && f.age > f.g.lifespan * 0.2f) {
        f.state = FaunaState::Mate;
        for (auto& b : fauna_) {
            if (b.id == f.id || !b.alive() || b.type != f.type || b.male == f.male) continue;
            if (b.mateCooldown > 0.f || b.energy < 50.f) continue;
            if (dist2(f.x, f.y, b.x, b.y) < 40.f) {
                moveToward(f, b.x, b.y, dt, 0.8f, false);
                return;
            }
        }
    }

    f.state = f.satisfaction > 0.3f ? FaunaState::Rest : FaunaState::Idle;
    if (f.satisfaction > 0.f) f.satisfaction = clampf(f.satisfaction - 0.06f * dt, 0.f, 1.f);
    wanderIn(f, dt, false);
}

void Ecosystem::schoolFish(Fauna& f, float dt) {
    // CARDUME: peixes juntos, mesma direção
    float cx = 0, cy = 0;
    float avgVx = 0, avgVy = 0;
    int n = 0;
    for (const auto& o : fauna_) {
        if (o.id == f.id || !o.alive() || o.type != FaunaType::Fish) continue;
        float d = dist2(f.x, f.y, o.x, o.y);
        if (d > 36.f) continue;
        cx += o.x;
        cy += o.y;
        avgVx += o.targetX - o.x;
        avgVy += o.targetY - o.y;
        n++;
    }

    // AFASTAR DA BORDA: força pro centro do oceano
    float margin = 8.f;
    float pushX = 0, pushY = 0;
    if (f.x < margin) pushX = (margin - f.x) * 0.5f;
    if (f.x > world_.width() - margin) pushX = -(f.x - (world_.width() - margin)) * 0.5f;
    if (f.y < margin) pushY = (margin - f.y) * 0.5f;
    if (f.y > world_.height() - margin) pushY = -(f.y - (world_.height() - margin)) * 0.5f;
    if (pushX != 0.f || pushY != 0.f) {
        moveToward(f, f.x + pushX, f.y + pushY, dt, 0.8f, true);
        return;
    }

    if (n == 0) {
        // sozinho: vaga devagar na água
        if (chance(0.3f * dt)) wanderIn(f, dt * 0.5f, true);
        return;
    }
    cx /= n;
    cy /= n;

    float dxC = cx - f.x, dyC = cy - f.y;
    float dC = std::sqrt(dxC * dxC + dyC * dyC);
    if (dC > 2.5f) {
        moveToward(f, cx, cy, dt, 0.6f, true);
    } else {
        avgVx /= n;
        avgVy /= n;
        float aLen = std::sqrt(avgVx * avgVx + avgVy * avgVy);
        if (aLen > 0.1f) {
            f.targetX = f.x + avgVx / aLen * 4.f;
            f.targetY = f.y + avgVy / aLen * 4.f;
            moveToward(f, f.targetX, f.targetY, dt, 0.55f, true);
        } else {
            // parados: vaga devagar junto
            if (chance(0.2f * dt)) wanderIn(f, dt * 0.4f, true);
        }
    }
    // separação
    for (const auto& o : fauna_) {
        if (o.id == f.id || !o.alive() || o.type != FaunaType::Fish) continue;
        float d = dist2(f.x, f.y, o.x, o.y);
        if (d < 1.2f && d > 0.01f) {
            float dx = f.x - o.x, dy = f.y - o.y;
            float len = std::sqrt(d);
            moveToward(f, f.x + dx / len * 1.5f, f.y + dy / len * 1.5f, dt, 0.8f, true);
        }
    }
}

void Ecosystem::updateFish(Fauna& f, float dt) {
    // mede do piscador
    float px, py;
    FaunaType pt;
    if (nearestPredatorThreat(f, px, py, pt) && pt == FaunaType::SeaPredator) {
        learnFear(f, pt);
        f.state = FaunaState::Flee;
        float dx = f.x - px, dy = f.y - py;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) len = 1.f;
        moveToward(f, f.x + dx / len * 5.f, f.y + dy / len * 5.f, dt, 1.2f, true);
        return;
    }
    f.fear = clampf(f.fear - 0.1f * dt, 0.f, 1.f);

    int tx = (int)f.x, ty = (int)f.y;
    if (!world_.inBounds(tx, ty)) { f.energy = 0; return; }
    TileData& here = world_.at(tx, ty);
    if (here.type != Tile::Water && here.type != Tile::DeepWater) {
        f.energy -= 2.f * dt;
        wanderIn(f, dt, true);
        return;
    }

    // come sempre que houver plâncton — a vida se alimenta primeiro
    if (here.plankton > 0 && f.energy < 90.f) {
        f.state = FaunaState::Forage;
        here.plankton--;
        here.nutrients = clampf(here.nutrients + 0.02f, 0.f, 1.f);
        f.energy = clampf(f.energy + 22.f, 0.f, 100.f);
        f.satisfaction = clampf(f.satisfaction + 0.22f, 0.f, 1.f);
        return;
    }
    // limpador oceânico: peixe come carniça se fome
    if (here.corpse > 0 && f.energy < 65.f) {
        f.state = FaunaState::Forage;
        here.corpse--;
        f.energy = clampf(f.energy + 20.f, 0.f, 100.f);
        f.satisfaction = clampf(f.satisfaction + 0.15f, 0.f, 1.f);
        return;
    }
    // caça plâncton
    f.state = FaunaState::Forage;
    float bx = f.x, by = f.y, best = -1.f;
    int r = (int)(f.g.vision + 2.f);
    for (int y = ty - r; y <= ty + r; y++) {
        for (int x = tx - r; x <= tx + r; x++) {
            if (!world_.inBounds(x, y)) continue;
            const TileData& t = world_.at(x, y);
            if (t.plankton == 0) continue;
            float sc = t.plankton * 4.f - dist2(f.x, f.y, x + 0.5f, y + 0.5f) * 0.05f;
            if (sc > best) { best = sc; bx = x + 0.5f; by = y + 0.5f; }
        }
    }
    if (best > 0.f) {
        moveToward(f, bx, by, dt, 0.95f, true);
        if (dist2(f.x, f.y, bx, by) < 0.5f) {
            TileData& t = world_.at((int)bx, (int)by);
            if (t.plankton > 0) {
                t.plankton--;
                f.energy = clampf(f.energy + 22.f, 0.f, 100.f);
                f.satisfaction = clampf(f.satisfaction + 0.22f, 0.f, 1.f);
            }
        }
        schoolFish(f, dt); // vai junto com o cardume
        return;
    }
    // sem comida: cardume vaga junto
    schoolFish(f, dt);
    if (chance(0.3f * dt)) wanderIn(f, dt, true);

    if (f.mateCooldown <= 0.f && f.energy > 50.f && f.age > f.g.lifespan * 0.15f) {
        f.state = FaunaState::Mate;
        for (auto& b : fauna_) {
            if (b.id == f.id || !b.alive() || b.type != f.type || b.male == f.male) continue;
            if (b.mateCooldown > 0.f || b.energy < 40.f) continue;
            if (dist2(f.x, f.y, b.x, b.y) < 100.f) {
                moveToward(f, b.x, b.y, dt, 0.9f, true);
                // perto o bastante = acasla
                if (dist2(f.x, f.y, b.x, b.y) < 4.f) {
                    // força acaslar se ambos prontos
                    f.energy = clampf(f.energy, 40.f, 100.f);
                }
                break;
            }
        }
        return;
    }

    f.state = f.satisfaction > 0.4f ? FaunaState::Rest : FaunaState::Idle;
    if (f.satisfaction > 0.4f) f.satisfaction = clampf(f.satisfaction - 0.08f * dt, 0.f, 1.f);
    wanderIn(f, dt, true);
}

void Ecosystem::updateHerbivore(Fauna& f, float dt) {
    float px, py;
    FaunaType pt;
    if (nearestPredatorThreat(f, px, py, pt)) {
        learnFear(f, pt);
        f.state = FaunaState::Flee;
        float dx = f.x - px, dy = f.y - py;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) len = 1.f;
        moveToward(f, f.x + dx / len * 4.5f, f.y + dy / len * 4.5f, dt, 1.15f, false);
        f.energy -= 0.3f * dt;
        return;
    }
    f.fear = clampf(f.fear - 0.12f * dt, 0.f, 1.f);

    if (f.energy < 20.f) {
        f.state = FaunaState::Flee;
        float bx, by; bool found = false;
        int r = (int)(f.g.vision + 2.f);
        int tx = (int)f.x, ty = (int)f.y;
        float best = 1e9f;
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                if (world_.at(x, y).corpse == 0) continue;
                float d = dist2(f.x, f.y, x + 0.5f, y + 0.5f);
                if (d < best) { best = d; bx = x + 0.5f; by = y + 0.5f; found = true; }
            }
        }
        if (found) {
            moveToward(f, bx, by, dt, 1.f, false);
            if (dist2(f.x, f.y, bx, by) < 0.5f) {
                TileData& t = world_.at((int)bx, (int)by);
                if (t.corpse > 0) { t.corpse--; f.energy = clampf(f.energy + 14.f, 0.f, 100.f); }
            }
            return;
        }
        wanderIn(f, dt * 0.4f, false);
        return;
    }

    if (f.energy < 50.f || (f.energy < 75.f && f.stateT <= 0.f && chance(0.06f))) {
        if (f.energy < 75.f && f.stateT <= 0.f) f.stateT = frand(4.f, 10.f);
        f.state = FaunaState::Forage;
        int tx = (int)f.x, ty = (int)f.y;
        if (world_.inBounds(tx, ty)) {
            Plant& p = world_.at(tx, ty).plant;
            if (p.kind != PlantKind::None && p.stage > 0) {
                float nutr = (p.kind == PlantKind::Fruit ? 16.f : 9.f) * p.gene.yield;
                if (p.kind == PlantKind::Toxic && chance(0.25f * (1.f - p.gene.cure * 0.5f))) {
                    f.energy -= 6.f;
                } else {
                    f.energy = clampf(f.energy + nutr, 0.f, 100.f);
                    f.satisfaction = clampf(f.satisfaction + 0.15f, 0.f, 1.f);
                }
                if (p.stage > 0) p.stage--;
                if (chance(0.5f)) p.kind = PlantKind::None;
                return;
            }
        }
        float bx = f.x, by = f.y, best = -1.f;
        int r = (int)f.g.vision;
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                const Plant& p = world_.at(x, y).plant;
                if (p.kind == PlantKind::None || p.stage == 0) continue;
                float sc = p.stage * p.gene.yield - dist2(f.x, f.y, x + 0.5f, y + 0.5f) * 0.04f;
                if (sc > best) { best = sc; bx = x + 0.5f; by = y + 0.5f; }
            }
        }
        if (best > 0.f) moveToward(f, bx, by, dt, 0.85f, false);
        else wanderIn(f, dt, false);
        return;
    }

    if (f.mateCooldown <= 0.f && f.energy > 65.f && f.age > f.g.lifespan * 0.15f) {
        f.state = FaunaState::Mate;
        for (auto& b : fauna_) {
            if (b.id == f.id || !b.alive() || b.type != f.type || b.male == f.male) continue;
            if (b.mateCooldown > 0.f || b.energy < 55.f) continue;
            if (dist2(f.x, f.y, b.x, b.y) < 64.f) {
                moveToward(f, b.x, b.y, dt, 0.8f, false);
                return;
            }
        }
    }

    f.state = f.satisfaction > 0.4f ? FaunaState::Rest : FaunaState::Idle;
    if (f.satisfaction > 0.f) f.satisfaction = clampf(f.satisfaction - 0.06f * dt, 0.f, 1.f);
    wanderIn(f, dt, false);
}

void Ecosystem::updateScavenger(Fauna& f, float dt) {
    float px, py;
    FaunaType pt;
    if (nearestPredatorThreat(f, px, py, pt)) {
        learnFear(f, pt);
        f.state = FaunaState::Flee;
        f.fear = clampf(f.fear + 0.3f * dt, 0.f, 1.f);
        float dx = f.x - px, dy = f.y - py;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) len = 1.f;
        moveToward(f, f.x + dx / len * 5.f, f.y + dy / len * 5.f, dt, 1.2f, false);
        return;
    }
    f.fear = clampf(f.fear - 0.15f * dt, 0.f, 1.f);

    if (f.energy < 70.f || (f.stateT <= 0.f && chance(0.07f))) {
        if (f.energy >= 70.f) f.stateT = frand(5.f, 12.f);
        f.state = FaunaState::Forage;
        float bx = f.x, by = f.y, best = 1e9f;
        bool found = false;
        int r = (int)(f.g.vision + 4.f);
        int tx = (int)f.x, ty = (int)f.y;
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                if (world_.at(x, y).corpse == 0) continue;
                float d = dist2(f.x, f.y, x + 0.5f, y + 0.5f);
                if (d < best) { best = d; bx = x + 0.5f; by = y + 0.5f; found = true; }
            }
        }
        if (found) {
            moveToward(f, bx, by, dt, 0.9f, false);
            if (dist2(f.x, f.y, bx, by) < 0.5f) {
                TileData& t = world_.at((int)bx, (int)by);
                if (t.corpse > 0) {
                    t.corpse--;
                    f.energy = clampf(f.energy + 20.f, 0.f, 100.f);
                    f.satisfaction = clampf(f.satisfaction + 0.25f, 0.f, 1.f);
                }
            }
            return;
        }
    }

    if (f.mateCooldown <= 0.f && f.energy > 55.f && f.age > f.g.lifespan * 0.2f) {
        f.state = FaunaState::Mate;
        for (auto& b : fauna_) {
            if (b.id == f.id || !b.alive() || b.type != f.type || b.male == f.male) continue;
            if (b.mateCooldown > 0.f || b.energy < 50.f) continue;
            if (dist2(f.x, f.y, b.x, b.y) < 64.f) {
                moveToward(f, b.x, b.y, dt, 0.75f, false);
                return;
            }
        }
    }

    f.state = f.satisfaction > 0.3f ? FaunaState::Rest : FaunaState::Idle;
    if (f.satisfaction > 0.f) f.satisfaction = clampf(f.satisfaction - 0.07f * dt, 0.f, 1.f);
    wanderIn(f, dt, false);
}

// caça genérico — mede agressividade, medo aprendido e saciedade
void Ecosystem::huntPrey(Fauna& f, float dt, const FaunaType* preyTypes, int nPrey, float speedMul) {
    float hx, hy;
    int hid = -1;
    bool anyFear = false;
    for (int i = 0; i < nPrey; i++) if (f.fearOf[(int)preyTypes[i]] > 0.6f) anyFear = true;

    // cooldown de caça: 2 dias entre caçadas
    if (f.stateT > 0.f) {
        f.stateT -= dt;
        // descansa enquanto espera
        f.state = FaunaState::Rest;
        f.energy += 0.4f * dt;
        if (chance(0.3f * dt)) wanderIn(f, dt * 0.25f, f.type == FaunaType::SeaPredator);
        return;
    }

    // só caça se tem fome de verdade
    if (f.energy < 50.f && !anyFear &&
        nearestOfTypes(f, preyTypes, nPrey, hx, hy, hid)) {
        f.state = FaunaState::Forage;
        moveToward(f, hx, hy, dt, speedMul + f.g.aggression * 0.3f, f.canFly() || f.type == FaunaType::SeaPredator);
        if (dist2(f.x, f.y, hx, hy) < (f.canFly() ? 1.2f : 0.55f)) {
            for (auto& prey : fauna_) {
                if (prey.id != hid || !prey.alive()) continue;
                float killChance = 0.45f + f.g.aggression * 0.4f - prey.g.speed * 0.1f;
                if (chance(clampf(killChance, 0.15f, 0.9f))) {
                    prey.energy = 0.f;
                    dropCorpse(prey.x, prey.y, prey.g.size * 0.3f);
                    f.energy = clampf(f.energy + 35.f, 0.f, 100.f);
                    f.satisfaction = 1.f;
                    f.stateT = 360.f; // 2 dias de descanso
                } else {
                    learnFear(prey, f.type);
                    if (chance(0.3f)) {
                        f.energy -= 5.f + prey.g.size * 4.f;
                        f.anger = clampf(f.anger + 0.3f, 0.f, 1.f);
                        if (prey.g.size > f.g.size * 1.2f) {
                            f.fearOf[(int)prey.type] = clampf(f.fearOf[(int)prey.type] + 0.35f, 0.f, 1.f);
                        }
                    }
                    return;
                }
                break;
            }
        }
        return;
    }

    // saciedade: dorme e digere
    if (f.satisfaction > 0.4f) {
        f.state = FaunaState::Rest;
        f.satisfaction = clampf(f.satisfaction - 0.02f * dt, 0.f, 1.f);
        f.energy += 0.5f * dt;
        if (chance(0.4f * dt)) wanderIn(f, dt * 0.2f, f.type == FaunaType::SeaPredator);
        return;
    }

    if (f.mateCooldown <= 0.f && f.energy > 65.f && f.age > f.g.lifespan * 0.2f && f.satisfaction > 0.3f) {
        f.state = FaunaState::Mate;
        for (auto& b : fauna_) {
            if (b.id == f.id || !b.alive() || b.type != f.type || b.male == f.male) continue;
            if (b.mateCooldown > 0.f || b.energy < 55.f) continue;
            if (dist2(f.x, f.y, b.x, b.y) < 80.f) {
                moveToward(f, b.x, b.y, dt, 0.75f, f.canFly() || f.type == FaunaType::SeaPredator);
                return;
            }
        }
    }

    f.state = FaunaState::Idle;
    f.satisfaction = clampf(f.satisfaction - 0.03f * dt, 0.f, 1.f);
    wanderIn(f, dt, f.type == FaunaType::SeaPredator);
}

void Ecosystem::updateSeaPredator(Fauna& f, float dt) {
    static const FaunaType prey[] = {FaunaType::Fish, FaunaType::Medusa, FaunaType::Amphibian};
    huntPrey(f, dt, prey, 3, 1.0f);
}

void Ecosystem::updateLandPredator(Fauna& f, float dt) {
    static const FaunaType prey[] = {FaunaType::Herbivore, FaunaType::Scavenger, FaunaType::Amphibian};
    huntPrey(f, dt, prey, 3, 1.0f);
}

void Ecosystem::updateSkyPredator(Fauna& f, float dt) {
    // FALCÃO: voa alto, caça mergulhando, pousa em árvores
    float hx, hy;
    int hid = -1;
    bool anyFear = false;
    for (int i = 0; i < 2; i++) {
        static const FaunaType preyT[] = {FaunaType::Scavenger, FaunaType::Herbivore};
        if (f.fearOf[(int)preyT[i]] > 0.6f) anyFear = true;
    }

    // pousado: descansa em terra
    if (f.state == FaunaState::Rest && world_.walkable(f.x, f.y)) {
        f.energy += 0.5f * dt;
        f.satisfaction = clampf(f.satisfaction - 0.05f * dt, 0.f, 1.f);
        if (f.satisfaction < 0.2f) f.state = FaunaState::Idle;
        return;
    }

    // caça: procura presa do alto e mergulha
    if ((f.energy < 55.f || (f.energy < 80.f && f.satisfaction < 0.3f)) && !anyFear) {
        static const FaunaType preyT[] = {FaunaType::Scavenger, FaunaType::Herbivore};
        if (nearestOfTypes(f, preyT, 2, hx, hy, hid)) {
            f.state = FaunaState::Forage;
            // voa até sobre a presa
            float d = dist2(f.x, f.y, hx, hy);
            if (d > 4.f) {
                moveToward(f, hx, hy, dt, 1.3f, true); // voa rápido
            } else {
                // MERGULHO: ataque rápido
                for (auto& prey : fauna_) {
                    if (prey.id != hid || !prey.alive()) continue;
                    float killChance = 0.55f + f.g.aggression * 0.3f;
                    if (chance(clampf(killChance, 0.2f, 0.9f))) {
                        prey.energy = 0.f;
                        dropCorpse(prey.x, prey.y, prey.g.size * 0.2f);
                        f.energy = clampf(f.energy + 30.f, 0.f, 100.f);
                        f.satisfaction = 1.f;
                        // procura árvore pra pousar e comer
                        f.state = FaunaState::Rest;
                        f.stateT = 60.f;
                    } else {
                        learnFear(prey, f.type);
                    }
                    break;
                }
            }
            return;
        }
    }

    // pousa em árvore/rocha depois de comer
    if (f.satisfaction > 0.5f && world_.walkable(f.x, f.y)) {
        f.state = FaunaState::Rest;
        return;
    }

    // voa procurando lugar bom
    f.state = FaunaState::Idle;
    // prefere ficar perto de floresta (árvore pra pousar)
    if (chance(0.3f * dt)) {
        // procura floresta e voa até lá
        int r = 8;
        int tx = (int)f.x, ty = (int)f.y;
        float best = 1e9f;
        float bx = f.x, by = f.y;
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                if (world_.at(x, y).type != Tile::Forest) continue;
                float d = dist2(f.x, f.y, x + 0.5f, y + 0.5f);
                if (d < best) { best = d; bx = x + 0.5f; by = y + 0.5f; }
            }
        }
        if (best < 100.f) moveToward(f, bx, by, dt, 0.8f, true);
        else wanderIn(f, dt * 0.5f, true);
    }
}

void Ecosystem::schoolBirds(Fauna& f, float dt) {
    // BANDO DE PASSARINHOS: juntos, mesmo medo
    float cx = 0, cy = 0;
    int n = 0;
    for (const auto& o : fauna_) {
        if (o.id == f.id || !o.alive() || o.type != FaunaType::SmallBird) continue;
        if (dist2(f.x, f.y, o.x, o.y) > 25.f) continue;
        cx += o.x;
        cy += o.y;
        n++;
    }
    // afasta da borda
    float margin = 5.f;
    float pushX = 0, pushY = 0;
    if (f.x < margin) pushX = (margin - f.x) * 0.4f;
    if (f.x > world_.width() - margin) pushX = -(f.x - (world_.width() - margin)) * 0.4f;
    if (f.y < margin) pushY = (margin - f.y) * 0.4f;
    if (f.y > world_.height() - margin) pushY = -(f.y - (world_.height() - margin)) * 0.4f;
    if (pushX != 0.f || pushY != 0.f) {
        moveToward(f, f.x + pushX, f.y + pushY, dt, 0.7f, true);
        return;
    }

    if (n == 0) {
        if (chance(0.3f * dt)) wanderIn(f, dt * 0.5f, true);
        return;
    }
    cx /= n;
    cy /= n;
    float dxC = cx - f.x, dyC = cy - f.y;
    float dC = std::sqrt(dxC * dxC + dyC * dyC);
    if (dC > 3.f) {
        moveToward(f, cx, cy, dt, 0.6f, true);
    } else if (chance(0.2f * dt)) {
        wanderIn(f, dt * 0.4f, true);
    }
}

void Ecosystem::updateSmallBird(Fauna& f, float dt) {
    // PASSARINHO: voa em bando, come sementes, foge de tudo
    float px, py;
    FaunaType pt;
    if (nearestPredatorThreat(f, px, py, pt)) {
        learnFear(f, pt);
        f.state = FaunaState::Flee;
        float dx = f.x - px, dy = f.y - py;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) len = 1.f;
        moveToward(f, f.x + dx / len * 6.f, f.y + dy / len * 6.f, dt, 1.3f, true);
        schoolBirds(f, dt); // foge em bando
        return;
    }
    f.fear = clampf(f.fear - 0.15f * dt, 0.f, 1.f);

    // come sementes (planta madura) ou plâncton
    if (f.energy < 70.f) {
        f.state = FaunaState::Forage;
        int tx = (int)f.x, ty = (int)f.y;
        // procura planta madura
        float bx = f.x, by = f.y, best = -1.f;
        int r = (int)(f.g.vision + 2.f);
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                const TileData& t = world_.at(x, y);
                float sc = -1.f;
                if (t.plant.kind != PlantKind::None && t.plant.stage >= 1)
                    sc = t.plant.stage * t.plant.gene.yield;
                else if (t.plankton > 0) sc = t.plankton * 0.5f;
                if (sc < 0.f) continue;
                sc -= dist2(f.x, f.y, x + 0.5f, y + 0.5f) * 0.04f;
                if (sc > best) { best = sc; bx = x + 0.5f; by = y + 0.5f; }
            }
        }
        if (best > 0.f) {
            moveToward(f, bx, by, dt, 1.f, true);
            if (dist2(f.x, f.y, bx, by) < 0.5f) {
                TileData& t = world_.at((int)bx, (int)by);
                if (t.plant.kind != PlantKind::None && t.plant.stage >= 1) {
                    t.plant.stage--;
                    if (chance(0.3f)) t.plant = Plant{};
                    f.energy = clampf(f.energy + 12.f, 0.f, 100.f);
                    f.satisfaction = clampf(f.satisfaction + 0.15f, 0.f, 1.f);
                } else if (t.plankton > 0) {
                    t.plankton--;
                    f.energy = clampf(f.energy + 8.f, 0.f, 100.f);
                }
            }
            schoolBirds(f, dt);
            return;
        }
    }

    // bando voa junto
    schoolBirds(f, dt);

    if (f.mateCooldown <= 0.f && f.energy > 55.f && f.age > f.g.lifespan * 0.2f) {
        f.state = FaunaState::Mate;
        for (auto& b : fauna_) {
            if (b.id == f.id || !b.alive() || b.type != f.type || b.male == f.male) continue;
            if (b.mateCooldown > 0.f || b.energy < 50.f) continue;
            if (dist2(f.x, f.y, b.x, b.y) < 30.f) {
                moveToward(f, b.x, b.y, dt, 0.8f, true);
                return;
            }
        }
    }

    f.state = f.satisfaction > 0.3f ? FaunaState::Rest : FaunaState::Idle;
    if (f.satisfaction > 0.f) f.satisfaction = clampf(f.satisfaction - 0.05f * dt, 0.f, 1.f);
}

void Ecosystem::updateEra(unsigned day) {
    LifeLadder L = ladder();
    Era next = era_;
    if (L.sages) {
        CreatureSim::Stats st = sages_.stats();
        if (st.avgTool > 0.35f && st.avgFire > 0.15f) next = Era::Culture;
        else next = Era::Mind;
    } else if (L.landPred || L.seaPred || L.skyPred) next = Era::Hunt;
    else if (L.herbivores) next = Era::Land;
    else if (L.fish || L.plankton) next = Era::Ocean;
    else if (L.microbe) next = Era::Microbial;
    else next = Era::Silence;

    if ((int)next > (int)era_) {
        era_ = next;
        loreAdd("=== " + std::string(eraName(era_)) + " — dia " + std::to_string(day) + " ===");
        loreAdd(eraStory(era_));
    }
}

void Ecosystem::tryEmergence(unsigned day) {
    // ---- ÁRVORE EVOLUTIVA: cada espécie nasce da anterior quando o ambiente permite ----
    // Nada "aparece do nada": um indivíduo da espécie ancestral MUTA para a nova.
    // Depois, a nova população só cresce por NASCIMENTO.

    int planTiles = 0;
    for (int y = 0; y < world_.height(); y += 3)
        for (int x = 0; x < world_.width(); x += 3)
            if (world_.at(x, y).plankton > 0) planTiles++;

    // medusa ← plancton (par fundador)
    if (!pioneerDone_[(int)FaunaType::Medusa] && planTiles > 5) {
        int found = 0;
        float mx[2] = {0, 0}, my[2] = {0, 0};
        for (int i = 0; i < 40 && found < 2; i++) {
            int x = irand(1, world_.width() - 2);
            int y = irand(1, world_.height() - 2);
            if (world_.at(x, y).type == Tile::Water && world_.at(x, y).plankton >= 1) {
                bool dup = false;
                for (int j = 0; j < found; j++)
                    if ((int)mx[j] == x && (int)my[j] == y) dup = true;
                if (dup) continue;
                mx[found] = x + 0.5f;
                my[found] = y + 0.5f;
                found++;
            }
        }
        if (found == 2) {
            spawnPioneer(FaunaType::Medusa, mx[0], my[0]);
            spawnFauna(FaunaType::Medusa, mx[1], my[1]);
            // garante casal
            auto& all = fauna_;
            all[all.size() - 2].male = true;
            all.back().male = false;
            all[all.size() - 2].energy = 80.f;
            all.back().energy = 80.f;
            loreAdd("Dia " + std::to_string(day) + ": no mar, duas formas simples respiraram. As Medusas — primeiro casal da vida.");
        }
    }

    // peixe ← medusas existem + plancton
    if (!pioneerDone_[(int)FaunaType::Fish] && countType(FaunaType::Medusa) >= 2 && planTiles > 8) {
        // DOIS ancestrais mutam juntos: já nascem como casal
        std::vector<Fauna*> ancs;
        for (auto& f : fauna_) if (f.alive() && f.type == FaunaType::Medusa) ancs.push_back(&f);
        if (ancs.size() >= 2) {
            for (int i = 0; i < 2 && i < (int)ancs.size(); i++) {
                ancs[i]->type = FaunaType::Fish;
                ancs[i]->g.speed = clampf(ancs[i]->g.speed + 0.4f, 0.5f, 2.f);
                ancs[i]->g.size = clampf(ancs[i]->g.size + 0.3f, 0.3f, 2.f);
                ancs[i]->g.vision = clampf(ancs[i]->g.vision + 1.5f, 3.f, 12.f);
                ancs[i]->energy = 90.f;
                ancs[i]->age = clampf(ancs[i]->age, 1.f, 3.f);
                ancs[i]->mateCooldown = 0.f;
                ancs[i]->g.metabolism = clampf(ancs[i]->g.metabolism * 0.7f, 0.3f, 1.f);
            }
            ancs[0]->male = true;
            ancs[1]->male = false;
            // joga um banco de plâncton em volta do casal (recursos para prosperar)
            int fx = (int)ancs[0]->x, fy = (int)ancs[0]->y;
            for (int y = fy - 4; y <= fy + 4; y++)
                for (int x = fx - 4; x <= fx + 4; x++)
                    if (world_.inBounds(x, y) && world_.at(x, y).type == Tile::Water)
                        world_.at(x, y).plankton = (uint8_t)std::min(3, (int)world_.at(x, y).plankton + 2);
            pioneerDone_[(int)FaunaType::Fish] = true;
            // família fundadora: casal + filhotes
            for (int kid = 0; kid < 2; kid++) {
                spawnFauna(FaunaType::Fish, ancs[0]->x + frand(-0.5f, 0.5f),
                           ancs[0]->y + frand(-0.5f, 0.5f), ancs[0]->id, ancs[1]->id);
                auto& baby = fauna_.back();
                baby.energy = 70.f;
                baby.g.metabolism = clampf(ancs[0]->g.metabolism * 0.8f, 0.3f, 1.f);
            }
            loreAdd("Dia " + std::to_string(day) + ": duas medusas nadaram forte demais. Os primeiros Peixes — e ja ha filhotes.");
        }
    }

    // anfíbio ← peixe + margem
    if (!pioneerDone_[(int)FaunaType::Amphibian] && countType(FaunaType::Fish) >= 6) {
        // varredura completa por margem
        std::vector<std::pair<int,int>> shores;
        for (int y = 3; y < world_.height() - 3; y += 2) {
            for (int x = 3; x < world_.width() - 3; x += 2) {
                Tile t = world_.at(x, y).type;
                if (t != Tile::Grass && t != Tile::Soil && t != Tile::Sand) continue;
                if (world_.nearWater((float)x, (float)y, 4.f)) shores.push_back({x, y});
            }
        }
        if (!shores.empty() && countType(FaunaType::Fish) >= 2) {
            auto [x, y] = shores[irand(0, (int)shores.size() - 1)];
            std::vector<Fauna*> ancs;
            for (auto& f : fauna_) if (f.alive() && f.type == FaunaType::Fish) ancs.push_back(&f);
            if (ancs.size() >= 2) {
                ancs[0]->type = FaunaType::Amphibian;
                ancs[0]->x = x + 0.5f;
                ancs[0]->y = y + 0.5f;
                ancs[0]->male = true;
                ancs[0]->energy = 90.f;
                ancs[0]->mateCooldown = 0.f;
                ancs[1]->type = FaunaType::Amphibian;
                ancs[1]->x = x + 0.7f;
                ancs[1]->y = y + 0.7f;
                ancs[1]->male = false;
                ancs[1]->energy = 90.f;
                ancs[1]->mateCooldown = 0.f;
                for (int kid = 0; kid < 3; kid++) {
                    spawnFauna(FaunaType::Amphibian, x + 0.5f + frand(-0.4f, 0.4f),
                               y + 0.5f + frand(-0.4f, 0.4f), ancs[0]->id, ancs[1]->id);
                    fauna_.back().energy = 75.f;
                }
                pioneerDone_[(int)FaunaType::Amphibian] = true;
                loreAdd("Dia " + std::to_string(day) + ": dois peixes arrastaram-se na margem. Os Anfibios — mar e terra.");
            }
        }
    }

    // herbívoro ← anfíbio + plantas
    if (!pioneerDone_[(int)FaunaType::Herbivore] && countType(FaunaType::Amphibian) >= 2 &&
        world_.plantDensity(world_.width() / 2, world_.height() / 2, world_.width() / 2) > 0.02f) {
        std::vector<Fauna*> ancs;
        for (auto& f : fauna_) if (f.alive() && f.type == FaunaType::Amphibian) ancs.push_back(&f);
        if (ancs.size() >= 2) {
            ancs[0]->type = FaunaType::Herbivore;
            ancs[0]->g.size = clampf(ancs[0]->g.size + 0.2f, 0.5f, 2.2f);
            ancs[0]->male = true;
            ancs[0]->energy = clampf(ancs[0]->energy + 15.f, 0.f, 100.f);
            ancs[0]->mateCooldown = 0.f;
            ancs[1]->type = FaunaType::Herbivore;
            ancs[1]->g.size = clampf(ancs[1]->g.size + 0.2f, 0.5f, 2.2f);
            ancs[1]->male = false;
            ancs[1]->energy = clampf(ancs[1]->energy + 15.f, 0.f, 100.f);
            ancs[1]->mateCooldown = 0.f;
            pioneerDone_[(int)FaunaType::Herbivore] = true;
            loreAdd("Dia " + std::to_string(day) + ": dois anfibios mastigaram grama. Os Herbivoros.");
        }
    }

    // Piscador ← peixes abundantes (só um predador, caça sozinho)
    if (!pioneerDone_[(int)FaunaType::SeaPredator] && countType(FaunaType::Fish) >= 4) {
        for (auto& f : fauna_) {
            if (f.alive() && f.type == FaunaType::Fish) {
                f.type = FaunaType::SeaPredator;
                f.g.size = clampf(f.g.size + 0.5f, 0.8f, 2.4f);
                f.g.aggression = clampf(f.g.aggression + 0.3f, 0.3f, 0.95f);
                f.energy = clampf(f.energy + 25.f, 0.f, 100.f);
                pioneerDone_[(int)FaunaType::SeaPredator] = true;
                loreAdd("Dia " + std::to_string(day) + ": um peixe ficou grande demais. O Piscador caça os outros.");
                break;
            }
        }
    }

    // Lobo ← herbívoros
    if (!pioneerDone_[(int)FaunaType::LandPredator] && countType(FaunaType::Herbivore) >= 3) {
        for (auto& f : fauna_) {
            if (f.alive() && f.type == FaunaType::Herbivore) {
                f.type = FaunaType::LandPredator;
                f.g.size = clampf(f.g.size + 0.4f, 0.8f, 2.4f);
                f.g.aggression = clampf(f.g.aggression + 0.35f, 0.4f, 0.95f);
                f.energy = clampf(f.energy + 25.f, 0.f, 100.f);
                pioneerDone_[(int)FaunaType::LandPredator] = true;
                loreAdd("Dia " + std::to_string(day) + ": um herbivoro olhou o outro com fome. O Lobo.");
                break;
            }
        }
    }

    // Carroneiro ← mortes
    if (!pioneerDone_[(int)FaunaType::Scavenger]) {
        int corpses = 0;
        for (int y = 0; y < world_.height(); y += 3)
            for (int x = 0; x < world_.width(); x += 3)
                if (world_.at(x, y).corpse > 2) corpses++;
        if (corpses >= 2) {
            for (auto& f : fauna_) {
                if (f.alive() && (f.type == FaunaType::Herbivore || f.type == FaunaType::Amphibian)) {
                    f.type = FaunaType::Scavenger;
                    f.energy = clampf(f.energy + 20.f, 0.f, 100.f);
                    pioneerDone_[(int)FaunaType::Scavenger] = true;
                    loreAdd("Dia " + std::to_string(day) + ": onde ha morte, alguem se aproveita. O Carroneiro.");
                    break;
                }
            }
        }
    }

    // Falcão ← carroceiros
    if (!pioneerDone_[(int)FaunaType::SkyPredator] && countType(FaunaType::Scavenger) >= 1) {
        for (auto& f : fauna_) {
            if (f.alive() && f.type == FaunaType::Scavenger) {
                f.type = FaunaType::SkyPredator;
                f.g.size = clampf(f.g.size * 0.7f, 0.3f, 1.2f);
                f.g.vision = clampf(f.g.vision + 3.f, 5.f, 13.f);
                f.energy = clampf(f.energy + 25.f, 0.f, 100.f);
                pioneerDone_[(int)FaunaType::SkyPredator] = true;
                loreAdd("Dia " + std::to_string(day) + ": alguem aprendeu a voar. O Falcon.");
                break;
            }
        }
    }

    // Passarinho ← plantas abundantes (ave pequena, come sementes)
    if (!pioneerDone_[(int)FaunaType::SmallBird] &&
        world_.plantDensity(world_.width() / 2, world_.height() / 2, world_.width() / 2) > 0.03f) {
        // nasce de um anfíbio ou herbívoro que "voa" (mutação rara)
        for (auto& f : fauna_) {
            if (f.alive() && (f.type == FaunaType::Amphibian || f.type == FaunaType::Herbivore)) {
                f.type = FaunaType::SmallBird;
                f.g.size = 0.3f;
                f.g.speed = 1.3f;
                f.energy = clampf(f.energy + 20.f, 0.f, 100.f);
                pioneerDone_[(int)FaunaType::SmallBird] = true;
                loreAdd("Dia " + std::to_string(day) + ": um bicho pequeno aprendeu a voar. Passarinhos!");
                break;
            }
        }
    }

    trySageBirth(day);
}

void Ecosystem::trySageBirth(unsigned day) {
    int sages = sages_.aliveCount();
    int herb = countType(FaunaType::Herbivore);
    int pred = countType(FaunaType::LandPredator) + countType(FaunaType::SeaPredator) +
               countType(FaunaType::SkyPredator);
    if (sages > 0 && sages > 1 + herb / 3) return;
    if (herb < 6 || pred < 1) return;
    if (sageCooldown_ > 0.f) return;

    float avgLife = 0.f;
    int n = 0;
    for (const auto& f : fauna_) {
        if (f.type != FaunaType::Herbivore || !f.alive()) continue;
        avgLife += f.g.lifespan;
        n++;
    }
    if (n > 0) avgLife /= n;
    if (avgLife < 5.5f) return;

    std::vector<Fauna*> candidates;
    for (auto& f : fauna_) {
        if (f.type == FaunaType::Herbivore && f.alive() && f.age > 1.f && f.energy > 40.f)
            candidates.push_back(&f);
    }
    if (candidates.empty()) return;
    Fauna* evo = candidates[irand(0, (int)candidates.size() - 1)];

    sages_.evolveFromAnimal(evo->x, evo->y, evo->g.speed, evo->g.size, evo->g.vision,
                            evo->g.metabolism, evo->g.lifespan, evo->parentA, evo->parentB);
    evo->energy = 0.f;
    sageCooldown_ = 50.f;
    loreAdd("Dia " + std::to_string(day) + ": sob pressao, " +
            std::string(faunaName(evo->type)) + " se levantou — nasceu o pensante.");
}

void Ecosystem::update(float dt, unsigned day) {
    if (sageCooldown_ > 0.f) sageCooldown_ -= dt;
    bool night = world_.isNight();
    std::vector<Fauna> newborns;
    for (auto& f : fauna_) {
        if (!f.alive()) continue;
        f.age += dt / SECONDS_PER_DAY;
        // metabolismo da vida: bem mais baixo — a vida deve se sustentar
        float drain = 0.18f * f.g.metabolism;
        if (f.satisfaction > 0.3f) drain *= (1.f - f.satisfaction * 0.35f);
        if (f.state == FaunaState::Forage || f.state == FaunaState::Flee) drain += 0.06f * f.g.speed;
        // nicho: habitat errado = gasta mais; habitat certo = gasta menos
        if (world_.inBounds((int)f.x, (int)f.y)) {
            Tile t = world_.at((int)f.x, (int)f.y).type;
            bool wet = (t == Tile::Water || t == Tile::DeepWater);
            if (wet && f.g.habitatWet > 0.6f) drain *= 0.85f; // em casa
            if (!wet && f.g.habitatDry > 0.6f) drain *= 0.85f;
            if (wet && f.g.habitatDry > 0.7f) drain *= 1.15f; // fora de casa
            if (!wet && f.g.habitatWet > 0.7f) drain *= 1.15f;
        }
        // competição suave
        int pop = countType(f.type);
        if (pop > 15) drain *= (1.f + (pop - 15) * 0.03f);
        if (pop <= 3) drain *= 0.6f;
        if (night) {
            if (f.type == FaunaType::Fish || f.type == FaunaType::Medusa) drain *= 0.5f;
            else drain *= 0.75f;
        }
        f.energy -= drain * dt;
        if (f.mateCooldown > 0.f) f.mateCooldown -= dt;
        if (f.age > f.g.lifespan) f.energy = 0.f;
        if (f.energy <= 0.f) continue;

        // sono noturno: só mamíferos e aves dormem; peixes e insetos não
        bool sleepsAtNight =
            f.type == FaunaType::Herbivore ||   // mamífero
            f.type == FaunaType::Scavenger ||    // mamífero (mas pode ser noturno — ver abaixo)
            f.type == FaunaType::LandPredator || // lobo: CAÇADOR noturno, não dorme
            f.type == FaunaType::SkyPredator;    // ave: dorme

        // lobo caça de noite — não dorme
        bool nocturnalHunter =
            f.type == FaunaType::LandPredator ||
            f.type == FaunaType::SeaPredator;

        // carroNEIRO pode ser noturno (aproveita carniça escondida)
        bool scavengerNocturnal = f.type == FaunaType::Scavenger && night && chance(0.5f);

        if (night && !nocturnalHunter && !scavengerNocturnal &&
            sleepsAtNight && f.state != FaunaState::Flee) {
            f.state = FaunaState::Rest;
            f.energy += 1.3f * dt;
            if (chance(0.4f * dt)) {
                // mamífero dorme no chão
                if (f.type == FaunaType::SkyPredator) {
                    // falcão pousa em terra firme
                    int tx = (int)f.x, ty = (int)f.y;
                    if (!world_.walkable(f.x, f.y)) {
                        float lx = f.x, ly = f.y;
                        bool found = false;
                        float best = 1e9f;
                        for (int y = ty - 6; y <= ty + 6; y++) {
                            for (int x = tx - 6; x <= tx + 6; x++) {
                                if (!world_.inBounds(x, y)) continue;
                                if (!world_.walkable(x + 0.5f, y + 0.5f)) continue;
                                float d = dist2(f.x, f.y, x + 0.5f, y + 0.5f);
                                if (d < best) { best = d; lx = x + 0.5f; ly = y + 0.5f; found = true; }
                            }
                        }
                        if (found) { f.x = lx; f.y = ly; }
                    }
                }
            }
            continue; // dorme, não faz mais nada
        }
        // peixes, medusas, anfíbios: ativos de noite (mais lentos)
        // lobo e piscador: caçam de noite
        // carroNEIRO noturno: caça carniça de noite

        switch (f.type) {
            case FaunaType::Medusa:
                if (night) { wanderIn(f, dt * 0.35f, true); f.state = FaunaState::Idle; }
                else updateMedusa(f, dt);
                break;
            case FaunaType::Fish:
                if (night) { schoolFish(f, dt * 0.5f); f.state = FaunaState::Idle; }
                else updateFish(f, dt);
                break;
            case FaunaType::Amphibian:
                if (night && f.energy > 30.f) {
                    f.state = FaunaState::Rest;
                    f.energy += 0.8f * dt;
                    wanderIn(f, dt * 0.3f, false);
                } else updateAmphibian(f, dt);
                break;
            case FaunaType::Herbivore:
                // mamífero: dorme de noite (já tratado acima)
                updateHerbivore(f, dt);
                break;
            case FaunaType::Scavenger:
                if (scavengerNocturnal) updateScavenger(f, dt);
                else if (night) {
                    f.state = FaunaState::Rest;
                    f.energy += 1.f * dt;
                    wanderIn(f, dt * 0.2f, false);
                } else updateScavenger(f, dt);
                break;
            case FaunaType::SeaPredator:
                // caçador noturno: mais ativo de noite
                updateSeaPredator(f, dt);
                break;
            case FaunaType::LandPredator:
                // lobo: caçador noturno
                updateLandPredator(f, dt);
                break;
            case FaunaType::SkyPredator:
                updateSkyPredator(f, dt);
                break;
            case FaunaType::SmallBird:
                if (night) {
                    // passarinho dorme em galho
                    f.state = FaunaState::Rest;
                    f.energy += 1.f * dt;
                    if (chance(0.3f * dt)) wanderIn(f, dt * 0.2f, true);
                } else updateSmallBird(f, dt);
                break;
                break;
        }
    }

    for (auto& f : fauna_) {
        if (!f.alive()) continue;
        tryReproduce(f, newborns);
        // rebrota quando a espécie está instável
        if (countType(f.type) <= 3 && f.energy > 70.f && f.age > f.g.lifespan * 0.1f &&
            f.mateCooldown <= 0.f && chance(0.025f)) {
            float nx = f.x + frand(-0.5f, 0.5f);
            float ny = f.y + frand(-0.5f, 0.5f);
            bool terrestrial = (f.type == FaunaType::Herbivore || f.type == FaunaType::Scavenger ||
                                f.type == FaunaType::LandPredator || f.type == FaunaType::SkyPredator ||
                                f.type == FaunaType::Amphibian);
            if (terrestrial) snapToLand(world_, nx, ny);
            spawnFauna(f.type, nx, ny, f.id, -1);
            auto& clone = fauna_.back();
            clone.energy = 55.f;
            clone.g = f.g;
            clone.g.speed = clampf(f.g.speed + gauss(0.f, 0.1f), 0.2f, 2.2f);
            clone.male = chance(0.5f);
            f.energy -= 20.f;
            f.mateCooldown = 15.f;
            loreAdd("Dia " + std::to_string(day) + ": quase no fim, " + std::string(faunaName(f.type)) +
                    " encontrou forcas para recomecar.");
        }
    }
    for (auto& nb : newborns) {
        fauna_.push_back(nb);
        pop_.totalBirths++;
        if (nb.children == 0 && chance(0.3f)) {
            // anuncia nascimento de predador etc.
            if (nb.isPredator()) {
                loreAdd("Dia " + std::to_string(day) + ": " + std::string(faunaName(nb.type)) +
                        " nasceu. A especie continua.");
            }
        }
    }

    for (auto& f : fauna_) {
        if (!f.alive()) dropCorpseFrom(f);
    }
    fauna_.erase(
        std::remove_if(fauna_.begin(), fauna_.end(),
                       [](const Fauna& f) { return !f.alive(); }),
        fauna_.end());

    // stats
    for (int i = 0; i < FAUNA_TYPE_COUNT; i++) pop_.counts[i] = 0;
    float sumSp[FAUNA_TYPE_COUNT] = {}, sumLf[FAUNA_TYPE_COUNT] = {},
          sumAg[FAUNA_TYPE_COUNT] = {}, sumSa[FAUNA_TYPE_COUNT] = {};
    for (const auto& f : fauna_) {
        if (!f.alive()) continue;
        int i = (int)f.type;
        pop_.counts[i]++;
        sumSp[i] += f.g.speed;
        sumLf[i] += f.g.lifespan;
        sumAg[i] += f.g.aggression;
        sumSa[i] += f.satisfaction;
    }
    for (int i = 0; i < FAUNA_TYPE_COUNT; i++) {
        if (pop_.counts[i] > 0) {
            pop_.avgSpeed[i] = sumSp[i] / pop_.counts[i];
            pop_.avgLife[i] = sumLf[i] / pop_.counts[i];
            pop_.avgAggro[i] = sumAg[i] / pop_.counts[i];
            pop_.avgSatis[i] = sumSa[i] / pop_.counts[i];
        }
    }

    updateEra(day);

    emergeTimer_ -= dt;
    if (emergeTimer_ <= 0.f) {
        emergeTimer_ = 10.f;
        tryEmergence(day);
    }
    lastDay_ = day;
}

}
