#include "god.hpp"
#include "rng.hpp"
#include <cmath>
#include <algorithm>

namespace skc {

GodSystem::GodSystem(World& world, CreatureSim& creatures)
    : world_(world), creatures_(creatures) {}

const char* GodSystem::actionName(GodAction a) {
    switch (a) {
        case GodAction::Rain: return "Chuva";
        case GodAction::PetitionFruit: return "Pedido de Fruta";
        case GodAction::Asteroid: return "Asteroide";
        case GodAction::Frost: return "Geada";
        default: return "?";
    }
}

const char* GodSystem::actionKey(GodAction a) {
    switch (a) {
        case GodAction::Rain: return "1";
        case GodAction::PetitionFruit: return "2";
        case GodAction::Asteroid: return "3";
        case GodAction::Frost: return "4";
        default: return "?";
    }
}

float GodSystem::cooldownMax(GodAction a) const {
    switch (a) {
        case GodAction::Rain: return 25.f;
        case GodAction::PetitionFruit: return 35.f;
        case GodAction::Asteroid: return 100.f;
        case GodAction::Frost: return 40.f;
        default: return 30.f;
    }
}

float GodSystem::cooldownLeft(GodAction a) const {
    int i = (int)a;
    if (i < 0 || i >= 4) return 0.f;
    return cd_[i];
}

void GodSystem::update(float dt) {
    for (int i = 0; i < 4; i++) {
        if (cd_[i] > 0.f) cd_[i] = std::max(0.f, cd_[i] - dt);
    }
}

GodResult GodSystem::invoke(GodAction a, int cx, int cy) {
    GodResult r;
    int i = (int)a;
    if (i < 0 || i >= 4) return r;
    if (cd_[i] > 0.f) {
        r.ok = false;
        r.text = "Ainda nao e hora.";
        return r;
    }

    if (!world_.inBounds(cx, cy)) {
        r.ok = false;
        r.text = "La nao existe mundo.";
        return r;
    }

    switch (a) {
        case GodAction::Rain: r = doRain(cx, cy); break;
        case GodAction::PetitionFruit: r = doPetitionFruit(cx, cy); break;
        case GodAction::Asteroid: r = doAsteroid(cx, cy); break;
        case GodAction::Frost: r = doFrost(cx, cy); break;
        default: break;
    }

    if (r.ok) cd_[i] = cooldownMax(a);
    return r;
}

GodResult GodSystem::doRain(int cx, int cy) {
    GodResult r;
    int radius = 18;
    float humidity = world_.globalHumidity();
    float local = world_.averageMoisture(cx, cy, radius);
    float roll = frand();

    float fullChance = 0.35f + humidity * 0.35f + (1.f - local) * 0.25f;
    float partialChance = fullChance + 0.3f;

    if (roll < fullChance) {
        for (int y = cy - radius; y <= cy + radius; y++) {
            for (int x = cx - radius; x <= cx + radius; x++) {
                if (!world_.inBounds(x, y)) continue;
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy > radius * radius) continue;
                TileData& t = world_.at(x, y);
                t.moisture = clampf(t.moisture + 0.55f, 0.f, 1.f);
                // chuva lava minerais da rocha para a água (origem do microbioma)
                if (t.type == Tile::Water || t.type == Tile::DeepWater) {
                    t.nutrients = clampf(t.nutrients + 0.12f, 0.f, 1.f);
                }
            }
        }
        r.ok = true;
        r.text = "Os ceus abriram. A terra bebeu; minerais correram para a agua.";
    } else if (roll < partialChance) {
        for (int y = cy - 8; y <= cy + 8; y++) {
            for (int x = cx - 8; x <= cx + 8; x++) {
                if (!world_.inBounds(x, y)) continue;
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy > 64) continue;
                TileData& t = world_.at(x, y);
                t.moisture = clampf(t.moisture + 0.22f, 0.f, 1.f);
                if (t.type == Tile::Water || t.type == Tile::DeepWater) {
                    t.nutrients = clampf(t.nutrients + 0.06f, 0.f, 1.f);
                }
            }
        }
        r.ok = true;
        r.text = "Uma garoa passagesia molhou um pouco do chao.";
    } else {
        r.ok = false;
        r.text = "O vento levou as nuvens. Nada caiu.";
    }
    return r;
}

