#include "save.hpp"
#include "mundo/world.hpp"
#include "pensantes/creature.hpp"
#include "animais/fauna.hpp"
#include "deus/god.hpp"
#include "rng.hpp"
#include <fstream>
#include <sstream>
#include <cstdio>

namespace skc {

bool saveGame(World& w, CreatureSim& sim, Ecosystem& eco,
              unsigned day, float timeAcc, float camX, float camY, float zoom,
              const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "SKC_SAVE 6\n";
    f << "meta " << day << " " << timeAcc << " " << camX << " " << camY << " " << zoom << "\n";
    f << "dims " << w.width() << " " << w.height() << "\n";

    for (int y = 0; y < w.height(); y++) {
        for (int x = 0; x < w.width(); x++) {
            const TileData& t = w.at(x, y);
            const PlantGene& pg = t.plant.gene;
            f << (int)t.type << " " << t.moisture << " " << t.fertility << " "
              << t.fert_bonus << " " << t.nutrients << " "
              << (int)t.biofilm << " " << (int)t.plankton << " "
              << (int)t.corpse << " " << (int)t.bones << " "
              << (int)t.plant.kind << " " << (int)t.plant.stage << " "
              << pg.growth << " " << pg.yield << " " << pg.toxicity
              << " " << pg.hardiness << " " << pg.cure
              << " " << pg.animal_spread << " " << pg.expansion << " " << pg.spore_range
              << " " << (int)t.trees << " " << (int)t.wood
              << "\n";
        }
    }

    f << "CREATURES " << (int)sim.all().size() << "\n";
    for (const auto& c : sim.all()) {
        f << c.id << " " << c.name << " " << c.surname << " "
          << c.x << " " << c.y << " " << (int)c.male << " "
          << c.energy << " " << c.hydration << " " << c.age << " "
          << c.g.speed << " " << c.g.metabolism << " " << c.g.size << " "
          << c.g.vision << " " << c.g.fertility << " " << c.g.robustness << " "
          << c.g.lifespan << " " << c.g.sociability << " " << c.g.aggression << " "
          << c.g.intelligence << " "
          << c.k.plant_value << " " << c.k.toxic_aversion << " " << c.k.discrimination << " "
          << c.k.tool_skill << " " << c.k.fire_knowledge << " " << c.k.antidote << " "
          << c.k.predator_knowledge << " "
          << c.em.fear << " " << c.em.anger << " " << c.em.love << " " << c.em.grief << " "
          << c.em.curiosity << " " << c.em.comfort << " " << c.em.loneliness << " "
          << c.em.satisfaction << " " << c.em.guilt << " " << c.em.shame << " " << c.em.pride
          << " " << c.morality << " " << c.reputation << " "
          << c.parentA << " " << c.parentB << " " << c.children << " "
          << c.communityId << " " << c.bornDay << " "
          << c.pers.sexuality << " " << c.pers.boldness << " " << c.pers.thinker << " "
          << c.pers.greed << " " << c.pers.laziness << " " << c.pers.empathy << " "
          << c.pers.wanderlust << " "
          << c.em.happiness << " " << c.em.security << " " << c.em.desireFamily << " "
          << c.pers.attachment << " " << c.pers.optimism << " "
          << c.totalChildrenBorn << " "
          << (int)c.state << "\n";
    }

    f << "FAUNA " << (int)eco.all().size() << "\n";
    for (const auto& a : eco.all()) {
        f << a.id << " " << (int)a.type << " " << (int)a.state << " "
          << a.x << " " << a.y << " "
          << a.energy << " " << a.age << " " << (int)a.male << " "
          << a.g.speed << " " << a.g.size << " " << a.g.vision << " "
          << a.g.metabolism << " " << a.g.lifespan << " " << a.g.aggression << " "
          << a.fear << " " << a.anger << " " << a.satisfaction << " "
          << a.trustSages << " " << a.children << " "
          << a.parentA << " " << a.parentB << " "
          << a.g.swim << " " << a.habitatUse << " " << a.groupId << " "
          << a.g.dietPlankton << " " << a.g.dietPlants << " " << a.g.dietMeat << " "
          << a.g.habitatWet << " " << a.g.habitatDry << " " << a.g.nocturnal << "\n";
    }

    auto& comms = sim.communities();
    f << "COMMUNITIES " << (int)comms.size() << "\n";
    for (const auto& cm : comms) {
        f << cm.id << " " << cm.x << " " << cm.y << " " << cm.territoryR << " "
          << (int)cm.hasFire() << " " << (int)cm.hasHut() << " " << cm.foundingDay << " "
          << cm.name << " " << cm.memberIds.size();
        for (int id : cm.memberIds) f << " " << id;
        f << "\n";
    }

    f.close();
    return true;
}

bool loadGame(World& w, CreatureSim*& sim, Ecosystem*& eco, GodSystem*& god,
              unsigned& day, float& timeAcc, float& camX, float& camY, float& zoom,
              const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string tag;
    f >> tag;
    if (tag != "SKC_SAVE") return false;
    int ver; f >> ver;

    delete god; god = nullptr;
    delete eco; eco = nullptr;
    delete sim; sim = nullptr;

    f >> tag;
    f >> day >> timeAcc >> camX >> camY >> zoom;

    int ww, wh;
    f >> tag >> ww >> wh;
    // assume mundo já tem tamanho adequado — limpa tiles
    if (ww != w.width() || wh != w.height()) {
        // se tamanho diferente, não carrega (mundo fixo)
        return false;
    }

    for (int y = 0; y < wh; y++) {
        for (int x = 0; x < ww; x++) {
            TileData& t = w.at(x, y);
            int type, pkind, pstage;
            f >> type >> t.moisture >> t.fertility >> t.fert_bonus >> t.nutrients;
            int bio, plan, corp, bone;
            f >> bio >> plan >> corp >> bone;
            t.biofilm = (uint8_t)bio;
            t.plankton = (uint8_t)plan;
            t.corpse = (uint8_t)corp;
            t.bones = (uint8_t)bone;
            f >> pkind >> pstage;
            t.plant.kind = (PlantKind)pkind;
            t.plant.stage = (uint8_t)pstage;
            PlantGene& pg = t.plant.gene;
            f >> pg.growth >> pg.yield >> pg.toxicity >> pg.hardiness >> pg.cure
              >> pg.animal_spread >> pg.expansion >> pg.spore_range;
            if (ver >= 6) {
                int trees, wood;
                f >> trees >> wood;
                t.trees = (uint8_t)trees;
                t.wood = (uint8_t)wood;
            } else {
                // save antigo: floresta ganha árvores derivadas do tipo
                t.trees = (t.type == Tile::Forest) ? 3 : 0;
                t.wood = 0;
            }
            t.type = (Tile)type;
        }
    }

    sim = new CreatureSim(w);
    eco = new Ecosystem(w, *sim);
    sim->setEcosystem(eco);
    god = new GodSystem(w, *sim);
    god->setEcosystem(eco);

    int nC;
    f >> tag >> nC;
    for (int i = 0; i < nC; i++) {
        Creature c;
        int male, state;
        f >> c.id >> c.name >> c.surname >> c.x >> c.y >> male;
        c.male = (bool)male;
        f >> c.energy >> c.hydration >> c.age;
        f >> c.g.speed >> c.g.metabolism >> c.g.size >> c.g.vision
          >> c.g.fertility >> c.g.robustness >> c.g.lifespan
          >> c.g.sociability >> c.g.aggression >> c.g.intelligence;
        f >> c.k.plant_value >> c.k.toxic_aversion >> c.k.discrimination
          >> c.k.tool_skill >> c.k.fire_knowledge >> c.k.antidote >> c.k.predator_knowledge;
        f >> c.em.fear >> c.em.anger >> c.em.love >> c.em.grief
          >> c.em.curiosity >> c.em.comfort >> c.em.loneliness
          >> c.em.satisfaction >> c.em.guilt >> c.em.shame >> c.em.pride;
        f >> c.morality >> c.reputation >> c.parentA >> c.parentB >> c.children
          >> c.communityId >> c.bornDay;
        f >> c.pers.sexuality >> c.pers.boldness >> c.pers.thinker
          >> c.pers.greed >> c.pers.laziness >> c.pers.empathy >> c.pers.wanderlust;
        if (ver >= 3) {
            f >> c.em.happiness >> c.em.security >> c.em.desireFamily
              >> c.pers.attachment >> c.pers.optimism >> c.totalChildrenBorn;
        } else {
            c.em.happiness = clampf(0.4f + c.pers.optimism * 0.3f, 0.f, 1.f);
            c.em.security = 0.4f;
        }
        f >> state;
        c.state = (CState)state;
        sim->all().push_back(c);
    }

    int nF;
    f >> tag >> nF;
    for (int i = 0; i < nF; i++) {
        Fauna a;
        int type, state, male;
        f >> a.id >> type >> state >> a.x >> a.y;
        a.type = (FaunaType)type;
        a.state = (FaunaState)state;
        f >> a.energy >> a.age >> male;
        a.male = (bool)male;
        f >> a.g.speed >> a.g.size >> a.g.vision >> a.g.metabolism
          >> a.g.lifespan >> a.g.aggression;
        f >> a.fear >> a.anger >> a.satisfaction >> a.trustSages
          >> a.children >> a.parentA >> a.parentB;
        if (ver >= 2) {
            f >> a.g.swim;
            if (ver >= 4) f >> a.habitatUse;
            else a.habitatUse = a.fullyAquatic() ? 1.f : 0.2f;
            if (ver >= 5) f >> a.groupId;
            f >> a.g.dietPlankton >> a.g.dietPlants >> a.g.dietMeat
              >> a.g.habitatWet >> a.g.habitatDry >> a.g.nocturnal;
        } else {
            // save v1: nado padrão por tipo
            if (a.fullyAquatic() || a.type == FaunaType::Amphibian ||
                a.type == FaunaType::Turtle) a.g.swim = 1.f;
            else if (a.type == FaunaType::Bear) a.g.swim = 0.75f;
            else if (a.type == FaunaType::Herbivore) a.g.swim = 0.1f;
            a.habitatUse = a.fullyAquatic() ? 1.f : 0.2f;
        }
        eco->all().push_back(a);
    }

    // recria grupos sociais a partir dos membros carregados
    {
        std::vector<FaunaGroup> rebuilt;
        for (auto& a : eco->all()) {
            if (a.groupId < 0) continue;
            FaunaGroup* g = nullptr;
            for (auto& gg : rebuilt) if (gg.id == a.groupId) { g = &gg; break; }
            if (!g) {
                FaunaGroup ng;
                ng.id = a.groupId;
                ng.type = a.type;
                rebuilt.push_back(ng);
                g = &rebuilt.back();
            }
            g->memberIds.push_back(a.id);
            g->x += a.x; g->y += a.y;
        }
        for (auto& g : rebuilt) {
            if (!g.memberIds.empty()) {
                g.x /= (float)g.memberIds.size();
                g.y /= (float)g.memberIds.size();
                g.leaderId = g.memberIds[0];
            }
        }
        eco->groups() = rebuilt;
    }

    int nM;
    f >> tag >> nM;
    for (int i = 0; i < nM; i++) {
        Community cm;
        int fire, hut, nMem;
        f >> cm.id >> cm.x >> cm.y >> cm.territoryR >> fire >> hut
          >> cm.foundingDay >> cm.name >> nMem;
        cm.hasFire(); // lido do arquivo antigo
        cm.hasHut();
        if (fire) { Building b; b.type = 0; cm.buildings.push_back(b); }
        if (hut) { Building b; b.type = 1; cm.buildings.push_back(b); }
        for (int j = 0; j < nMem; j++) {
            int id; f >> id;
            cm.memberIds.push_back(id);
        }
        sim->communities().push_back(cm);
    }

    f.close();
    w.rebiome(); // reatribui biomas dos tiles carregados
    // cavernas: reatribui ~10% das rochas como cavernas (não persiste o hash exato)
    {
        // sem acesso a seed aqui — usa posição como hash
        for (int y = 0; y < w.height(); y++) {
            for (int x = 0; x < w.width(); x++) {
                TileData& t = w.at(x, y);
                t.cave = false;
                t.caveFire = false;
                if (t.type == Tile::Rock) {
                    unsigned h = (unsigned)x * 374761393u ^ (unsigned)y * 668265263u;
                    h = (h ^ (h >> 13)) * 1274126177u;
                    if ((h & 0xFF) > 220) t.cave = true; // ~12%
                }
            }
        }
    }
    w.setTimeOfDay(timeAcc / SECONDS_PER_DAY);
    return true;
}

}