GodResult GodSystem::doPetitionFruit(int cx, int cy) {
    GodResult r;
    int radius = 12;
    float localM = world_.averageMoisture(cx, cy, radius);

    float soil = 0.f;
    int soilN = 0;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (!world_.inBounds(x, y)) continue;
            const TileData& t = world_.at(x, y);
            if (t.type == Tile::Grass || t.type == Tile::Forest || t.type == Tile::Soil) {
                soil += t.fertility + t.fert_bonus;
                soilN++;
            }
        }
    }
    float avgFert = soilN > 0 ? soil / soilN : 0.f;
    float accept = clampf(0.25f + localM * 0.45f + avgFert * 0.35f, 0.05f, 0.95f);

    if (!chance(accept)) {
        r.ok = false;
        if (localM < 0.25f) r.text = "O solo estava seco. O pedido morreu na terra.";
        else if (avgFert < 0.25f) r.text = "A terra e pobre. Nao houve resposta.";
        else r.text = "A terra ouviu, mas decidiu nao responder ainda.";
        return r;
    }

    int grown = 0;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (!world_.inBounds(x, y)) continue;
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy > radius * radius) continue;
            TileData& t = world_.at(x, y);
            if (t.plant.kind != PlantKind::None) continue;
            float p = 0.45f * accept * t.moisture * (t.fertility + t.fert_bonus + 0.2f);
            if (chance(p)) {
                PlantKind k = chance(0.22f) ? PlantKind::Toxic : PlantKind::Fruit;
                t.plant.kind = k;
                t.plant.stage = (uint8_t)irand(1, 2);
                t.fert_bonus += 0.15f;
                grown++;
            }
        }
    }

    r.ok = true;
    if (grown > 8) r.text = "A terra aceitou. Brotoes verdes despertaram.";
    else if (grown > 0) r.text = "Alguns brotos aceitaram o pedido.";
    else r.text = "O pedido foi ouvido, mas nada brotou desta vez.";
    return r;
}

GodResult GodSystem::doAsteroid(int cx, int cy) {
    GodResult r;
    int radius = irand(8, 14);

    for (int y = cy - radius - 2; y <= cy + radius + 2; y++) {
        for (int x = cx - radius - 2; x <= cx + radius + 2; x++) {
            if (!world_.inBounds(x, y)) continue;
            int dx = x - cx, dy = y - cy;
            float d = std::sqrt((float)(dx * dx + dy * dy));
            if (d > (float)radius + 1.5f) continue;
            TileData& t = world_.at(x, y);
            t.plant = Plant{};
            if (d < radius * 0.35f) {
                t.type = Tile::Rock;
                t.fert_bonus = 1.2f;
                t.fertility = clampf(t.fertility + 0.15f, 0.f, 1.f);
                t.nutrients = clampf(t.nutrients + 0.5f, 0.f, 1.f);
                // cratera pode virar lago com chuva
                if (chance(0.35f)) { t.type = Tile::Water; t.moisture = 1.f; }
            } else if (d < radius * 0.7f) {
                t.type = chance(0.5f) ? Tile::Ice : Tile::Rock;
                t.moisture = clampf(t.moisture + 0.3f, 0.f, 1.f);
                t.fert_bonus += 0.4f;
                t.nutrients = clampf(t.nutrients + 0.3f, 0.f, 1.f);
                t.biofilm = 0;
                t.plankton = 0;
            } else {
                if (t.type == Tile::Water || t.type == Tile::DeepWater) t.type = Tile::Ice;
                else if (chance(0.4f)) t.type = Tile::Sand;
                t.fert_bonus += 0.15f;
                t.nutrients = clampf(t.nutrients + 0.15f, 0.f, 1.f);
                t.plankton = 0;
            }
        }
    }

    creatures_.killInRadius((float)cx + 0.5f, (float)cy + 0.5f, (float)radius + 1.f);
    if (eco_) eco_->killInRadius((float)cx + 0.5f, (float)cy + 0.5f, (float)radius + 1.f);

    r.ok = true;
    r.text = "O ceu rasgou-se. Gelo e pedra caíram; minerais banharam o cratera.";
    return r;
}

GodResult GodSystem::doFrost(int cx, int cy) {
    GodResult r;
    int radius = 12;
    int froze = 0;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (!world_.inBounds(x, y)) continue;
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy > radius * radius) continue;
            TileData& t = world_.at(x, y);
            if (t.type == Tile::Water) {
                t.type = Tile::Ice;
                t.moisture = 0.12f;
                t.plankton = 0;
                t.biofilm = 0;
                froze++;
            } else if (t.type == Tile::DeepWater && chance(0.3f)) {
                t.type = Tile::Ice;
                t.plankton = 0;
                froze++;
            } else {
                t.moisture = clampf(t.moisture - 0.35f, 0.f, 1.f);
                if (t.plant.kind != PlantKind::None && chance(0.5f)) {
                    if (t.plant.stage > 0) t.plant.stage--;
                    else t.plant = Plant{};
                }
            }
        }
    }
    // geada mata vida sensível ao frio na área
    if (eco_) eco_->killInRadius((float)cx + 0.5f, (float)cy + 0.5f, (float)radius * 0.6f);
    r.ok = true;
    if (froze > 10) r.text = "O frio apertou. A agua virou pedra gelada; o plancton dormiu.";
    else r.text = "A geada assou a terra, mas o gelo foi curto.";
    return r;
}

}
