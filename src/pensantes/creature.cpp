#include "creature.hpp"
#include "fauna.hpp"
#include "rng.hpp"
#include <cmath>
#include <algorithm>

namespace skc {

static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

// pensante nunca pode ficar na água — se cair, volta pra terra (sem gelo)
static void snapPenToLand(World& w, float& x, float& y) {
    if (w.walkable(x, y)) {
        int tx = (int)x, ty = (int)y;
        if (w.inBounds(tx, ty) && w.at(tx, ty).type != Tile::Ice) return;
        // está no gelo: continua procurando terra quente
    }
    int tx = (int)x, ty = (int)y;
    for (int r = 1; r <= 12; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int nx = tx + dx, ny = ty + dy;
                if (!w.inBounds(nx, ny)) continue;
                Tile t = w.at(nx, ny).type;
                if (t == Tile::Ice) continue; // nunca pousa no gelo
                if (w.walkable(nx + 0.5f, ny + 0.5f)) {
                    x = nx + 0.5f;
                    y = ny + 0.5f;
                    return;
                }
            }
        }
    }
}

const char* stateName(CState s) {
    switch (s) {
        case CState::Wander: return "Vagando";
        case CState::SeekFood: return "Buscando comida";
        case CState::SeekWater: return "Buscando agua";
        case CState::SeekMate: return "Escolhendo par";
        case CState::SeekShelter: return "Buscando abrigo";
        case CState::Eat: return "Comendo";
        case CState::Drink: return "Bebendo";
        case CState::Rest: return "Descansando";
        case CState::Fight: return "Brigando";
        case CState::Mourn: return "De luto";
        case CState::Investigate: return "Investigando";
        case CState::Gather: return "Em comunidade";
        case CState::Build: return "Construindo";
        case CState::Sick: return "Doente";
        case CState::Dead: return "Morto";
        default: return "?";
    }
}

const char* buildingName(int type) {
    switch (type) {
        case 0: return "Fogueira";
        case 1: return "Toca";
        case 2: return "Casa";
        case 3: return "Celeiro";
        case 4: return "Totem";
        case 5: return "Muralha";
        case 6: return "Templo";
        default: return "?";
    }
}

const char* buildingDesc(int type) {
    switch (type) {
        case 0: return "luz e calor";
        case 1: return "abrigo simples";
        case 2: return "casa de família";
        case 3: return "guarda comida";
        case 4: return "marca o território";
        case 5: return "protege da guerra";
        case 6: return "centro espiritual";
        default: return "";
    }
}

std::string makeName() {
    static const char* syl[] = {
        "ka", "mi", "tu", "ra", "so", "el", "an", "vi", "ne", "po",
        "lu", "shi", "ze", "or", "ae", "bru", "qui", "tha", "mos", "fen"
    };
    int n = irand(2, 3);
    std::string s;
    for (int i = 0; i < n; i++) s += syl[irand(0, 19)];
    s[0] = (char)toupper(s[0]);
    return s;
}

std::string makeCommunityName() {
    static const char* root[] = {"Sol", "Rio", "Pedra", "Fogo", "Raiz", "Ceu", "Luz", "Vento", "Estrela", "Sombra"};
    static const char* suf[] = {"da Colina", "do Lago", "das Arvores", "do Amanhecer", "da Fonte",
                                 "do Ceu", "das Pedras", "da Noite", "do Norte", "da terra"};
    std::string n = root[irand(0, 9)];
    n += " ";
    n += suf[irand(0, 9)];
    return n;
}

Community* CreatureSim::community(int id) {
    for (auto& c : communities_) if (c.id == id) return &c;
    return nullptr;
}

Grave* CreatureSim::graveAt(float wx, float wy, float radius) {
    float r2 = radius * radius;
    Grave* best = nullptr;
    float bd = r2;
    for (auto& g : graves_) {
        float d = dist2(wx, wy, g.x, g.y);
        if (d <= bd) { bd = d; best = &g; }
    }
    return best;
}

const char* CreatureSim::culturalEraName() const {
    switch (culturalEra_) {
        case 0: return "Era Primitiva";
        case 1: return "Descoberta do Fogo";
        case 2: return "Idade das Ferramentas";
        case 3: return "Agricultura";
        case 4: return "Escrita e Cultura";
        case 5: return "Civilizacao";
        default: return "Era Desconhecida";
    }
}

void CreatureSim::updateCommunities(float dt, unsigned day) {
    // remove mortos das comunidades
    for (auto& comm : communities_) {
        comm.memberIds.erase(
            std::remove_if(comm.memberIds.begin(), comm.memberIds.end(),
                           [&](int id) {
                               Creature* c = get(id);
                               return !c || !c->alive() || c->communityId != comm.id;
                           }),
            comm.memberIds.end());
    }
    communities_.erase(
        std::remove_if(communities_.begin(), communities_.end(),
                       [](const Community& c) { return c.memberIds.empty(); }),
        communities_.end());

    // fundação: pensantes amigos e sociáveis se juntam
    for (auto& a : creatures_) {
        if (!a.alive() || a.juvenile() || a.communityId >= 0) continue;
        if (a.g.intelligence < 0.25f) continue;
        for (auto& b : creatures_) {
            if (b.id == a.id || !b.alive() || b.juvenile() || b.communityId >= 0) continue;
            if (dist2(a.x, a.y, b.x, b.y) > 25.f) continue;
            Bond* ba = a.bond(b.id);
            Bond* bb = b.bond(a.id);
            float trust = 0.f;
            if (ba) trust += ba->trust;
            if (bb) trust += bb->trust;
            trust *= 0.5f;
            if (trust < 0.25f) continue;
            if (a.g.sociability + b.g.sociability < 0.8f) continue;
            // fundam comunidade: perto de água+plantas OU perto de CAVERNA
            float cx = (a.x + b.x) * 0.5f;
            float cy = (a.y + b.y) * 0.5f;
            bool goodSite = false;
            if (world_.nearWater(cx, cy, 6.f) &&
                world_.plantDensity((int)cx, (int)cy, 8) >= 0.02f) {
                goodSite = true; // margem clássica
            }
            // caverna: lar natural, não precisa de água por perto
            if (!goodSite) {
                int r = 6;
                for (int y = (int)cy - r; y <= (int)cy + r && !goodSite; y++)
                    for (int x = (int)cx - r; x <= (int)cx + r; x++)
                        if (world_.isCave(x, y)) { goodSite = true; break; }
            }
            if (!goodSite) continue;

            // SE TEM TÚMULO DE FAMÍLIA POR PERTO, funda perto dele (lar junto à memória)
            float prefX = cx, prefY = cy;
            {
                float bestG = 1e9f;
                bool foundG = false;
                for (auto& gv : graves_) {
                    bool family = false;
                    // túmulo de quem tem o mesmo sobrenome ou é parente
                    if (!a.surname.empty() && gv.surname == a.surname) family = true;
                    if (!b.surname.empty() && gv.surname == b.surname) family = true;
                    if (gv.id == a.parentA || gv.id == a.parentB ||
                        gv.id == b.parentA || gv.id == b.parentB) family = true;
                    if (!family) continue;
                    float d = dist2(cx, cy, gv.x, gv.y);
                    if (d < bestG && d < 400.f) { bestG = d; prefX = gv.x + frand(-2, 2); prefY = gv.y + frand(-2, 2); foundG = true; }
                }
                if (foundG) {
                    cx = prefX;
                    cy = prefY;
                    // precisa ser site válido no novo local
                    bool ok2 = world_.nearWater(cx, cy, 8.f);
                    if (!ok2) {
                        int rc = 8;
                        for (int y = (int)cy - rc; y <= (int)cy + rc && !ok2; y++)
                            for (int x = (int)cx - rc; x <= (int)cx + rc; x++)
                                if (world_.isCave(x, y)) { ok2 = true; break; }
                    }
                    if (!ok2) { cx = (a.x + b.x) * 0.5f; cy = (a.y + b.y) * 0.5f; }
                }
            }

            Community comm;
            comm.id = nextCommunityId_++;
            comm.name = makeCommunityName();
            comm.x = cx;
            comm.y = cy;
            comm.foundingDay = day;
            comm.memberIds.push_back(a.id);
            comm.memberIds.push_back(b.id);
            a.communityId = comm.id;
            b.communityId = comm.id;
            // ideias fundadoras
            for (auto& idea : a.ideas) comm.groupIdeas.push_back(idea);
            for (auto& idea : b.ideas) {
                bool dup = false;
                for (auto& gi : comm.groupIdeas) if (gi.text == idea.text) { dup = true; break; }
                if (!dup) comm.groupIdeas.push_back(idea);
            }
            communities_.push_back(comm);
            a.addChronicle(day, "fundou a comunidade " + comm.name);
            b.addChronicle(day, "fundou a comunidade " + comm.name);
            addEvent(comm.name + " nasceu. " + a.name + " e " + b.name + " fincaram a primeira fogueira.", EvKind::Build);
            return; // uma fundação por tick
        }
    }

        // construções e encontros
        for (auto& comm : communities_) {
            if (comm.memberIds.empty()) continue;

            // ---- MOLDAR O TERRENO: o lar transforma a volta ----
            if (chance(0.08f * dt * 30.f)) {
                int tx = (int)comm.x + irand(-5, 5);
                int ty = (int)comm.y + irand(-5, 5);
                if (world_.inBounds(tx, ty)) {
                    TileData& t = world_.at(tx, ty);
                    // pisoteio: grama vira solo batido perto da casa
                    if (t.type == Tile::Grass && !t.cave && t.trees == 0 &&
                        dist2(tx + 0.5f, ty + 0.5f, comm.x, comm.y) < 36.f) {
                        t.type = Tile::Soil;
                    }
                    // árvore demais perto da casa: derruba (clareira)
                    else if (t.trees >= 3 && dist2(tx + 0.5f, ty + 0.5f, comm.x, comm.y) < 16.f) {
                        int got = 0;
                        if (world_.chopTree(tx, ty, got)) {
                            if (chance(0.15f))
                                addEvent(comm.name + " abriu uma clareira perto de casa.", EvKind::Build);
                        }
                    }
                    // solo batido ganha fertilidade (composto)
                    else if (t.type == Tile::Soil && chance(0.1f)) {
                        t.fertility = clampf(t.fertility + 0.01f, 0.f, 1.f);
                    }
                }
            }

            // fogueira e toca: precisam de ferramenta em alguém
            if (!comm.hasFire() || !comm.hasHut()) {
            for (int id : comm.memberIds) {
                Creature* m = get(id);
                if (!m || !m->alive()) continue;
                if (m->k.tool_skill > 0.25f) {
                    if (!comm.hasFire() && m->k.fire_knowledge > 0.1f && chance(0.01f * dt * 30.f)) {
                        // precisa de madeira por perto (ou caverna com lenha)
                        bool hasWood = false;
                        for (int y = (int)comm.y - 8; y <= (int)comm.y + 8 && !hasWood; y++) {
                            for (int x = (int)comm.x - 8; x <= (int)comm.x + 8; x++) {
                                if (!world_.inBounds(x, y)) continue;
                                TileData& t = world_.at(x, y);
                                if (t.wood > 0) { t.wood--; hasWood = true; break; }
                                if (t.trees > 0) {
                                    int got = 0;
                                    if (world_.chopTree(x, y, got)) { hasWood = true; break; }
                                }
                            }
                        }
                        // caverna: fogueira é mais fácil (abrigo pronto)
                        if (world_.isCave((int)comm.x, (int)comm.y)) hasWood = true;
                        if (hasWood) {
                            // prefere perto de túmulo de família (lar junto à memória)
                            float bx = comm.x + frand(-2, 2), by = comm.y + frand(-2, 2);
                            for (auto& gv : graves_) {
                                bool fam = false;
                                for (int id : comm.memberIds) {
                                    Creature* mem = get(id);
                                    if (!mem) continue;
                                    if (!mem->surname.empty() && gv.surname == mem->surname) { fam = true; break; }
                                }
                                if (fam && dist2(gv.x, gv.y, comm.x, comm.y) < 100.f) {
                                    bx = gv.x + frand(-1.5f, 1.5f);
                                    by = gv.y + frand(-1.5f, 1.5f);
                                    break;
                                }
                            }
                            Building b; b.type = 0; b.x = bx; b.y = by;
                            comm.buildings.push_back(b);
                            changeReputation(*m, 0.05f, day, "construiu fogueira");
                            // acende fogueira na caverna se a comunidade está numa
                            if (world_.isCave((int)comm.x, (int)comm.y)) {
                                world_.at((int)comm.x, (int)comm.y).caveFire = true;
                                addEvent(comm.name + " acendeu fogueira na caverna. O lar esquentou.", EvKind::Build);
                            } else {
                                addEvent(comm.name + " acendeu a primeira fogueira (com lenha).", EvKind::Build);
                            }
                        }
                    }
                    if (!comm.hasHut() && m->k.tool_skill > 0.35f && chance(0.008f * dt * 30.f)) {
                        bool hasWood = false;
                        for (int y = (int)comm.y - 8; y <= (int)comm.y + 8 && !hasWood; y++) {
                            for (int x = (int)comm.x - 8; x <= (int)comm.x + 8; x++) {
                                if (!world_.inBounds(x, y)) continue;
                                TileData& t = world_.at(x, y);
                                if (t.wood > 0) { t.wood--; hasWood = true; break; }
                                if (t.trees > 0) {
                                    int got = 0;
                                    if (world_.chopTree(x, y, got)) { hasWood = true; break; }
                                }
                            }
                        }
                        if (hasWood) {
                            Building b; b.type = 1; b.x = comm.x + frand(-3, 3); b.y = comm.y + frand(-3, 3);
                            comm.buildings.push_back(b);
                            addEvent(comm.name + " ergueu a primeira toca.", EvKind::Build);
                        }
                    }
                    // construções avançadas (limitadas pela era cultural)
                    if (comm.hasHut() && comm.totalBuildings() < 8) {
                        // casa (era ferramentas+)
                        if (m->k.tool_skill > 0.5f && m->culturalLevel >= 2 && !comm.hasBuilding(2) &&
                            chance(0.003f * dt * 30.f)) {
                            Building b; b.type = 2; b.x = comm.x + frand(-4, 4); b.y = comm.y + frand(-4, 4);
                            comm.buildings.push_back(b);
                            addEvent(comm.name + " construiu uma casa.", EvKind::Build);
                        }
                        // celeiro (era agricultura+)
                        if (m->tool == 4 && m->culturalLevel >= 3 && !comm.hasBuilding(3) &&
                            chance(0.004f * dt * 30.f)) {
                            Building b; b.type = 3; b.x = comm.x + frand(-4, 4); b.y = comm.y + frand(-4, 4);
                            comm.buildings.push_back(b);
                            addEvent(comm.name + " construiu um celeiro.", EvKind::Build);
                        }
                        // totem (marca território)
                        if (m->g.intelligence > 0.4f && !comm.hasBuilding(4) &&
                            chance(0.003f * dt * 30.f)) {
                            Building b; b.type = 4; b.x = comm.x; b.y = comm.y - 3;
                            comm.buildings.push_back(b);
                            addEvent(comm.name + " ergueu um totem.", EvKind::Build);
                        }
                        // muralha (se tem guerra)
                        if (m->tool == 3 && !comm.hasBuilding(5) && comm.totalBuildings() >= 3 &&
                            chance(0.002f * dt * 30.f)) {
                            Building b; b.type = 5; b.x = comm.x; b.y = comm.y;
                            comm.buildings.push_back(b);
                            addEvent(comm.name + " construiu uma muralha.", EvKind::Build);
                        }
                        // templo (era escrita+)
                        if (m->culturalLevel >= 4 && !comm.hasBuilding(6) && m->g.intelligence > 0.5f &&
                            chance(0.002f * dt * 30.f)) {
                            Building b; b.type = 6; b.x = comm.x + 2; b.y = comm.y + 2;
                            comm.buildings.push_back(b);
                            addEvent(comm.name + " ergueu um templo!", EvKind::Build);
                        }
                    }
                }
            }
        }
        // ideia do grupo se fortalece e contamina membros
        for (int id : comm.memberIds) {
            Creature* m = get(id);
            if (!m || !m->alive()) continue;
            if (!comm.groupIdeas.empty()) {
                const Idea& gi = comm.groupIdeas[irand(0, (int)comm.groupIdeas.size() - 1)];
                if (Idea* mine = m->findIdea(gi.text)) {
                    mine->strength = clampf(lerpf(mine->strength, gi.strength, 0.02f * dt), 0.f, 1.f);
                } else if (chance(0.01f * dt)) {
                    m->formIdea(gi.kind, gi.text, gi.strength * 0.6f, day);
                }
            }
            // membros ficam perto de casa
            float dHome = dist2(m->x, m->y, comm.x, comm.y);
            if (dHome > comm.territoryR * comm.territoryR && m->state != CState::SeekFood &&
                m->state != CState::SeekWater && m->state != CState::Fight) {
                // puxa pra casa — mas nunca pra dentro da água
                if (chance(0.3f * dt)) {
                    m->targetX = comm.x + gauss(0.f, 3.f);
                    m->targetY = comm.y + gauss(0.f, 3.f);
                    snapPenToLand(world_, m->targetX, m->targetY);
                }
            }
        }
    }

    // encontro entre comunidades diferentes
    for (size_t i = 0; i < communities_.size(); i++) {
        for (size_t j = i + 1; j < communities_.size(); j++) {
            Community& A = communities_[i];
            Community& B = communities_[j];
            if (dist2(A.x, A.y, B.x, B.y) > 30.f * 30.f) continue;
            // um membro de A perto de um de B
            for (int idA : A.memberIds) {
                Creature* ma = get(idA);
                if (!ma || !ma->alive()) continue;
                for (int idB : B.memberIds) {
                    Creature* mb = get(idB);
                    if (!mb || !mb->alive()) continue;
                    if (dist2(ma->x, ma->y, mb->x, mb->y) > 16.f) continue;

                    Bond& b = ma->ensureBond(mb->id, day);
                    // GUERRA: moral baixa + agressividade + arma = conflito
                    bool war = (ma->morality < 0.35f && ma->g.aggression > 0.5f && ma->tool == 3 &&
                                chance(0.03f * dt));
                    if (war) {
                        damage(*mb, 12.f, ma->id, day);
                        ma->em.anger = clampf(ma->em.anger + 0.3f, 0.f, 1.f);
                        changeReputation(*ma, -0.2f, day, "guerra entre tribos");
                        addEvent("GUERRA! " + A.name + " atacou " + B.name + "!", EvKind::War);
                    } else {
                        // troca de ideias entre tribos
                        b.trust = clampf(b.trust + 0.04f * dt, -1.f, 1.f);
                        if (!A.groupIdeas.empty() && chance(0.04f * dt)) {
                            const Idea& idea = A.groupIdeas[irand(0, (int)A.groupIdeas.size() - 1)];
                            bool dup = false;
                            for (auto& gi : B.groupIdeas) if (gi.text == idea.text) { dup = true; break; }
                            if (!dup && B.groupIdeas.size() < 10) {
                                B.groupIdeas.push_back(idea);
                                addEvent(B.name + " recebeu uma ideia de " + A.name + ".", EvKind::Idea);
                            }
                        }
                        // casamento entre tribos
                        if (ma->male != mb->male && ma->mateCooldown <= 0.f && mb->mateCooldown <= 0.f &&
                            ma->energy > 60.f && mb->energy > 60.f && chance(0.01f * dt)) {
                            ma->em.mateId = mb->id;
                            mb->em.mateId = ma->id;
                            ma->communityId = B.id;
                            addEvent(ma->name + " se juntou a " + B.name + " por amor.", EvKind::Love);
                        }
                    }
                    return; // um encontro por tick
                }
            }
        }
    }
}

void CreatureSim::learnPleasure(Creature& c, float amount) {
    // aprender é prazeroso — reforço positivo que motiva explorar
    c.em.satisfaction = clampf(c.em.satisfaction + amount, 0.f, 1.f);
    c.em.curiosity = clampf(c.em.curiosity + 0.03f, 0.f, 1.f);
    c.em.happiness = clampf(c.em.happiness + amount * 0.3f, 0.f, 1.f);
}

void CreatureSim::notifyChildDeath(const Creature& child) {
    for (int pid : {child.parentA, child.parentB}) {
        if (pid < 0) continue;
        Creature* p = get(pid);
        if (!p || !p->alive()) continue;
        p->em.grief = clampf(p->em.grief + 0.45f, 0.f, 1.f);
        p->em.happiness = clampf(p->em.happiness - 0.35f, 0.f, 1.f);
        p->em.love = clampf(p->em.love - 0.1f, 0.f, 1.f);
        // LUTO COM LEMBRANÇA: o filho morto vira memória e respeito
        if (p->em.grief < 0.3f) {
            addEvent(p->name + " sentiu o vazio de perder um filho.", EvKind::Death);
            p->formIdea(IdeaKind::Love, "perder quem se ama doeu fundo", 0.55f, currentDay_, 0.45f);
            p->formIdea(IdeaKind::Self, "a vida e fragil", 0.4f, currentDay_, 0.5f);
            p->em.respect = clampf(p->em.respect + 0.25f, 0.f, 1.f);
            p->formIdea(IdeaKind::Community, "os mortos merecem lembranca", 0.45f, currentDay_, 0.85f);
        }
        // marca o local da perda como "túmulo" pra poder voltar e lembrar
        if (p->graveX < 0.f) {
            p->graveX = child.x;
            p->graveY = child.y;
            p->graveName = child.name;
        }
        // se sabemos quem matou (wrath do filho), sangue odeia sangue
        if (child.em.wrathTarget >= 0) {
            if (Creature* killer = get(child.em.wrathTarget)) {
                if (!killer->surname.empty())
                    p->bloodEnemy = killer->surname;
                else if (!killer->name.empty())
                    p->bloodEnemy = killer->name;
            }
        }
    }
}

void Creature::addChronicle(unsigned day, const std::string& text) {
    // não repete a mesma entrada no mesmo dia
    if (!chronicle.empty() && chronicle.back().day == day && chronicle.back().text == text)
        return;
    chronicle.push_back(LifeEvent{day, text});
    if (chronicle.size() > 80) chronicle.erase(chronicle.begin());
}

void Creature::formIdea(IdeaKind kind, const std::string& text, float power, unsigned day, float morality) {
    for (auto& idea : ideas) {
        if (idea.kind == kind && idea.text == text) {
            idea.strength = clampf(idea.strength + power, 0.f, 1.f);
            idea.formedDay = day;
            return;
        }
    }
    Idea idea;
    idea.kind = kind;
    idea.text = text;
    idea.strength = clampf(power, 0.05f, 1.f);
    idea.morality = clampf(morality, 0.f, 1.f);
    idea.formedDay = day;
    idea.selfTaught = true; // default: descoberta própria
    ideas.push_back(idea);
    // CRÔNICA: toda ideia nova vira linha do tempo
    addChronicle(day, "teve a ideia: \"" + text + "\"");
    if (ideas.size() > 18) {
        auto weakest = std::min_element(ideas.begin(), ideas.end(),
            [](const Idea& a, const Idea& b) { return a.strength < b.strength; });
        ideas.erase(weakest);
    }
}

Idea* Creature::findIdea(const std::string& fragment) {
    for (auto& idea : ideas) {
        if (idea.text.find(fragment) != std::string::npos) return &idea;
    }
    return nullptr;
}

const Idea* Creature::findIdea(const std::string& fragment) const {
    for (auto& idea : ideas) {
        if (idea.text.find(fragment) != std::string::npos) return &idea;
    }
    return nullptr;
}

void Creature::applyIdeas(float dt) {
    // moral emerge da média das ideias
    float mSum = 0.f, mW = 0.f;
    for (auto& idea : ideas) {
        mSum += idea.morality * idea.strength;
        mW += idea.strength;
    }
    if (mW > 0.1f) {
        float ideMorality = mSum / mW;
        morality = lerpf(morality, ideMorality, clampf(0.06f * dt, 0.f, 1.f));
    }
    float bias = (pers.empathy - 0.5f) * 0.025f - (g.aggression - 0.4f) * 0.02f;
    morality = clampf(morality + bias * dt, 0.f, 1.f);

    em.guilt = clampf(em.guilt - 0.03f * dt, 0.f, 1.f);
    em.shame = clampf(em.shame - 0.02f * dt, 0.f, 1.f);
    em.pride = clampf(em.pride - 0.025f * dt, 0.f, 1.f);

    // ideias moldam emoções de forma contínua
    if (const Idea* i = findIdea("lobo")) {
        if (i->text.find("inimigo") != std::string::npos || i->text.find("perigo") != std::string::npos)
            em.fear = clampf(em.fear + i->strength * 0.03f * dt, 0.f, 1.f);
    }
    if (const Idea* i = findIdea("juntos")) {
        em.loneliness = clampf(em.loneliness - i->strength * 0.06f * dt, 0.f, 1.f);
        em.security = clampf(em.security + i->strength * 0.02f * dt, 0.f, 1.f);
    }
    if (const Idea* i = findIdea("amar")) {
        if (i->text.find("doe") != std::string::npos || i->text.find("machuca") != std::string::npos)
            em.love = clampf(em.love - i->strength * 0.015f * dt, 0.f, 1.f);
        else
            em.love = clampf(em.love + i->strength * 0.025f * dt, 0.f, 1.f);
    }
    if (const Idea* i = findIdea("comida")) {
        if (i->text.find("vale") != std::string::npos || i->text.find("ouro") != std::string::npos)
            pers.greed = clampf(pers.greed + i->strength * 0.01f * dt, 0.f, 1.f);
    }
    if (const Idea* i = findIdea("cur")) {
        em.comfort = clampf(em.comfort + i->strength * 0.02f * dt, 0.f, 1.f);
    }
    if (const Idea* i = findIdea("por que")) {
        em.curiosity = clampf(em.curiosity + i->strength * 0.03f * dt, 0.f, 1.f);
    }
    // ideia de caverna: quem acredita busca abrigo com mais garra
    if (const Idea* i = findIdea("caverna")) {
        em.comfort = clampf(em.comfort + i->strength * 0.015f * dt, 0.f, 1.f);
        em.security = clampf(em.security + i->strength * 0.01f * dt, 0.f, 1.f);
    }
    // ideias de família reforçam o desejo
    if (const Idea* i = findIdea("familia")) {
        em.desireFamily = clampf(em.desireFamily + i->strength * 0.02f * dt, 0.f, 1.f);
    }
    if (const Idea* i = findIdea("sozinho")) {
        if (i->text.find("doeu") != std::string::npos || i->text.find("dor") != std::string::npos)
            em.loneliness = clampf(em.loneliness + i->strength * 0.02f * dt, 0.f, 1.f);
    }
    // ideias de felicidade/autoestima
    if (const Idea* i = findIdea("alegria")) {
        em.happiness = clampf(em.happiness + i->strength * 0.015f * dt, 0.f, 1.f);
    }
}

CreatureSim::CreatureSim(World& world) : world_(world) {}

void CreatureSim::addEvent(const std::string& s, EvKind kind) {
    SimEvent e;
    e.text = s;
    e.t = 0.f;
    e.kind = kind;
    // eventos importantes ficam mais tempo no log
    switch (kind) {
        case EvKind::Birth:     e.life = 20.f; break;
        case EvKind::Death:     e.life = 22.f; break;
        case EvKind::Discovery: e.life = 18.f; break;
        case EvKind::Build:     e.life = 16.f; break;
        case EvKind::War:       e.life = 18.f; break;
        case EvKind::Love:      e.life = 16.f; break;
        case EvKind::Idea:      e.life = 14.f; break;
        case EvKind::Health:    e.life = 14.f; break;
        default:                e.life = 12.f; break;
    }
    events_.push_back(e);
    if (events_.size() > 60) events_.erase(events_.begin());
}

// reputação muda com motivo visível — nunca "do nada"
void CreatureSim::changeReputation(Creature& c, float delta, unsigned day, const char* why) {
    float old = c.reputation;
    c.reputation = clampf(c.reputation + delta, -1.f, 1.f);
    // loga quando cruza um limiar (evita spam)
    bool crossedDown = old > -0.25f && c.reputation <= -0.25f;
    bool crossedBad = old > -0.55f && c.reputation <= -0.55f;
    bool crossedUp = old < 0.2f && c.reputation >= 0.2f;
    if (crossedDown)
        addEvent(c.name + ": reputacao caiu. (" + std::string(why) + ")", EvKind::War);
    else if (crossedBad)
        addEvent(c.name + " e visto com desconfianca por todos. (" + std::string(why) + ")", EvKind::War);
    else if (crossedUp && delta > 0)
        addEvent(c.name + " ganhou respeito da comunidade.", EvKind::Build);
}

void CreatureSim::spawnStarter(int count) {
    int tries = 0;
    int made = 0;
    while (made < count && tries < count * 200) {
        tries++;
        int x = irand(4, world_.width() - 5);
        int y = irand(4, world_.height() - 5);
        const TileData& t = world_.at(x, y);
        if (!world_.walkable((float)x + 0.5f, (float)y + 0.5f)) continue;
        if (t.type == Tile::Rock || t.type == Tile::Ice) continue;

        Creature c;
        c.id = nextId_++;
        c.name = makeName();
        c.surname = chance(0.5f) ? c.name : makeName();
        c.x = x + frand(0.2f, 0.8f);
        c.y = y + frand(0.2f, 0.8f);
        c.male = chance(0.5f);
        c.energy = frand(70.f, 95.f);
        c.hydration = frand(70.f, 95.f);
        c.age = frand(1.6f, 4.f);
        c.k.plant_value = clampf(gauss(0.55f, 0.1f), 0.25f, 0.85f);
        c.g.speed = clampf(gauss(1.f, 0.15f), 0.4f, 1.8f);
        c.g.metabolism = clampf(gauss(1.f, 0.12f), 0.5f, 1.6f);
        c.g.size = clampf(gauss(1.f, 0.1f), 0.6f, 1.5f);
        c.g.vision = clampf(gauss(7.f, 1.2f), 4.f, 12.f);
        c.g.fertility = clampf(gauss(1.f, 0.15f), 0.4f, 1.6f);
        c.g.robustness = clampf(gauss(0.5f, 0.15f), 0.1f, 0.9f);
        c.g.lifespan = clampf(gauss(10.f, 1.5f), 6.f, 14.f);
        // corpo e mente: cada um é único
        c.body.strength = clampf(gauss(0.5f, 0.2f), 0.1f, 1.f);
        c.body.endurance = clampf(gauss(0.5f, 0.2f), 0.1f, 1.f);
        c.body.agility = clampf(gauss(0.5f, 0.2f), 0.1f, 1.f);
        c.body.health = 1.f;
        c.mind.wisdom = clampf(gauss(0.15f, 0.1f), 0.f, 0.5f);
        c.mind.memory = clampf(gauss(0.3f, 0.15f), 0.05f, 0.8f);
        c.mind.creativity = clampf(gauss(0.3f, 0.2f), 0.f, 0.9f);
        c.mind.willpower = clampf(gauss(0.5f, 0.2f), 0.1f, 1.f);
        c.g.aggression = clampf(gauss(0.3f, 0.1f), 0.05f, 0.9f);
        c.g.intelligence = clampf(gauss(0.25f, 0.1f), 0.05f, 0.7f);
        c.morality = clampf(0.5f + (c.pers.empathy - 0.5f) * 0.5f - (c.g.aggression - 0.3f) * 0.3f, 0.1f, 0.9f);
        c.tool = 0;
        // personalidade: cada um é UM
        c.pers.sexuality = clampf(gauss(0.5f, 0.28f), 0.f, 1.f);
        c.pers.boldness = clampf(gauss(0.5f, 0.28f), 0.f, 1.f);
        c.pers.thinker = clampf(gauss(0.5f, 0.28f), 0.f, 1.f);
        c.pers.greed = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.pers.laziness = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.pers.empathy = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.pers.wanderlust = clampf(gauss(0.5f, 0.28f), 0.f, 1.f);
        c.pers.attachment = clampf(gauss(0.5f, 0.3f), 0.f, 1.f);
        c.pers.optimism = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        // estado emocional inicial
        c.em.happiness = clampf(0.4f + c.pers.optimism * 0.3f, 0.f, 1.f);
        c.em.security = 0.4f;
        // cada um quer um tipo diferente de par
        // cada fêmea é ÚNICA — preferências bem variadas
        c.prefs.likeStrength = clampf(gauss(0.5f, 0.35f), 0.f, 1.f);
        c.prefs.likeSmarts = clampf(gauss(0.5f, 0.35f), 0.f, 1.f);
        c.prefs.likeSkill = clampf(gauss(0.5f, 0.35f), 0.f, 1.f);
        c.prefs.likeSize = clampf(gauss(0.5f, 0.35f), 0.f, 1.f);
        c.prefs.likeCalm = clampf(gauss(0.5f, 0.35f), 0.f, 1.f);
        c.prefs.likeYoung = clampf(gauss(0.5f, 0.35f), 0.f, 1.f);
        c.prefs.likeHair = clampf(gauss(0.5f, 0.4f), 0.f, 1.f);
        c.prefs.likeVoice = clampf(gauss(0.5f, 0.4f), 0.f, 1.f);
        c.prefs.likeGenerosity = clampf(gauss(0.5f, 0.4f), 0.f, 1.f);
        c.prefs.likeCourage = clampf(gauss(0.5f, 0.4f), 0.f, 1.f);
        c.prefs.likeLoyalty = clampf(gauss(0.5f, 0.4f), 0.f, 1.f);
        c.prefs.likeHumor = clampf(gauss(0.5f, 0.4f), 0.f, 1.f);
        c.prefs.likeWealth = clampf(gauss(0.5f, 0.4f), 0.f, 1.f);
        creatures_.push_back(c);
        made++;
    }
    addEvent("As primeiras criaturas despertaram no mundo.", EvKind::Discovery);
}

int CreatureSim::aliveCount() const {
    int n = 0;
    for (const auto& c : creatures_) if (c.alive()) n++;
    return n;
}

CreatureSim::Stats CreatureSim::stats() const {
    Stats s;
    int n = 0;
    for (const auto& c : creatures_) {
        if (!c.alive()) continue;
        s.avgSpeed += c.g.speed;
        s.avgVision += c.g.vision;
        s.avgLife += c.g.lifespan;
        s.avgRobust += c.g.robustness;
        s.avgTool += c.k.tool_skill;
        s.avgFire += c.k.fire_knowledge;
        s.avgPlantValue += c.k.plant_value;
        n++;
    }
    if (n > 0) {
        s.avgSpeed /= n;
        s.avgVision /= n;
        s.avgLife /= n;
        s.avgRobust /= n;
        s.avgTool /= n;
        s.avgFire /= n;
        s.avgPlantValue /= n;
        s.generations = births_;
    }
    return s;
}

Creature* CreatureSim::get(int id) {
    for (auto& c : creatures_) if (c.id == id) return &c;
    return nullptr;
}

void CreatureSim::killInRadius(float cx, float cy, float radius) {
    float r2 = radius * radius;
    for (auto& c : creatures_) {
        if (!c.alive()) continue;
        if (dist2(c.x, c.y, cx, cy) <= r2) {
            c.energy = 0.f;
            c.state = CState::Dead;
            deaths_++;
            // deixa rastro de morte no chão
            int tx = (int)c.x, ty = (int)c.y;
            if (world_.inBounds(tx, ty)) {
                TileData& t = world_.at(tx, ty);
                t.corpse = (uint8_t)clampf(t.corpse + 6.f + c.g.size * 3.f, 0.f, 15.f);
            }
        }
    }
}

void CreatureSim::damage(Creature& c, float amount, int attackerId, unsigned day) {
    c.energy -= amount;
    c.em.anger = clampf(c.em.anger + 0.45f, 0.f, 1.f);
    c.em.fear = clampf(c.em.fear + 0.2f, 0.f, 1.f);
    if (attackerId >= 0) {
        c.em.wrathTarget = attackerId;
        Bond& b = c.ensureBond(attackerId, day);
        b.trust = clampf(b.trust - 0.6f, -1.f, 1.f);
        b.affection = clampf(b.affection - 0.4f, -1.f, 1.f);
        b.grudge = clampf(b.grudge + 0.55f, 0.f, 1.f);
        c.lastDamageFrom = (float)attackerId;

        // o agressor sente culpa se tem moral; senão, impunidade
        if (Creature* atk = get(attackerId)) {
            if (atk->morality > 0.4f) {
                atk->em.guilt = clampf(atk->em.guilt + 0.3f * (atk->morality), 0.f, 1.f);
                changeReputation(*atk, -0.12f, day, "agressao com moral");
                if (chance(0.3f * atk->morality)) {
                    atk->formIdea(IdeaKind::Self, "machucar doeu em ambos", 0.35f, day, 0.65f);
                }
            } else {
                // impunidade: sem moral, quase nada cai (só testemunhas marcam)
                changeReputation(*atk, -0.02f, day, "agressao");
            }
            // a vítima espalha o que aconteceu (UMA vez, não por testemunha)
            if (chance(0.35f)) {
                changeReputation(*atk, -0.08f, day, "vitima contou");
                for (auto& w : creatures_) {
                    if (w.id == c.id || w.id == attackerId || !w.alive()) continue;
                    if (dist2(c.x, c.y, w.x, w.y) > 40.f) continue;
                    Bond& wb = w.ensureBond(attackerId, day);
                    wb.trust = clampf(wb.trust - 0.3f, -1.f, 1.f);
                    wb.grudge = clampf(wb.grudge + 0.2f, 0.f, 1.f);
                }
            }
        }

        if (c.g.intelligence > 0.3f)
            c.formIdea(IdeaKind::Social, "uns querem me machucar", 0.45f, day, 0.3f);
        else
            c.formIdea(IdeaKind::Social, "os fortes mandam", 0.4f, day, 0.15f);
        c.formIdea(IdeaKind::Self, "preciso me defender", 0.5f, day, 0.5f);
    }
    if (c.energy <= 0.f) {
        c.energy = 0.f;
        c.state = CState::Dead;
        deaths_++;
    }
}

float CreatureSim::thermalStress(const Creature& c) const {
    int tx = (int)c.x, ty = (int)c.y;
    if (!world_.inBounds(tx, ty)) return 0.4f;
    const TileData& t = world_.at(tx, ty);
    // CAVERNA: abrigo perfeito — quase zero estresse térmico
    if (t.cave) {
        return 0.05f + (t.caveFire ? 0.f : 0.05f); // sem fogueira ainda, mas quase nada
    }
    float stress = 0.f;

    // frio: noite + gelo/rocha exposta
    if (t.type == Tile::Ice) stress += 0.55f;
    if (t.type == Tile::Rock && world_.isNight()) stress += 0.25f;
    if (world_.isNight()) {
        stress += 0.35f;
        // floresta = abrigo natural
        if (t.type == Tile::Forest) stress -= 0.3f;
        // fogo conhecido ameniza muito
        if (c.k.fire_knowledge > 0.25f) stress -= 0.25f * c.k.fire_knowledge;
    }
    // calor: areia/rocha de dia
    if (!world_.isNight() && (t.type == Tile::Sand || t.type == Tile::Rock)) {
        stress += 0.2f;
        if (t.moisture < 0.15f) stress += 0.15f;
    }
    return clampf(stress, 0.f, 1.f);
}

void CreatureSim::updateEmotions(Creature& c, float dt, unsigned day) {
    Emotions& e = c.em;
    c.applyIdeas(dt);

    // decaimento natural (emoções não são estáticas)
    e.fear = clampf(e.fear - (0.06f + c.pers.optimism * 0.04f) * dt, 0.f, 1.f);
    e.anger = clampf(e.anger - 0.04f * dt, 0.f, 1.f);
    e.grief = clampf(e.grief - (0.02f + c.pers.optimism * 0.03f) * dt, 0.f, 1.f);
    e.loneliness = clampf(e.loneliness + 0.02f * dt, 0.f, 1.f);

    // conforto térmico: o corpo sente o mundo
    float stress = thermalStress(c);
    e.comfort = lerpf(e.comfort, clampf(1.f - stress, 0.f, 1.f), clampf(1.5f * dt, 0.f, 1.f));
    if (stress > 0.4f) {
        c.energy -= stress * 0.3f * dt;
        if (stress > 0.55f) e.fear = clampf(e.fear + 0.08f * dt, 0.f, 1.f);
    }
    // CAVERNA: quem usa aprende que é abrigo e segurança
    if (world_.inBounds((int)c.x, (int)c.y) && world_.at((int)c.x, (int)c.y).cave) {
        e.comfort = clampf(e.comfort + 0.06f * dt, 0.f, 1.f);
        e.security = clampf(e.security + 0.08f * dt, 0.f, 1.f);
        e.fear = clampf(e.fear - 0.06f * dt, 0.f, 1.f);
        // APRENDIZADO: a caverna vira crença
        if (chance(0.015f * dt * 30.f)) {
            if (!c.findIdea("caverna"))
                c.formIdea(IdeaKind::World, "a caverna e abrigo contra o mundo", 0.5f, day, 0.75f);
            else if (!c.findIdea("segura") && c.findIdea("caverna"))
                c.formIdea(IdeaKind::Self, "dentro da rocha estou seguro", 0.4f, day, 0.8f);
        }
        if (world_.at((int)c.x, (int)c.y).caveFire && chance(0.01f * dt * 30.f)) {
            if (!c.findIdea("fogo na caverna"))
                c.formIdea(IdeaKind::Community, "fogo na caverna e o lar", 0.5f, day, 0.85f);
        }
    }

    // medo: predador por perto
    float px, py;
    int threatId = -1;
    if (findThreat(c, px, py, threatId)) {
        float d = dist2(c.x, c.y, px, py);
        float prox = clampf(1.f - d / 40.f, 0.f, 1.f);
        e.fear = clampf(e.fear + prox * 0.55f * dt, 0.f, 1.f);
    }

    // raiva cresce se tem alvo de vingança vivo
    if (e.wrathTarget >= 0) {
        Creature* enemy = get(e.wrathTarget);
        if (!enemy || !enemy->alive()) {
            e.wrathTarget = -1;
            e.grief = clampf(e.grief + 0.12f, 0.f, 1.f);
        } else {
            float d = dist2(c.x, c.y, enemy->x, enemy->y);
            if (d < 100.f) e.anger = clampf(e.anger + 0.2f * dt, 0.f, 1.f);
        }
    }

    // ---- PENSAMENTOS ESCUROS (raros, com raiva alta) ----
    // raiva satura → ideias sombrias que podem ser herdadas
    if (e.anger > 0.65f && c.g.intelligence > 0.15f && chance(0.008f * dt * 30.f)) {
        int roll = irand(0, 5);
        switch (roll) {
            case 0:
                if (!c.findIdea("dominar"))
                    c.formIdea(IdeaKind::Self, "so os fortes merecem viver", 0.45f, day, 0.1f);
                break;
            case 1:
                if (!c.findIdea("vinganca justa"))
                    c.formIdea(IdeaKind::Social, "vinganca e a unica justica", 0.5f, day, 0.2f);
                break;
            case 2:
                if (!c.findIdea("odio") && !c.bloodEnemy.empty())
                    c.formIdea(IdeaKind::Social, "os " + c.bloodEnemy + " sao o mal", 0.55f, day, 0.05f);
                break;
            case 3:
                if (!c.findIdea("desconfia"))
                    c.formIdea(IdeaKind::Social, "ninguem e de confiar", 0.4f, day, 0.25f);
                break;
            case 4:
                // ódio direto a alguém que machucou a família
                if (c.em.wrathTarget >= 0) {
                    if (Creature* en = get(c.em.wrathTarget)) {
                        std::string enName = en->surname.empty() ? en->name : en->surname;
                        if (!c.findIdea("odeio " + enName))
                            c.formIdea(IdeaKind::Social, "odeio " + enName, 0.6f, day, 0.05f);
                        // marca inimigo de sangue
                        if (!en->surname.empty()) c.bloodEnemy = en->surname;
                    }
                }
                break;
            case 5:
                if (!c.findIdea("mundo cruel"))
                    c.formIdea(IdeaKind::World, "o mundo e cruel e fraco", 0.4f, day, 0.2f);
                break;
        }
        // moral cai com pensamentos escuros
        c.morality = clampf(c.morality - 0.02f, 0.f, 1.f);
    }

    // =========================================================
    //  ECONOMIA DE BEM-ESTAR
    //  amor → conforto → menos solidão
    //  companheiro → amor + segurança + conforto
    //  filhotes → felicidade; perder → tristeza
    //  casa/comunidade → segurança
    //  solidão → tristeza
    // =========================================================

    bool mateNear = false;
    Creature* mate = nullptr;
    if (e.mateId >= 0) {
        mate = get(e.mateId);
        if (!mate || !mate->alive()) {
            // perdeu o par: luto com lembrança
            if (e.grief < 0.2f) {
                addEvent(c.name + " sente a falta de quem amava.", EvKind::Love);
                c.formIdea(IdeaKind::Love, "amar pode doer", 0.45f, day, 0.5f);
                c.formIdea(IdeaKind::Community, "os mortos merecem lembranca", 0.4f, day, 0.85f);
                e.respect = clampf(e.respect + 0.2f, 0.f, 1.f);
                // guarda o local do túmulo
                if (c.graveX < 0.f) {
                    c.graveX = mate->x;
                    c.graveY = mate->y;
                    c.graveName = mate->name;
                }
                // se o par tinha inimigo de sangue, herda o ódio
                if (!mate->bloodEnemy.empty() && c.bloodEnemy.empty())
                    c.bloodEnemy = mate->bloodEnemy;
            }
            e.grief = clampf(e.grief + 0.08f * dt, 0.f, 1.f);
            e.love = clampf(e.love - 0.1f * dt, 0.f, 1.f);
            e.security = clampf(e.security - 0.05f * dt, 0.f, 1.f);
            mate = nullptr;
        } else {
            float d = dist2(c.x, c.y, mate->x, mate->y);
            mateNear = d < 40.f;
        }
    }

    // --- COMPANHEIRO presente → amor, segurança, conforto, fim da solidão ---
    if (mateNear && mate) {
        float d = dist2(c.x, c.y, mate->x, mate->y);
        if (d < 25.f) {
            // compatibilidade: atração mutua alimenta o conforto do casal
            float compat = clampf(c.attractionTo(*mate) * 0.6f +
                                  (c.bond(mate->id) ? c.bond(mate->id)->affection * 0.4f : 0.f), 0.f, 1.f);
            float attachW = 0.5f + c.pers.attachment * 0.7f;
            e.love = clampf(e.love + 0.14f * dt * attachW, 0.f, 1.f);
            e.security = clampf(e.security + 0.1f * dt, 0.f, 1.f);
            // amor + compatibilidade + companheiro = conforto
            e.comfort = clampf(e.comfort + e.love * compat * 0.12f * dt, 0.f, 1.f);
            // solidão some com companheiro
            e.loneliness = clampf(e.loneliness - 0.35f * dt, 0.f, 1.f);
            // prazer de estar junto
            e.satisfaction = clampf(e.satisfaction + 0.015f * dt * e.love, 0.f, 1.f);
            Bond& b = c.ensureBond(mate->id, day);
            b.trust = clampf(b.trust + 0.04f * dt, -1.f, 1.f);
            b.affection = clampf(b.affection + 0.06f * dt, -1.f, 1.f);
        } else if (d < 100.f) {
            // par um pouco distante: saudade puxa
            e.loneliness = clampf(e.loneliness + 0.01f * dt, 0.f, 1.f);
            e.love = clampf(e.love + 0.03f * dt, 0.f, 1.f);
        }
    }

    // --- AMOR dá conforto e prazer ---
    e.comfort = clampf(e.comfort + e.love * 0.04f * dt, 0.f, 1.f);
    if (e.love > 0.5f) e.satisfaction = clampf(e.satisfaction + 0.008f * dt, 0.f, 1.f);

    // --- AMIGOS/FAMÍLIA por perto também puxam a solidão ---
    {
        int friends = 0;
        int kidsNear = 0;
        for (auto& o : creatures_) {
            if (o.id == c.id || !o.alive()) continue;
            if (o.id == e.mateId) continue; // par já tratado
            Bond* b = c.bond(o.id);
            bool family = (o.id == c.parentA || o.id == c.parentB ||
                           std::find(c.childrenIds.begin(), c.childrenIds.end(), o.id) != c.childrenIds.end() ||
                           (!c.surname.empty() && o.surname == c.surname));
            if (o.juvenile()) {
                kidsNear++;
                // criança por perto = família: solidão cai, conforto sobe
                e.loneliness = clampf(e.loneliness - 0.05f * dt, 0.f, 1.f);
                e.comfort = clampf(e.comfort + 0.015f * dt, 0.f, 1.f);
                e.happiness = clampf(e.happiness + 0.01f * dt, 0.f, 1.f);
            }
            if ((b && b->affection > 0.2f) || family ||
                dist2(c.x, c.y, o.x, o.y) < 16.f) friends++;
        }
        if (friends > 0) {
            e.loneliness = clampf(e.loneliness - (0.06f + friends * 0.02f) * dt, 0.f, 1.f);
            e.security = clampf(e.security + 0.02f * dt, 0.f, 1.f);
        }
        (void)kidsNear;
    }

    // --- PROTEÇÃO: se meu filho (ou criança próxima) está em perigo, fico corajoso ---
    if (!c.juvenile() && e.fear < 0.7f) {
        for (auto& o : creatures_) {
            if (!o.alive() || !o.juvenile()) continue;
            bool myChild = (std::find(c.childrenIds.begin(), c.childrenIds.end(), o.id) != c.childrenIds.end());
            bool nearKid = dist2(c.x, c.y, o.x, o.y) < 36.f;
            if (!myChild && !nearKid) continue;
            // criança com medo? adulto sente e protege
            if (o.em.fear > 0.35f) {
                float px2, py2;
                int tid2 = -1;
                if (findThreat(o, px2, py2, tid2)) {
                    // adulto vai na direção da ameaça (protege)
                    if (dist2(c.x, c.y, px2, py2) < 100.f) {
                        e.fear = clampf(e.fear - 0.1f * dt, 0.f, 1.f); // coragem pelo filho
                        e.anger = clampf(e.anger + 0.05f * dt, 0.f, 1.f);
                        // comportamento de proteção acontece no behavior
                        c.em.wrathTarget = tid2 >= 0 ? tid2 : -1;
                        break;
                    }
                }
            }
        }
    }

    // --- CONFORTO reduz solidão ---
    e.loneliness = clampf(e.loneliness - e.comfort * 0.06f * dt, 0.f, 1.f);

    // --- SOLIDÃO gera tristeza ---
    if (e.loneliness > 0.5f) {
        e.grief = clampf(e.grief + (e.loneliness - 0.5f) * 0.06f * dt, 0.f, 1.f);
        e.happiness = clampf(e.happiness - 0.04f * dt, 0.f, 1.f);
    }

    // --- SEGURANÇA reduz medo ---
    if (e.security > 0.4f) e.fear = clampf(e.fear - e.security * 0.08f * dt, 0.f, 1.f);
    e.security = clampf(e.security - 0.015f * dt, 0.f, 1.f); // decai devagar

    // --- CASA / COMUNIDADE dá conforto e segurança ---
    if (c.communityId >= 0) {
        if (Community* comm = community(c.communityId)) {
            float dHome = dist2(c.x, c.y, comm->x, comm->y);
            if (dHome < comm->territoryR * comm->territoryR) {
                if (comm->hasHut() || comm->hasBuilding(2)) {
                    e.comfort = clampf(e.comfort + 0.06f * dt, 0.f, 1.f);
                    e.security = clampf(e.security + 0.08f * dt, 0.f, 1.f);
                    e.happiness = clampf(e.happiness + 0.02f * dt, 0.f, 1.f);
                }
                if (comm->hasFire()) {
                    e.security = clampf(e.security + 0.06f * dt, 0.f, 1.f);
                    e.comfort = clampf(e.comfort + 0.04f * dt, 0.f, 1.f);
                }
            }
        }
    }

    // --- FILHOTES vivos dão felicidade ---
    int livingKids = 0;
    int kidsNear = 0;
    for (int kidId : c.childrenIds) {
        Creature* k = get(kidId);
        if (k && k->alive()) {
            livingKids++;
            if (dist2(c.x, c.y, k->x, k->y) < 40.f) kidsNear++;
        }
    }
    c.livingChildren = livingKids;
    if (livingKids > 0) {
        e.happiness = clampf(e.happiness + (0.06f + kidsNear * 0.04f) * dt, 0.f, 1.f);
        e.pride = clampf(e.pride + 0.03f * dt, 0.f, 1.f);
        e.satisfaction = clampf(e.satisfaction + 0.02f * dt, 0.f, 1.f);
        e.desireFamily = clampf(e.desireFamily + 0.02f * dt, 0.f, 1.f);
    }

    // --- DESEJO DE FAMÍLIA cresce com idade, conforto, apego ---
    if (!c.juvenile()) {
        float ageW = clampf((c.age - 1.5f) / 3.f, 0.f, 1.f);
        e.desireFamily = clampf(e.desireFamily +
            (0.008f + ageW * 0.015f + e.comfort * 0.01f + c.pers.attachment * 0.02f) * dt,
            0.f, 1.f);
    }

    // --- empatia: o bem-estar alheio contamina (positivo ou negativo) ---
    if (c.pers.empathy > 0.55f) {
        // olha o par/filhos: felicidade deles sobe a nossa um pouco
        if (mateNear && mate->em.happiness > 0.6f)
            e.happiness = clampf(e.happiness + 0.015f * dt, 0.f, 1.f);
        if (kidsNear > 0 && mate && mate->em.grief > 0.4f)
            e.grief = clampf(e.grief + 0.02f * dt, 0.f, 1.f);
    }

    // --- FELICIDADE base: otimismo + conforto + amor - solidão - medo ---
    float happyBase = 0.25f
        + c.pers.optimism * 0.30f
        + e.comfort * 0.20f
        + e.love * 0.12f
        + e.security * 0.10f
        + e.happiness * 0.15f
        - e.loneliness * 0.25f
        - e.fear * 0.15f
        - e.grief * 0.20f;
    e.happiness = clampf(lerpf(e.happiness, clampf(happyBase, 0.f, 1.f), clampf(0.4f * dt, 0.f, 1.f)), 0.f, 1.f);

    // curiosidade flutua com o dia (e medo a reprime)
    e.curiosity = clampf(0.25f + 0.25f * world_.lightLevel() + 0.1f * (1.f - e.fear)
                         + c.pers.thinker * 0.1f, 0.f, 1.f);

    // ---- REPUTAÇÃO: recuperação lenta (redenção) + família sente o extremo ----
    // rumo ao zero: quem faz o bem com o tempo é perdoado
    if (c.reputation < 0.f)
        c.reputation = clampf(c.reputation + 0.004f * dt * (0.5f + c.pers.empathy * 0.5f), -1.f, 1.f);
    else if (c.reputation > 0.f)
        c.reputation = clampf(c.reputation - 0.002f * dt, -1.f, 1.f); // orgulho esfria
    // reputação extrema: até a família desconfia (laço não some, mas o trust erode)
    if (c.reputation < -0.65f) {
        for (auto& o : creatures_) {
            if (o.id == c.id || !o.alive()) continue;
            bool family = (o.id == c.parentA || o.id == c.parentB ||
                           std::find(c.childrenIds.begin(), c.childrenIds.end(), o.id) != c.childrenIds.end() ||
                           (!c.surname.empty() && o.surname == c.surname) ||
                           o.em.mateId == c.id);
            if (!family) continue;
            Bond* b = c.bond(o.id);
            if (b) {
                // o afeto aguenta mais que a confiança
                b->trust = clampf(b->trust - 0.015f * dt, -1.f, 1.f);
            }
        }
    }
}

bool CreatureSim::findThreat(const Creature& c, float& px, float& py, int& threatId) const {
    threatId = -1;
    float bd = 1e9f;
    bool found = false;

    for (const auto& o : creatures_) {
        if (o.id == c.id || !o.alive()) continue;
        // CRIANÇA NUNCA É AMEAÇA — nem pra adulto, nem pra ninguém
        if (o.juvenile()) continue;
        if (o.em.anger < 0.4f && o.g.size < c.g.size * 1.15f) continue;
        float d = dist2(c.x, c.y, o.x, o.y);
        float r = (c.g.vision + o.g.size * 2.f);
        if (d < r * r && d < bd) {
            bd = d;
            px = o.x;
            py = o.y;
            threatId = o.id;
            found = true;
        }
    }
    if (eco_) {
        for (const auto& f : eco_->all()) {
            if (!f.alive() || !f.isPredator()) continue;
            // simbiose: predador que confia em pensantes não é ameaça
            if (f.trustSages > 0.45f) continue;
            float d = dist2(c.x, c.y, f.x, f.y);
            float r = c.g.vision + f.g.size * 3.f + 2.f;
            if (d < r * r && d < bd) {
                bd = d;
                px = f.x;
                py = f.y;
                threatId = -1;
                found = true;
            }
        }
    }
    return found;
}

void CreatureSim::scanCorpse(const Creature& c, float& bx, float& by, bool& found) const {
    found = false;
    float best = 1e9f;
    int r = (int)(c.g.vision + 2.f);
    int cx = (int)c.x, cy = (int)c.y;
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            if (!world_.inBounds(x, y)) continue;
            const TileData& t = world_.at(x, y);
            if (t.corpse == 0) continue;
            float d = dist2(c.x, c.y, x + 0.5f, y + 0.5f);
            if (d < best) {
                best = d;
                bx = x + 0.5f;
                by = y + 0.5f;
                found = true;
            }
        }
    }
}

void CreatureSim::scanShelter(const Creature& c, float& bx, float& by, bool& found) const {
    found = false;
    float best = 1e9f;
    int r = (int)(c.g.vision + 3.f);
    int cx = (int)c.x, cy = (int)c.y;
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            if (!world_.inBounds(x, y)) continue;
            Tile t = world_.at(x, y).type;
            float score = -1.f;
            // CAVERNA é o melhor abrigo do mundo
            if (world_.at(x, y).cave) score = 2.f;
            else if (t == Tile::Forest) score = 1.f;
            else if (t == Tile::Rock) score = 0.9f; // rocha comum
            else if (t == Tile::Soil) score = 0.25f;
            if (score < 0.f) continue;
            // abrigo cercado de gelo não adianta (montanha fria)
            if (t == Tile::Rock || t == Tile::Forest) {
                int iceNear = 0;
                for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -2; dx <= 2; dx++)
                        if (world_.inBounds(x + dx, y + dy) &&
                            world_.at(x + dx, y + dy).type == Tile::Ice) iceNear++;
                if (iceNear > 12) score *= 0.3f; // quase não vale a pena
            }
            float d = dist2(c.x, c.y, x + 0.5f, y + 0.5f) * (1.f - score * 0.3f);
            if (d < best) {
                best = d;
                bx = x + 0.5f;
                by = y + 0.5f;
                found = true;
            }
        }
    }
}

bool CreatureSim::tryEatCorpse(Creature& c, float dt) {
    float bx, by;
    bool found = false;
    scanCorpse(c, bx, by, found);
    if (!found) return false;
    float d = dist2(c.x, c.y, bx, by);
    if (d > 0.5f) {
        moveToward(c, bx, by, dt);
        return false;
    }
    TileData& t = world_.at((int)bx, (int)by);
    if (t.corpse > 0) {
        t.corpse--;
        c.energy = clampf(c.energy + 18.f, 0.f, 100.f);
        c.em.satisfaction = clampf(c.em.satisfaction + 0.3f, 0.f, 1.f);
        // DESCOBERTA: a primeira vez que come morte vira crença
        if (!c.findIdea("carne morta")) {
            c.formIdea(IdeaKind::Food, "carne morta ainda alimenta", 0.5f, currentDay_, 0.45f);
            addEvent(c.name + " provou carne morta. O mundo ficou menos vazio.", EvKind::Discovery);
        }
        // moral alta: come, mas sente
        if (c.morality > 0.55f && chance(0.4f)) {
            c.em.guilt = clampf(c.em.guilt + 0.15f, 0.f, 1.f);
            if (!c.findIdea("morte"))
                c.formIdea(IdeaKind::Self, "a morte sustenta a vida", 0.35f, currentDay_, 0.4f);
        }
        c.state = CState::Eat;
        c.stateT = 0.8f;
        return true;
    }
    return false;
}

// CAÇADA PRIMITIVA — o pensante que mata
// Requisitos: fome real (energia < 14), ousadia alta, bicho pequeno por perto.
// Raro como na vida real: só os desesperados e corajosos caçam.
// Carne fresca dá MUITA energia, mas precisa de repouso e deixa culpa.
bool CreatureSim::tryHuntFauna(Creature& c, float dt, unsigned day) {
    if (!eco_) return false;
    if (c.energy >= 14.f) return false;           // só com fome de verdade
    if (c.stateT > 0.f) return false;             // descansando da última refeição
    // coragem: ousadia + pouca moral (moral alta hesita) + reputação baixa ajuda (nada a perder)
    float courage = c.pers.boldness * 0.6f
                  + (1.f - c.morality) * 0.25f
                  + (c.reputation < -0.3f ? 0.2f : 0.f)
                  + c.g.aggression * 0.15f;
    if (!chance(courage * 0.04f * dt * 30.f)) return false;

    // procura presa pequena e pacífica (não ataca predador)
    float best = 1e9f;
    Fauna* prey = nullptr;
    float r = (float)(c.g.vision + 2.f);
    for (auto& f : eco_->all()) {
        if (!f.alive()) continue;
        if (f.isPredator()) continue;             // não caça quem caça
        if (f.g.size > c.g.size * 0.85f) continue; // só bicho menor
        if (f.trustSages > 0.5f) continue;         // simbiose: não mata quem confia
        float d = dist2(c.x, c.y, f.x, f.y);
        if (d < r * r && d < best) { best = d; prey = &f; }
    }
    if (!prey) return false;

    c.state = CState::Fight;
    moveToward(c, prey->x, prey->y, dt, 1.15f);
    if (dist2(c.x, c.y, prey->x, prey->y) < 0.7f) {
        // ataque: força + agressividade vs fuga da presa
        float kill = 0.35f + c.body.strength * 0.3f + c.g.aggression * 0.25f
                   - prey->g.speed * 0.12f - prey->g.size * 0.1f;
        if (chance(clampf(kill, 0.1f, 0.85f))) {
            // MATOU — a descoberta do caçador primitivo
            eco_->dropCorpse(prey->x, prey->y, prey->g.size * 0.5f);
            prey->energy = 0.f;
            // carne fresca: energia alta, mas precisa repousar (digestão de carne)
            c.energy = clampf(c.energy + 40.f, 0.f, 100.f);
            c.em.satisfaction = 1.f;
            c.stateT = 20.f; // repouso prolongado após caça
            c.state = CState::Eat;
            learnPleasure(c, 0.25f); // a vitória dá prazer
            // DESCOBERTA: caçar funciona — ideia fundadora
            if (!c.findIdea("caça")) {
                c.formIdea(IdeaKind::Food, "caçar dá força que a planta não dá", 0.55f, day, 0.35f);
                addEvent(c.name + " caçou. Nasceu o caçador — carne fresca é outra coisa.", EvKind::Discovery);
                c.addChronicle(day, "caçou um animal pela primeira vez");
            }
            // CONSCIÊNCIA: quem tem moral sente o peso
            if (c.morality > 0.4f) {
                c.em.guilt = clampf(c.em.guilt + 0.35f * c.morality, 0.f, 1.f);
                c.formIdea(IdeaKind::Self, "matar doeu em mim tambem", 0.4f, day, 0.55f);
                if (chance(0.3f))
                    addEvent(c.name + " comeu, mas algo dentro dele pesou.", EvKind::Health);
            } else {
                // sem moral: orgulho do predador
                c.em.pride = clampf(c.em.pride + 0.3f, 0.f, 1.f);
                c.formIdea(IdeaKind::Self, "os fortes comem primeiro", 0.5f, day, 0.15f);
            }
            // reputação: matar É visto — só se tem testemunha por perto
            {
                int wit = 0;
                for (auto& w : creatures_) {
                    if (w.id == c.id || !w.alive()) continue;
                    if (dist2(c.x, c.y, w.x, w.y) > 25.f) continue;
                    wit++;
                    Bond& wb = w.ensureBond(c.id, day);
                    wb.trust = clampf(wb.trust - 0.15f, -1.f, 1.f);
                    if (w.morality > 0.6f) w.em.fear = clampf(w.em.fear + 0.15f, 0.f, 1.f);
                }
                if (wit > 0)
                    changeReputation(c, -0.1f, day, "caçou na frente dos outros");
                else
                    changeReputation(c, -0.02f, day, "caçada solitaria");
            }
            return true;
        } else {
            // errou: presa foge, ele fica com raiva da própria falha
            c.em.anger = clampf(c.em.anger + 0.15f, 0.f, 1.f);
            c.energy -= 2.f;
        }
    }
    return false;
}

void CreatureSim::evolveFromAnimal(float x, float y, float speed, float size, float vision,
                                   float metabolism, float lifespan, int parentA, int parentB) {
    Creature c;
    c.id = nextId_++;
    c.name = makeName();
    c.x = x + frand(-0.3f, 0.3f);
    c.y = y + frand(-0.3f, 0.3f);
    snapPenToLand(world_, c.x, c.y); // nunca nasce no mar
    c.male = chance(0.5f);
    c.energy = 80.f;
    c.hydration = 85.f;
    c.age = 0.f;
    // cérebro maior: visão e learning boosts, metabolismo um pouco puxado
    c.g.speed = clampf(speed * 0.95f, 0.5f, 1.8f);
    c.g.metabolism = clampf(metabolism * 1.05f, 0.5f, 1.6f);
    c.g.size = clampf(size * 0.95f, 0.55f, 1.5f);
    c.g.vision = clampf(vision + 3.f, 6.f, 13.f);
    c.g.fertility = clampf(gauss(1.f, 0.12f), 0.4f, 1.6f);
    c.g.robustness = clampf(gauss(0.55f, 0.12f), 0.15f, 0.9f);
    c.g.lifespan = clampf(lifespan + 3.f, 7.f, 15.f);
    c.g.aggression = clampf(gauss(0.25f, 0.1f), 0.05f, 0.85f);
    // primeiro pensante: bem primitivo
    c.g.intelligence = clampf(gauss(0.3f, 0.08f), 0.1f, 0.5f);
    c.culturalLevel = 0;
    c.tool = 0;
    // pensantes nascem com curiosidade
    c.k.plant_value = clampf(gauss(0.7f, 0.08f), 0.4f, 0.95f);
    c.k.discrimination = clampf(gauss(0.2f, 0.05f), 0.f, 0.5f);
    c.parentA = parentA;
    c.parentB = parentB;
    c.state = CState::Wander;
    c.stateT = 1.f;
    c.bornDay = currentDay_;
    c.addChronicle(currentDay_, "despertou pensante");
    creatures_.push_back(c);
    births_++;
    addEvent("Um animal se ergueu: " + c.name + " nasceu pensante.", EvKind::Birth);
}

bool CreatureSim::canSenseToxic(const Creature& c) const {
    return c.k.discrimination > frand(0.55f, 1.05f);
}

void CreatureSim::scanFood(const Creature& c, float& bx, float& by, float& best) const {
    best = -1e9f;
    bx = by = 0.f;
    // MEMÓRIA: se lembra de um lugar bom e ainda tem planta, vai direto
    if (c.memFoodT > 0.f && c.memFoodX >= 0.f) {
        int mx = (int)c.memFoodX, my = (int)c.memFoodY;
        if (world_.inBounds(mx, my)) {
            const Plant& p = world_.at(mx, my).plant;
            if (p.kind != PlantKind::None && p.stage >= 1) {
                bx = c.memFoodX + 0.5f;
                by = c.memFoodY + 0.5f;
                best = 1.5f; // score alto: memória vale muito
                return;
            }
        }
    }
    int r = (int)c.g.vision;
    int cx = (int)c.x, cy = (int)c.y;
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            if (!world_.inBounds(x, y)) continue;
            const TileData& t = world_.at(x, y);
            if (t.plant.kind == PlantKind::None) continue;
            if (t.plant.stage < 1) continue;

            float score = c.k.plant_value + frand(-0.12f, 0.12f);
            if (t.plant.kind == PlantKind::Toxic) {
                bool sensed = c.k.discrimination > 0.35f;
                if (sensed || canSenseToxic(c)) score -= c.k.toxic_aversion + 0.45f;
                else score -= c.k.toxic_aversion * 0.4f;
            } else {
                score += 0.1f * c.k.discrimination;
            }
            if (c.energy < 22.f) score += 0.55f; // desespero come qualquer coisa
            if (t.plant.stage == 2) score += 0.15f;
            float d = std::sqrt(dist2(c.x, c.y, x + 0.5f, y + 0.5f));
            score -= d * 0.035f;
            if (score > best) {
                best = score;
                bx = x + 0.5f;
                by = y + 0.5f;
            }
        }
    }
}

void CreatureSim::scanMate(const Creature& c, float& bx, float& by, bool& found) const {
    found = false;
    float bestVal = -1e9f;
    bx = by = 0.f;
    int r = (int)(c.g.vision + 3.f);
    // monogâmico: se já tem companheiro vivo, só "procura" ele
    if (c.em.mateId >= 0) {
        for (const auto& m : creatures_) {
            if (m.id == c.em.mateId && m.alive()) {
                bx = m.x; by = m.y; found = true;
                return;
            }
        }
    }
    for (const auto& o : creatures_) {
        if (o.id == c.id || !o.alive()) continue;
        if (o.male == c.male) continue;
        if (o.juvenile() || o.mateCooldown > 0.f) continue;
        if (o.energy < 50.f) continue;
        // já tem par vivo: não rouba
        if (o.em.mateId >= 0) {
            bool taken = false;
            for (const auto& om : creatures_) {
                if (om.id == o.em.mateId && om.alive()) { taken = true; break; }
            }
            if (taken) continue;
        }
        float d = dist2(c.x, c.y, o.x, o.y);
        if (d > (float)(r * r)) continue;

        // cada um tem gosto próprio — a fêmea avalia o macho
        float atr = c.attractionTo(o);
        if (const Bond* b = c.bond(o.id)) atr += b->affection * 0.4f + b->trust * 0.25f;
        if (atr < 0.4f) continue; // não é atraente o bastante

        float val = atr - d * 0.02f; // perto facilita, mas beleza/importância pesa
        if (val > bestVal) {
            bestVal = val;
            bx = o.x;
            by = o.y;
            found = true;
        }
    }
}

void CreatureSim::scanWater(const Creature& c, float& bx, float& by, bool& found) const {
    found = false;
    float best = 1e9f;
    int r = (int)(c.g.vision + 1.f);
    int cx = (int)c.x, cy = (int)c.y;
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            if (!world_.inBounds(x, y)) continue;
            Tile t = world_.at(x, y).type;
            if (t != Tile::Water && t != Tile::DeepWater) continue;
            float d = dist2(c.x, c.y, x + 0.5f, y + 0.5f);
            if (d < best) {
                best = d;
                bx = x + 0.5f;
                by = y + 0.5f;
                found = true;
            }
        }
    }
}

void CreatureSim::moveToward(Creature& c, float tx, float ty, float dt, float mul) {
    float dx = tx - c.x;
    float dy = ty - c.y;
    float d = std::sqrt(dx * dx + dy * dy);
    if (d < 0.05f) return;
    dx /= d;
    dy /= d;

    float speed = c.g.speed * 2.4f * mul;
    if (c.juvenile()) speed *= 1.05f;

    int txi = (int)c.x, tyi = (int)c.y;
    if (world_.inBounds(txi, tyi)) {
        Tile tt = world_.at(txi, tyi).type;
        if (tt == Tile::Rock || tt == Tile::Ice) speed *= 0.55f;
        if (tt == Tile::Sand) speed *= 0.8f;
    }

    float nx = c.x + dx * speed * dt;
    float ny = c.y + dy * speed * dt;
    if (world_.walkable(nx, c.y)) c.x = nx;
    if (world_.walkable(c.x, ny)) c.y = ny;

    // planta espalha semente pelo animal que caminha (zoochoria)
    if (c.seedLoad > 0.f && chance(c.seedSpreadGene * 0.04f)) {
        int sx = (int)c.x, sy = (int)c.y;
        if (world_.inBounds(sx, sy)) {
            TileData& t = world_.at(sx, sy);
            if ((t.type == Tile::Grass || t.type == Tile::Forest || t.type == Tile::Soil) &&
                t.plant.kind == PlantKind::None && t.moisture > 0.18f) {
                PlantKind k = c.seedKind == 2 ? PlantKind::Toxic : PlantKind::Fruit;
                t.plant.kind = k;
                t.plant.stage = 0;
                t.plant.gene.animal_spread = c.seedSpreadGene;
                t.plant.gene.toxicity = k == PlantKind::Toxic ? clampf(gauss(0.5f, 0.15f), 0.2f, 0.9f)
                                                              : clampf(gauss(0.25f, 0.1f), 0.f, 0.6f);
                t.plant.gene.expansion = clampf(gauss(0.35f, 0.12f), 0.05f, 0.9f);
                c.seedLoad -= 1.f;
            }
        }
    }
}

void CreatureSim::wander(Creature& c, float dt) {
    // personalidade: ousado anda mais, preguiçoso menos, nômade vai longe
    float bold = 0.5f + c.pers.boldness * 0.5f;
    float lazy = 1.f - c.pers.laziness * 0.55f;
    float range = 3.f + c.pers.wanderlust * 8.f;
    c.stateT -= dt * lazy;
    if (c.stateT <= 0.f) {
        c.stateT = frand(1.5f, 4.f) / bold;
        // NUNCA escolhe gelo como destino
        for (int tries = 0; tries < 6; tries++) {
            c.targetX = clampf(c.x + gauss(0.f, range), 1.f, (float)world_.width() - 2.f);
            c.targetY = clampf(c.y + gauss(0.f, range), 1.f, (float)world_.height() - 2.f);
            int ttx = (int)c.targetX, tty = (int)c.targetY;
            if (world_.inBounds(ttx, tty) && world_.at(ttx, tty).type != Tile::Ice) break;
            if (tries == 5) {
                // nenhuma alternativa boa: fica onde está
                c.targetX = c.x;
                c.targetY = c.y;
            }
        }
    }
    moveToward(c, c.targetX, c.targetY, dt, 0.55f * (0.7f + bold * 0.5f));
}

bool CreatureSim::tryEat(Creature& c, float dt) {
    int tx = (int)c.x, ty = (int)c.y;
    if (!world_.inBounds(tx, ty)) return false;
    TileData& t = world_.at(tx, ty);
    if (t.plant.kind == PlantKind::None || t.plant.stage < 2) {
        float bx, by, best;
        scanFood(c, bx, by, best);
        if (best > -1e8f) {
            moveToward(c, bx, by, dt);
            if (dist2(c.x, c.y, bx, by) < 0.35f) {
                c.state = CState::Eat;
                c.stateT = 1.2f;
            }
        } else {
            c.state = CState::Wander;
        }
        return false;
    }

    // na planta madura
    PlantKind kind = t.plant.kind;
    float geneYield = t.plant.gene.yield;
    float geneTox = t.plant.gene.toxicity;
    float geneCure = t.plant.gene.cure;
    float geneSpread = t.plant.gene.animal_spread;
    t.plant = Plant{};
    if (kind == PlantKind::Fruit || geneCure > 0.3f) {
        float base = (22.f + 16.f * c.k.plant_value) * geneYield;
        c.energy = clampf(c.energy + base, 0.f, 100.f);
        // fruta suculenta dá água também (nem todo mundo precisa ir ao rio)
        c.hydration = clampf(c.hydration + 8.f * geneYield, 0.f, 100.f);
        // sementes no corpo: a planta usa o animal como carro-chefe
        c.seedLoad = clampf(c.seedLoad + 1.f + c.pers.greed * 2.f, 0.f, 10.f);
        c.seedKind = (kind == PlantKind::Fruit) ? 1 : 2;
        c.seedSpreadGene = geneSpread;
        if (c.k.tool_skill > 0.3f) c.energy = clampf(c.energy + 4.f * c.k.tool_skill, 0.f, 100.f);
        if (c.k.fire_knowledge > 0.4f) c.energy = clampf(c.energy + 6.f * c.k.fire_knowledge, 0.f, 100.f);
        c.k.plant_value = clampf(c.k.plant_value + 0.07f, 0.f, 1.f);
        c.k.discrimination = clampf(c.k.discrimination + 0.02f, 0.f, 1.f);
        // MEMÓRIA: lembra onde achou comida boa (inteligência)
        c.memFoodX = (float)tx;
        c.memFoodY = (float)ty;
        c.memFoodT = 30.f + c.g.intelligence * 40.f; // lembra dias
        // planta-cura: aprende antídoto e se cura
        if (geneCure > 0.25f) {
            c.k.antidote = clampf(c.k.antidote + geneCure * 0.25f, 0.f, 1.f);
            if (c.sickT > 0.f) {
            c.sickT = clampf(c.sickT - geneCure * 3.f, 0.f, 99.f);
                c.energy = clampf(c.energy + 10.f * geneCure, 0.f, 100.f);
                addEvent(c.name + " achou o antídoto e se curou.", EvKind::Health);
                c.formIdea(IdeaKind::Plant, "existe cura para o veneno", 0.55f + geneCure * 0.3f, currentDay_, 0.85f);
                c.formIdea(IdeaKind::Mystery, "as plantas escondem respostas", 0.3f, currentDay_, 0.6f);
                c.state = CState::Wander;
            }
        }
        c.em.satisfaction = clampf(c.em.satisfaction + 0.35f, 0.f, 1.f);
        c.em.happiness = clampf(c.em.happiness + 0.08f, 0.f, 1.f);
        // comer é vivo: reforça o instinto de buscar comida boa
        if (!c.findIdea("comer e viver"))
            c.formIdea(IdeaKind::Food, "comer bem e viver bem", 0.35f, currentDay_, 0.6f);
        if (c.age > 2.f && c.k.discrimination > 0.3f && c.k.tool_skill < 1.f && chance(0.012f)) {
            c.k.tool_skill = clampf(c.k.tool_skill + 0.15f, 0.f, 1.f);
            learnPleasure(c, 0.2f); // aprender é prazeroso
            addEvent(c.name + " aprendeu a usar um galho como ferramenta.", EvKind::Discovery);
            c.formIdea(IdeaKind::Self, "minhas maos podem mudar o mundo", 0.5f, currentDay_, 0.7f);
            c.formIdea(IdeaKind::World, "os galhos servem para algo", 0.45f, currentDay_, 0.55f);
        }
        if (c.k.tool_skill > 0.4f && c.k.fire_knowledge < 1.f && chance(0.006f)) {
            c.k.fire_knowledge = clampf(c.k.fire_knowledge + 0.2f, 0.f, 1.f);
            learnPleasure(c, 0.25f);
            addEvent(c.name + " trouxe luz para a noite: fogo.", EvKind::Discovery);
            c.addChronicle(currentDay_, "acendeu o primeiro fogo");
            c.formIdea(IdeaKind::World, "a luz afasta o medo", 0.6f, currentDay_, 0.8f);
            c.formIdea(IdeaKind::Self, "eu posso trazer calor", 0.5f, currentDay_, 0.75f);
            c.formIdea(IdeaKind::Community, "o fogo junta quem tem frio", 0.4f, currentDay_, 0.8f);
        }
        c.state = CState::Wander;
        if (chance(0.08f)) addEvent(c.name + " encontrou comida boa.", EvKind::Health);
    } else {
        float dmg = 24.f * (1.45f - c.g.robustness * 0.9f) * (0.6f + geneTox);
        // antídoto no corpo ameniza o veneno
        dmg *= (1.f - c.k.antidote * 0.45f);
        c.energy -= dmg;
        c.sickT = clampf(c.sickT + 3.f + geneTox * 3.f, 0.f, 20.f);
        c.k.toxic_aversion = clampf(c.k.toxic_aversion + 0.3f, 0.f, 1.f);
        c.k.discrimination = clampf(c.k.discrimination + 0.22f, 0.f, 1.f);
        c.k.plant_value = clampf(c.k.plant_value - 0.08f, 0.05f, 1.f);
        c.state = CState::Sick;
        c.stateT = 4.f;
        // a dor vira crença
        c.formIdea(IdeaKind::Plant, "nem tudo que alimenta e seguro", 0.5f + geneTox * 0.3f, currentDay_, 0.5f);
        if (c.k.antidote > 0.2f)
            c.formIdea(IdeaKind::Plant, "existe cura para o veneno", 0.3f, currentDay_, 0.8f);
        if (c.energy > 0.f) {
            addEvent(c.name + " errou e aprendeu com a dor.", EvKind::Health);
        } else {
            addEvent(c.name + " nao aprendeu a tempo.", EvKind::Death);
        }
    }
    return true;
}

bool CreatureSim::tryDrink(Creature& c, float dt) {
    float bx, by, foundf;
    bool found = false;
    scanWater(c, bx, by, found);
    (void)foundf;
    if (!found) {
        // sem água por perto: vagueia, não entra em loop
        c.state = CState::Wander;
        wander(c, dt);
        return false;
    }
    float d = dist2(c.x, c.y, bx, by);
    if (d > 1.1f) {
        moveToward(c, bx, by, dt);
        return false;
    }
    // bebe rápido — hidrata de verdade
    c.hydration = clampf(c.hydration + 55.f * dt, 0.f, 100.f);
    c.state = CState::Drink;
    if (c.hydration > 80.f) {
        c.state = CState::Rest;
        c.stateT = 2.f; // descansa um pouco depois de beber
        c.em.satisfaction = clampf(c.em.satisfaction + 0.2f, 0.f, 1.f);
        c.em.comfort = clampf(c.em.comfort + 0.1f, 0.f, 1.f);
        if (!c.findIdea("sede"))
            c.formIdea(IdeaKind::World, "a sede lembra que somos vivos", 0.3f, currentDay_, 0.5f);
    }
    return true;
}

void CreatureSim::tryReproduce(Creature& a, std::vector<Creature>& out) {
    if (a.juvenile() || a.mateCooldown > 0.f || a.energy < 55.f) return;
    // capacidade suave: casal com muitos filhos ou mundo cheio freia
    if (a.livingChildren >= 5) return;
    int pop = aliveCount();
    if (pop > 30 && chance(0.5f)) return;
    if (pop > 60) return;
    // MONOGAMIA: se já tem companheiro vivo, só acasla com ele
    Creature* currentMate = a.em.mateId >= 0 ? get(a.em.mateId) : nullptr;
    bool hasLivingMate = currentMate && currentMate->alive();
    for (auto& b : creatures_) {
        if (b.id == a.id || !b.alive()) continue;
        if (b.male == a.male) continue;
        if (b.juvenile() || b.mateCooldown > 0.f || b.energy < 55.f) continue;
        if (b.livingChildren >= 5) continue;
        // fiel ao par: não acasla com outro se tem companheiro vivo
        if (hasLivingMate && b.id != a.em.mateId) continue;
        Creature* bMate = b.em.mateId >= 0 ? get(b.em.mateId) : nullptr;
        if (bMate && bMate->alive() && bMate->id != a.id) continue;
        if (dist2(a.x, a.y, b.x, b.y) > 1.44f) continue;
        // casal feliz quer mais filhotes; casal infeliz espera
        float p = 0.12f * a.g.fertility * b.g.fertility;
        p *= 0.5f + a.em.happiness * 0.5f;
        p *= 0.6f + a.em.love * 0.4f;
        if (!chance(p)) continue;

        a.energy -= 25.f;
        b.energy -= 25.f;
        a.mateCooldown = 22.f;
        b.mateCooldown = 22.f;
        a.children++;
        b.children++;
        a.totalChildrenBorn++;
        b.totalChildrenBorn++;
        // acaslar dá prazer e confirma o par
        a.em.mateId = b.id;
        b.em.mateId = a.id;
        a.em.love = clampf(a.em.love + 0.35f, 0.f, 1.f);
        b.em.love = clampf(b.em.love + 0.35f, 0.f, 1.f);
        a.em.satisfaction = clampf(a.em.satisfaction + 0.4f, 0.f, 1.f);
        b.em.satisfaction = clampf(b.em.satisfaction + 0.4f, 0.f, 1.f);
        a.em.loneliness = 0.f;
        b.em.loneliness = 0.f;
        a.em.security = clampf(a.em.security + 0.15f, 0.f, 1.f);
        b.em.security = clampf(b.em.security + 0.15f, 0.f, 1.f);
        a.em.desireFamily = clampf(a.em.desireFamily + 0.2f, 0.f, 1.f);
        b.em.desireFamily = clampf(b.em.desireFamily + 0.2f, 0.f, 1.f);
        a.addChronicle(currentDay_, "acasalou com " + b.fullName());
        b.addChronicle(currentDay_, "acasalou com " + a.fullName());
        // ideia de família descoberta na prática
        a.formIdea(IdeaKind::Love, "criar juntos e a maior alegria", 0.5f, currentDay_, 0.85f);
        b.formIdea(IdeaKind::Love, "criar juntos e a maior alegria", 0.5f, currentDay_, 0.85f);
        a.formIdea(IdeaKind::Community, "familia e o primeiro lar", 0.45f, currentDay_, 0.85f);
        b.formIdea(IdeaKind::Community, "familia e o primeiro lar", 0.45f, currentDay_, 0.85f);
        {
            Bond& ba = a.ensureBond(b.id, currentDay_);
            ba.trust = clampf(ba.trust + 0.3f, -1.f, 1.f);
            ba.affection = clampf(ba.affection + 0.4f, -1.f, 1.f);
            Bond& bb = b.ensureBond(a.id, currentDay_);
            bb.trust = clampf(bb.trust + 0.3f, -1.f, 1.f);
            bb.affection = clampf(bb.affection + 0.4f, -1.f, 1.f);
        }
        Creature child = makeChild(a, b);
        // ---- FAMÍLIA ----
        // sobrenome do pai (macho)
        const Creature& dad = a.male ? a : b;
        const Creature& mom = a.male ? b : a;
        child.surname = dad.surname.empty() ? dad.name : dad.surname;
        a.childrenIds.push_back(child.id);
        b.childrenIds.push_back(child.id);
        // laço familiar forte logo ao nascer
        Bond& ba = a.ensureBond(child.id, currentDay_);
        ba.trust = 1.f;
        ba.affection = clampf(ba.affection + 0.8f, -1.f, 1.f);
        Bond& bb = b.ensureBond(child.id, currentDay_);
        bb.trust = 1.f;
        bb.affection = clampf(bb.affection + 0.8f, -1.f, 1.f);
        Bond& bc = child.ensureBond(a.id, currentDay_);
        bc.trust = 0.9f;
        bc.affection = 0.9f;
        Bond& bd = child.ensureBond(b.id, currentDay_);
        bd.trust = 0.9f;
        bd.affection = 0.9f;
        // registra o que os pais passaram (visível no filho)
        auto addT = [&](Creature& ch, const Creature& parent, const char* lesson, float v) {
            TeachingRecord t;
            t.teacherId = parent.id;
            t.teacherName = parent.name;
            t.lesson = lesson;
            t.value = v;
            ch.teachings.push_back(t);
        };
        if (a.k.plant_value > 0.4f || b.k.plant_value > 0.4f)
            addT(child, a.k.plant_value > b.k.plant_value ? a : b, "amar a fruta", std::max(a.k.plant_value, b.k.plant_value));
        if (a.k.toxic_aversion > 0.2f || b.k.toxic_aversion > 0.2f)
            addT(child, a.k.toxic_aversion > b.k.toxic_aversion ? a : b, "desconfiar do veneno", std::max(a.k.toxic_aversion, b.k.toxic_aversion));
        if (a.k.antidote > 0.15f || b.k.antidote > 0.15f)
            addT(child, a.k.antidote > b.k.antidote ? a : b, "onde nasce o antídoto", std::max(a.k.antidote, b.k.antidote));
        if (a.k.tool_skill > 0.2f || b.k.tool_skill > 0.2f)
            addT(child, a.k.tool_skill > b.k.tool_skill ? a : b, "usar ferramentas", std::max(a.k.tool_skill, b.k.tool_skill));
        if (a.k.fire_knowledge > 0.15f || b.k.fire_knowledge > 0.15f)
            addT(child, a.k.fire_knowledge > b.k.fire_knowledge ? a : b, "o fogo", std::max(a.k.fire_knowledge, b.k.fire_knowledge));
        // mãe ensina mais (maternidade cultural)
        addT(child, mom, "o caminho das plantas", mom.k.plant_value);
        addT(child, dad, "como se defender", dad.g.aggression + dad.k.predator_knowledge * 0.5f);
        addEvent(child.fullName() + " nasceu de " + mom.fullName() + " e " + dad.fullName() + " — amor virou vida.", EvKind::Birth);
        a.addChronicle(currentDay_, "nasceu o filho " + child.fullName());
        b.addChronicle(currentDay_, "nasceu o filho " + child.fullName());
        out.push_back(child);
        return;
    }
}

Creature CreatureSim::makeChild(const Creature& a, const Creature& b) {
    Creature c;
    c.id = nextId_++;
    c.name = makeName();
    c.x = (a.x + b.x) * 0.5f + frand(-0.3f, 0.3f);
    c.y = (a.y + b.y) * 0.5f + frand(-0.3f, 0.3f);
    snapPenToLand(world_, c.x, c.y); // filhote nunca nasce no mar
    c.male = chance(0.5f);
    c.energy = 55.f;
    c.hydration = 60.f;
    c.age = 0.f;
    c.g.speed = clampf(gauss((a.g.speed + b.g.speed) * 0.5f, 0.12f), 0.35f, 2.2f);
    c.g.metabolism = clampf(gauss((a.g.metabolism + b.g.metabolism) * 0.5f, 0.1f), 0.45f, 1.7f);
    c.g.size = clampf(gauss((a.g.size + b.g.size) * 0.5f, 0.08f), 0.55f, 1.6f);
    c.g.vision = clampf(gauss((a.g.vision + b.g.vision) * 0.5f, 1.f), 3.5f, 13.f);
    c.g.fertility = clampf(gauss((a.g.fertility + b.g.fertility) * 0.5f, 0.12f), 0.35f, 1.7f);
    c.g.robustness = clampf(gauss((a.g.robustness + b.g.robustness) * 0.5f, 0.12f), 0.05f, 0.95f);
    c.g.lifespan = clampf(gauss((a.g.lifespan + b.g.lifespan) * 0.5f, 1.2f), 5.f, 15.f);
    c.g.aggression = clampf(gauss((a.g.aggression + b.g.aggression) * 0.5f, 0.08f), 0.05f, 0.95f);
    c.g.intelligence = clampf(gauss((a.g.intelligence + b.g.intelligence) * 0.55f, 0.06f), 0.05f, 1.f);
    c.morality = clampf(gauss((a.morality + b.morality) * 0.5f, 0.12f), 0.05f, 0.95f);
    // corpo e mente: herança + mutação
    c.body.strength = clampf(gauss((a.body.strength + b.body.strength) * 0.5f, 0.12f), 0.05f, 1.f);
    c.body.endurance = clampf(gauss((a.body.endurance + b.body.endurance) * 0.5f, 0.12f), 0.05f, 1.f);
    c.body.agility = clampf(gauss((a.body.agility + b.body.agility) * 0.5f, 0.12f), 0.05f, 1.f);
    c.body.health = 1.f;
    c.mind.wisdom = clampf((a.mind.wisdom + b.mind.wisdom) * 0.5f, 0.f, 1.f);
    c.mind.memory = clampf(gauss((a.mind.memory + b.mind.memory) * 0.5f, 0.1f), 0.05f, 1.f);
    c.mind.creativity = clampf(gauss((a.mind.creativity + b.mind.creativity) * 0.5f, 0.15f), 0.f, 1.f);
    c.mind.willpower = clampf(gauss((a.mind.willpower + b.mind.willpower) * 0.5f, 0.12f), 0.05f, 1.f);
    // nível cultural: filho herda o dos pais + evolução lenta
    c.culturalLevel = std::max(a.culturalLevel, b.culturalLevel);
    if (chance(0.15f)) c.culturalLevel = clampf(c.culturalLevel + 1, 0, 10);
    float eraBonus = std::min(c.culturalLevel * 0.04f, 0.4f);
    c.g.intelligence = clampf(c.g.intelligence + eraBonus, 0.05f, 1.f);
    // ferramenta herdada (só se já tem nível cultural mínimo)
    if ((a.tool > 0 || b.tool > 0) && c.culturalLevel >= 1) {
        int best = std::max(a.tool, b.tool);
        if (chance(0.5f)) c.tool = best;
    }
    // escrita: herda ideias registradas em pedra dos pais
    for (auto& idea : a.writtenIdeas) {
        if (chance(0.3f) && !c.findIdea(idea.text)) {
            c.formIdea(idea.kind, idea.text, idea.strength * 0.7f, currentDay_, idea.morality);
            if (Idea* mine = c.findIdea(idea.text)) mine->selfTaught = false;
        }
    }
    for (auto& idea : b.writtenIdeas) {
        if (chance(0.3f) && !c.findIdea(idea.text)) {
            c.formIdea(idea.kind, idea.text, idea.strength * 0.7f, currentDay_, idea.morality);
            if (Idea* mine = c.findIdea(idea.text)) mine->selfTaught = false;
        }
    }
    // personalidade: herança + acaso (ninguém é cópia)
    auto mix = [](float x, float y, float noise) {
        return clampf((x + y) * 0.5f + gauss(0.f, noise), 0.f, 1.f);
    };
    c.pers.sexuality = mix(a.pers.sexuality, b.pers.sexuality, 0.15f);
    c.pers.boldness = mix(a.pers.boldness, b.pers.boldness, 0.15f);
    c.pers.thinker = mix(a.pers.thinker, b.pers.thinker, 0.14f);
    c.pers.greed = mix(a.pers.greed, b.pers.greed, 0.12f);
    c.pers.laziness = mix(a.pers.laziness, b.pers.laziness, 0.12f);
    c.pers.empathy = mix(a.pers.empathy, b.pers.empathy, 0.13f);
    c.pers.wanderlust = mix(a.pers.wanderlust, b.pers.wanderlust, 0.15f);
    c.pers.attachment = mix(a.pers.attachment, b.pers.attachment, 0.16f);
    c.pers.optimism = mix(a.pers.optimism, b.pers.optimism, 0.14f);
    // filhos de casal amoroso já nascem com certo senso de lar
    if (a.em.mateId == b.id && a.em.love > 0.4f) {
        c.pers.attachment = clampf(c.pers.attachment + 0.1f, 0.f, 1.f);
    }
    c.em.happiness = clampf(0.35f + c.pers.optimism * 0.3f, 0.f, 1.f);
    c.em.security = 0.35f;
    // preferências: meio dos pais + mistério (nem filho é igual ao pai)
    c.prefs.likeStrength = clampf(gauss((a.prefs.likeStrength + b.prefs.likeStrength) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeSmarts = clampf(gauss((a.prefs.likeSmarts + b.prefs.likeSmarts) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeSkill = clampf(gauss((a.prefs.likeSkill + b.prefs.likeSkill) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeSize = clampf(gauss((a.prefs.likeSize + b.prefs.likeSize) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeCalm = clampf(gauss((a.prefs.likeCalm + b.prefs.likeCalm) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeYoung = clampf(gauss((a.prefs.likeYoung + b.prefs.likeYoung) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeHair = clampf(gauss((a.prefs.likeHair + b.prefs.likeHair) * 0.5f, 0.2f), 0.f, 1.f);
    c.prefs.likeVoice = clampf(gauss((a.prefs.likeVoice + b.prefs.likeVoice) * 0.5f, 0.2f), 0.f, 1.f);
    c.prefs.likeGenerosity = clampf(gauss((a.prefs.likeGenerosity + b.prefs.likeGenerosity) * 0.5f, 0.2f), 0.f, 1.f);
    c.prefs.likeCourage = clampf(gauss((a.prefs.likeCourage + b.prefs.likeCourage) * 0.5f, 0.2f), 0.f, 1.f);
    c.prefs.likeLoyalty = clampf(gauss((a.prefs.likeLoyalty + b.prefs.likeLoyalty) * 0.5f, 0.2f), 0.f, 1.f);
    c.prefs.likeHumor = clampf(gauss((a.prefs.likeHumor + b.prefs.likeHumor) * 0.5f, 0.2f), 0.f, 1.f);
    c.prefs.likeWealth = clampf(gauss((a.prefs.likeWealth + b.prefs.likeWealth) * 0.5f, 0.2f), 0.f, 1.f);

    // heranca de conhecimento (ensino + instinto)
    c.k.plant_value = clampf((a.k.plant_value + b.k.plant_value) * 0.5f + gauss(0.f, 0.05f), 0.f, 1.f);
    c.k.toxic_aversion = clampf((a.k.toxic_aversion + b.k.toxic_aversion) * 0.45f + gauss(0.f, 0.04f), 0.f, 1.f);
    c.k.discrimination = clampf((a.k.discrimination + b.k.discrimination) * 0.4f + gauss(0.f, 0.04f), 0.f, 1.f);
    c.k.tool_skill = clampf((a.k.tool_skill + b.k.tool_skill) * 0.45f, 0.f, 1.f);
    c.k.fire_knowledge = clampf((a.k.fire_knowledge + b.k.fire_knowledge) * 0.4f, 0.f, 1.f);
    c.k.antidote = clampf((a.k.antidote + b.k.antidote) * 0.5f, 0.f, 1.f);

    // HERANÇA DE IDEIAS: o pai/mãe passam o que acreditam (com força parcial)
    auto inherit = [&](const Creature& parent) {
        for (auto& idea : parent.ideas) {
            if (!chance(0.55f * idea.strength + 0.15f)) continue;
            if (c.findIdea(idea.text)) continue;
            c.formIdea(idea.kind, idea.text, idea.strength * 0.65f, currentDay_);
            if (Idea* mine = c.findIdea(idea.text)) {
                mine->fromId = parent.id;
                mine->fromName = parent.name;
                mine->selfTaught = false; // herdada, não descoberta
            }
        }
    };
    inherit(a);
    inherit(b);
    // HERANÇA DE ÓDIO: o sangue lembra o inimigo (feudo entre gerações)
    if (!a.bloodEnemy.empty()) c.bloodEnemy = a.bloodEnemy;
    else if (!b.bloodEnemy.empty()) c.bloodEnemy = b.bloodEnemy;
    // filho de mãe que lutou pode nascer com respeito pelos mortos
    if (a.em.respect > 0.3f || b.em.respect > 0.3f)
        c.em.respect = clampf((a.em.respect + b.em.respect) * 0.3f, 0.f, 1.f);
    // herança comunitária: ideias do grupo se o filho nasce numa comunidade
    int commId = a.communityId >= 0 ? a.communityId : b.communityId;
    if (commId >= 0) {
        c.communityId = commId;
        if (Community* comm = community(commId)) {
            for (auto& gi : comm->groupIdeas) {
                if (!chance(0.4f * gi.strength + 0.1f)) continue;
                if (c.findIdea(gi.text)) continue;
                c.formIdea(gi.kind, gi.text, gi.strength * 0.55f, currentDay_);
                if (Idea* mine = c.findIdea(gi.text)) {
                    mine->fromName = comm->name;
                    mine->selfTaught = false;
                }
            }
        }
    }

    c.parentA = a.id;
    c.parentB = b.id;
    c.state = CState::Wander;
    c.stateT = 1.f;
    c.bornDay = currentDay_;
    // berçário: ponto de nascimento é a "casa" do filhote (orfão não vaga longe)
    c.targetX = c.x;
    c.targetY = c.y;
    // CRÔNICA: primeira linha da vida
    c.addChronicle(currentDay_, "nasceu de " + a.fullName() + " e " + b.fullName());
    // sobrenome vem do pai
    const Creature& dad = a.male ? a : b;
    c.surname = dad.surname.empty() ? dad.name : dad.surname;
    births_++;
    return c;
}

void CreatureSim::teachNearby(float dt) {
    for (auto& j : creatures_) {
        if (!j.alive() || !j.juvenile()) continue;
        const Creature* teacher = nullptr;
        float best = 1e9f;
        for (auto& a : creatures_) {
            if (&a == &j || !a.alive() || a.juvenile()) continue;
            float d = dist2(j.x, j.y, a.x, a.y);
            if (d > 12.f) continue;
            bool isParent = (a.id == j.parentA || a.id == j.parentB);
            float score = d + (isParent ? 0.f : 8.f);
            if (score < best) {
                best = score;
                teacher = &a;
            }
        }
        if (!teacher) continue;
        float w = 0.12f * dt;
        j.k.plant_value = lerpf(j.k.plant_value, teacher->k.plant_value, w);
        j.k.toxic_aversion = lerpf(j.k.toxic_aversion, teacher->k.toxic_aversion, w);
        j.k.discrimination = lerpf(j.k.discrimination, teacher->k.discrimination, w);
        // cultura: filhos herdam ferramenta e fogo dos pais (ensino real)
        j.k.tool_skill = lerpf(j.k.tool_skill, teacher->k.tool_skill, w * 0.9f);
        j.k.fire_knowledge = lerpf(j.k.fire_knowledge, teacher->k.fire_knowledge, w * 0.8f);
        // INTELIGÊNCIA: ensinar acelera o aluno
        j.g.intelligence = clampf(j.g.intelligence + 0.008f * dt * (0.5f + teacher->g.intelligence), 0.f, 1.f);
        j.mind.memory = clampf(j.mind.memory + 0.003f * dt, 0.f, 1.f);
    }
}

void CreatureSim::seekRevenge(Creature& c, float dt, unsigned day) {
    if (c.em.wrathTarget < 0 || c.em.anger < 0.35f) return;
    Creature* enemy = get(c.em.wrathTarget);
    if (!enemy || !enemy->alive()) {
        c.em.wrathTarget = -1;
        return;
    }
    // só vinga se for forte o bastante (senão só foge e guarda rancor)
    bool stronger = c.energy > enemy->energy * 1.1f || c.g.size > enemy->g.size * 1.1f;
    float d = dist2(c.x, c.y, enemy->x, enemy->y);
    if (!stronger) {
        // mantém distância do inimigo
        if (d < 36.f) {
            float dx = c.x - enemy->x, dy = c.y - enemy->y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.01f) len = 1.f;
            moveToward(c, c.x + dx / len * 5.f, c.y + dy / len * 5.f, dt, 1.f);
        }
        return;
    }
    c.state = CState::Fight;
    if (d > 1.1f) {
        moveToward(c, enemy->x, enemy->y, dt, 1.1f);
    } else {
    // golpe de vingança — agressividade multiplica o dano
    damage(*enemy, (10.f + c.g.size * 5.f) * (0.6f + c.g.aggression), c.id, day);
        c.em.anger = clampf(c.em.anger - 0.25f, 0.f, 1.f);
        Bond& b = c.ensureBond(enemy->id, day);
        b.grudge = clampf(b.grudge - 0.35f, 0.f, 1.f);
        b.trust = clampf(b.trust - 0.2f, -1.f, 1.f);
        // o ferido vai lembrar (se sobreviver)
        if (enemy->alive()) {
            enemy->em.wrathTarget = c.id;
            enemy->em.anger = clampf(enemy->em.anger + 0.5f, 0.f, 1.f);
            Bond& eb = enemy->ensureBond(c.id, day);
            eb.trust = -1.f;
            eb.grudge = clampf(eb.grudge + 0.5f, 0.f, 1.f);
            addEvent(c.name + " desabafou a raiva em " + enemy->name + ". Rancor nasceu.", EvKind::War);
        } else {
            addEvent(c.name + " vingou-se. O inimigo nao se levantou.", EvKind::War);
            c.em.grief = clampf(c.em.grief + 0.1f, 0.f, 1.f); // até a vingança pesa
            c.em.anger = clampf(c.em.anger - 0.3f, 0.f, 1.f);
        }
        if (c.em.anger < 0.1f) c.em.wrathTarget = -1;
    }
}

void CreatureSim::socializeNearby(Creature& c, float dt, unsigned day) {
    // família reconhece: filhos, pais e par têm laço mais forte
    for (auto& o : creatures_) {
        if (o.id == c.id || !o.alive()) continue;
        bool isFamily = (o.id == c.parentA || o.id == c.parentB ||
                         std::find(c.childrenIds.begin(), c.childrenIds.end(), o.id) != c.childrenIds.end() ||
                         (!c.surname.empty() && o.surname == c.surname));
        if (isFamily) {
            Bond& b = c.ensureBond(o.id, day);
            b.trust = clampf(b.trust + 0.08f * dt, -1.f, 1.f);
            b.affection = clampf(b.affection + 0.06f * dt, -1.f, 1.f);
            c.em.loneliness = clampf(c.em.loneliness - 0.15f * dt, 0.f, 1.f);
            // família protege: medo cai perto de parentes
            if (c.em.fear > 0.2f) c.em.fear = clampf(c.em.fear - 0.1f * dt, 0.f, 1.f);
        }
    }

    // simbiose com animais: presença calma constrói confiança
    if (eco_) {
        for (auto& f : eco_->all()) {
            if (!f.alive()) continue;
            float d = dist2(c.x, c.y, f.x, f.y);
            if (d > 16.f) continue;
            if (c.em.fear < 0.25f && f.fear < 0.25f) {
                // convivência pacífica
                f.trustSages = clampf(f.trustSages + 0.02f * dt, 0.f, 1.f);
                SpeciesBond& sb = c.ensureSpeciesBond((int)f.type);
                sb.trust = clampf(sb.trust + 0.025f * dt, -1.f, 1.f);
                if (sb.trust > 0.4f) {
                    sb.affection = clampf(sb.affection + 0.02f * dt, 0.f, 1.f);
                }
            } else if (c.em.fear > 0.5f) {
                // medo contamina o encontro
                f.trustSages = clampf(f.trustSages - 0.01f * dt, 0.f, 1.f);
            }
        }
    }

    for (auto& o : creatures_) {
        if (o.id == c.id || !o.alive()) continue;
        float d = dist2(c.x, c.y, o.x, o.y);
        if (d > 9.f) continue;

        Bond& b = c.ensureBond(o.id, day);
        Bond* ob = o.bond(c.id);
        if (!ob) {
            float first = c.em.curiosity * 0.3f - c.em.fear * 0.2f;
            // INIMIGO DE SANGUE: desconfia de quem carrega o sobrenome odeado
            if (!c.bloodEnemy.empty() &&
                (o.surname == c.bloodEnemy || o.name == c.bloodEnemy)) {
                first = -0.4f;
                c.em.anger = clampf(c.em.anger + 0.15f * dt, 0.f, 1.f);
                if (chance(0.01f * dt * 30.f))
                    addEvent(c.name + " viu um " + c.bloodEnemy + ". O sangue lembrou.", EvKind::War);
            }
            b.trust = clampf(first, -0.5f, 0.4f);
        }
        // encontro com inimigo de sangue: rancor cresce
        if (!c.bloodEnemy.empty() &&
            (o.surname == c.bloodEnemy || o.name == c.bloodEnemy)) {
            b.trust = clampf(b.trust - 0.03f * dt, -1.f, 1.f);
            b.grudge = clampf(b.grudge + 0.04f * dt, 0.f, 1.f);
            c.em.anger = clampf(c.em.anger + 0.05f * dt, 0.f, 1.f);
        }

        // convivência pacífica constrói confiança
        if (c.em.anger < 0.2f && o.em.anger < 0.2f) {
            float w = 0.03f * dt * (0.5f + c.g.sociability);
            b.trust = clampf(b.trust + w, -1.f, 1.f);
            if (b.trust > 0.3f) b.affection = clampf(b.affection + w * 0.5f, -1.f, 1.f);
            o.em.loneliness = clampf(o.em.loneliness - 0.1f * dt, 0.f, 1.f);
            c.em.loneliness = clampf(c.em.loneliness - 0.1f * dt, 0.f, 1.f);

            // ---- AMIZADE nasce ----
            if (b.affection > 0.4f && o.id != c.em.mateId) {
                // virou amigo: evento + ideia (uma vez)
                if (!c.findIdea("amizade")) {
                    c.formIdea(IdeaKind::Social, "amizade aquece a vida", 0.45f, day, 0.85f);
                    addEvent(c.name + " e " + o.name + " viraram amigos.", EvKind::Love);
                }
                // amigo triste recebe conforto
                if (o.em.grief > 0.3f || o.em.loneliness > 0.4f) {
                    o.em.grief = clampf(o.em.grief - 0.06f * dt, 0.f, 1.f);
                    o.em.loneliness = clampf(o.em.loneliness - 0.12f * dt, 0.f, 1.f);
                    o.em.comfort = clampf(o.em.comfort + 0.04f * dt, 0.f, 1.f);
                    c.em.satisfaction = clampf(c.em.satisfaction + 0.02f * dt, 0.f, 1.f);
                }
                // amigo com fome: compartilha energia (se eu tenho sobra)
                if (o.energy < 35.f && c.energy > 70.f) {
                    float give = 4.f * dt;
                    o.energy = clampf(o.energy + give, 0.f, 100.f);
                    c.energy -= give * 0.5f;
                    c.em.satisfaction = clampf(c.em.satisfaction + 0.02f * dt, 0.f, 1.f);
                    if (chance(0.02f * dt * 30.f))
                        changeReputation(c, 0.03f, day, "compartilhou comida");
                    if (!c.findIdea("compartilhar"))
                        c.formIdea(IdeaKind::Social, "compartilhar fortalece os laços", 0.4f, day, 0.85f);
                }
            }

            // COMO ESTRANHOS CONVERTEM: confiança + empatia + ideia forte = proselitismo
                    if (b.trust > 0.35f && !c.ideas.empty() && chance(0.015f * dt * 30.f * c.pers.empathy * (0.3f + c.g.intelligence))) {
                        const Idea& idea = c.ideas[irand(0, (int)c.ideas.size() - 1)];
                        if (idea.strength > 0.35f) {
                            if (Idea* theirs = o.findIdea(idea.text)) {
                                theirs->strength = clampf(theirs->strength + idea.strength * 0.04f, 0.f, 1.f);
                            } else {
                                o.formIdea(idea.kind, idea.text, idea.strength * 0.4f, day);
                                if (Idea* mine = o.findIdea(idea.text)) {
                                    mine->fromId = c.id;
                                    mine->fromName = c.name;
                                    mine->selfTaught = false;
                                }
                                if (chance(0.25f))
                                    addEvent(o.name + " comecou a acreditar: \"" + idea.text + "\"", EvKind::Idea);
                            }
                        }
                    }
        }

        // rancor antigo não deixa a paz
        if (b.grudge > 0.4f && b.trust < 0.f) {
            b.trust = clampf(b.trust - 0.02f * dt, -1.f, 1.f);
            if (b.grudge > 0.7f && c.em.anger > 0.2f && c.em.wrathTarget < 0) {
                c.em.wrathTarget = o.id;
            }
        }
    }
}

void CreatureSim::behavior(Creature& c, float dt, unsigned day) {
    // ---- hierarquia de sentimentos e necessidades ----

    // ---- CRIANÇA NÃO VAGA SOZINHA ----
    if (c.juvenile()) {
        // doente: mesmo criança busca antídoto
        if (c.sickT > 0.f) {
            // deixa o bloco de doença tratar (está mais abaixo)
        } else {
            // ---- CAVERNA: criança busca abrigo (frio, noite, medo) ----
            float stress = thermalStress(c);
            bool scared = c.em.fear > 0.35f;
            bool night = world_.isNight();
            bool inCave = world_.inBounds((int)c.x, (int)c.y) &&
                          world_.at((int)c.x, (int)c.y).cave;
            if (!inCave && (stress > 0.3f || scared || (night && c.em.comfort < 0.5f))) {
                float bx = c.x, by = c.y;
                float best = 1e9f;
                bool found = false;
                int r = (int)(c.g.vision + 5.f);
                int tx = (int)c.x, ty = (int)c.y;
                for (int y = ty - r; y <= ty + r; y++) {
                    for (int x = tx - r; x <= tx + r; x++) {
                        if (!world_.inBounds(x, y)) continue;
                        if (!world_.at(x, y).cave) continue;
                        float d = dist2(c.x, c.y, x + 0.5f, y + 0.5f);
                        if (d < best) { best = d; bx = x + 0.5f; by = y + 0.5f; found = true; }
                    }
                }
                if (found && best < 100.f) {
                    c.state = CState::SeekShelter;
                    moveToward(c, bx, by, dt, scared ? 1.1f : 0.85f);
                    return;
                }
            }
            // dentro da caverna: segura, aprende
            if (inCave) {
                c.em.security = clampf(c.em.security + 0.08f * dt, 0.f, 1.f);
                c.em.comfort = clampf(c.em.comfort + 0.06f * dt, 0.f, 1.f);
                c.em.fear = clampf(c.em.fear - 0.08f * dt, 0.f, 1.f);
            }

            // fica perto do pai/mãe vivo
            Creature* parent = nullptr;
            float bestD = 1e9f;
            for (int pid : {c.parentA, c.parentB}) {
                if (pid < 0) continue;
                Creature* p = get(pid);
                if (!p || !p->alive()) continue;
                float d = dist2(c.x, c.y, p->x, p->y);
                if (d < bestD) { bestD = d; parent = p; }
            }
            if (parent) {
                // PERIGO: pai morto ou longe demais → corre atrás
                if (bestD > 100.f) {
                    c.state = CState::SeekMate; // "ir até alguém"
                    moveToward(c, parent->x, parent->y, dt, 1.1f);
                    return;
                }
                // sede: bebe SEMPRE que precisa (não espera o pai chegar)
                if (c.hydration < 40.f) {
                    c.state = CState::SeekWater;
                    tryDrink(c, dt);
                    return;
                }
                // fome: come se tiver pertinho do pai
                if (c.energy < 45.f && bestD < 36.f) {
                    c.state = CState::SeekFood;
                    tryEat(c, dt * 0.6f);
                    return;
                }
                if (c.energy < 20.f) {
                    c.state = CState::Rest;
                    c.energy += 3.f * dt; // desacorda
                    return;
                }
                // perto do pai: imita, socializa, anda junto
                c.g.intelligence = clampf(c.g.intelligence + 0.01f * dt, 0.f, 1.f);
                c.state = CState::Gather;
                if (bestD > 9.f) moveToward(c, parent->x, parent->y, dt, 0.8f);
                else if (bestD > 4.f && chance(0.3f * dt)) {
                    // anda junto, mas não colado
                    moveToward(c, parent->x, parent->y, dt, 0.4f);
                }
                socializeNearby(c, dt, day);
                return;
            }
            // ÓRFÃO: procura qualquer adulto pra adotar — não fica perdido
            {
                Creature* adult = nullptr;
                float bd = 1e9f;
                int r = (int)(c.g.vision + 8.f); // busca ampla
                for (auto& o : creatures_) {
                    if (o.id == c.id || !o.alive() || o.juvenile()) continue;
                    float d = dist2(c.x, c.y, o.x, o.y);
                    // prefere parente de sangue, depois qualquer um
                    bool kin = (!c.surname.empty() && o.surname == c.surname) ||
                               (o.id == c.parentA || o.id == c.parentB);
                    float bonus = kin ? 0.5f : 1.f;
                    float score = d * bonus;
                    if (score < bd && d < (float)(r * r)) {
                        bd = score;
                        adult = &o;
                    }
                }
                if (adult) {
                    float dAd = dist2(c.x, c.y, adult->x, adult->y);
                    // chegou perto: pede adoção
                    if (dAd < 2.6f && c.stateT <= 0.f) {
                        // adulto aceita se tiver coração (moral/empatia) e espaço na família
                        bool willing = (adult->morality > 0.25f || adult->pers.empathy > 0.35f) &&
                                       adult->livingChildren < 6;
                        if (willing && chance(0.35f + adult->pers.empathy * 0.35f)) {
                            // ADOTEI
                            if (c.parentA < 0) c.parentA = adult->id;
                            else if (c.parentB < 0 || !get(c.parentB) || !get(c.parentB)->alive())
                                c.parentB = adult->id;
                            adult->childrenIds.push_back(c.id);
                            adult->children++;
                            adult->livingChildren++;
                            Bond& ba = adult->ensureBond(c.id, day);
                            ba.trust = 0.8f;
                            ba.affection = 0.7f;
                            Bond& bc = c.ensureBond(adult->id, day);
                            bc.trust = 0.85f;
                            bc.affection = 0.8f;
                            adult->em.happiness = clampf(adult->em.happiness + 0.15f, 0.f, 1.f);
                            adult->em.pride = clampf(adult->em.pride + 0.2f, 0.f, 1.f);
                            c.em.security = clampf(c.em.security + 0.4f, 0.f, 1.f);
                            c.em.loneliness = clampf(c.em.loneliness - 0.5f, 0.f, 1.f);
                            c.addChronicle(day, "foi adotado por " + adult->fullName());
                            adult->addChronicle(day, "adotou " + c.fullName());
                            addEvent(adult->name + " adotou " + c.fullName() + ". A famila cresceu.", EvKind::Love);
                            // filho herda comunidade do adotante
                            if (c.communityId < 0 && adult->communityId >= 0)
                                c.communityId = adult->communityId;
                        }
                        c.stateT = frand(4.f, 8.f); // espera pra tentar de novo
                    }
                    // anda até o adulto
                    c.state = CState::Gather;
                    if (dAd > 9.f) moveToward(c, adult->x, adult->y, dt, 0.9f);
                    else if (dAd > 2.5f) moveToward(c, adult->x, adult->y, dt, 0.5f);
                    // come/bebe no caminho
                    if (c.hydration < 40.f) {
                        c.state = CState::SeekWater;
                        tryDrink(c, dt);
                        return;
                    }
                    if (c.energy < 45.f) {
                        c.state = CState::SeekFood;
                        tryEat(c, dt * 0.5f);
                        return;
                    }
                    socializeNearby(c, dt, day);
                    return;
                }
                // NENHUM adulto no mundo: sobrevive como pode, sem morrer parado
                if (c.hydration < 40.f) {
                    c.state = CState::SeekWater;
                    tryDrink(c, dt);
                    return;
                }
                if (c.energy < 50.f) {
                    c.state = CState::SeekFood;
                    tryEat(c, dt * 0.6f);
                    return;
                }
                c.state = CState::Gather;
                wander(c, dt * 0.25f);
                socializeNearby(c, dt, day);
                return;
            }
        }
        // criança doente cai aqui e cai no bloco de doença abaixo
    }

    // ---- FAMÍLIA NÃO SE SEPARA (sem virar perseguição infinita) ----
    // par: se estão em comunidades diferentes ou sem comunidade, volta pro par
    if (c.em.mateId >= 0 && c.energy > 45.f && c.em.fear < 0.55f) {
        Creature* mate = get(c.em.mateId);
        if (mate && mate->alive()) {
            float dMate = dist2(c.x, c.y, mate->x, mate->y);
            bool sameComm = (c.communityId >= 0 && c.communityId == mate->communityId);
            if (!sameComm && dMate > 36.f && dMate < 160.f) {
                c.state = CState::SeekMate;
                moveToward(c, mate->x, mate->y, dt, 0.85f);
                return;
            }
        }
    }
    // SOLITÁRIO: procura o melhor amigo (não fica perdido)
    if (c.em.mateId < 0 && c.em.loneliness > 0.45f && c.energy > 40.f && c.em.fear < 0.4f) {
        int bfId = -1;
        float bfAff = 0.4f;
        float bd = 1e9f;
        for (auto& b : c.bonds) {
            if (b.affection < bfAff || b.id == c.em.mateId) continue;
            Creature* fr = get(b.id);
            if (!fr || !fr->alive() || fr->juvenile()) continue;
            float d = dist2(c.x, c.y, fr->x, fr->y);
            if (d < bd && d < 144.f) { bd = d; bfId = b.id; bfAff = b.affection; }
        }
        if (bfId >= 0) {
            Creature* fr = get(bfId);
            c.state = CState::Gather;
            if (bd > 16.f) moveToward(c, fr->x, fr->y, dt, 0.6f);
            else {
                // junto do amigo: solidão cai
                c.em.loneliness = clampf(c.em.loneliness - 0.15f * dt, 0.f, 1.f);
                c.em.comfort = clampf(c.em.comfort + 0.03f * dt, 0.f, 1.f);
                socializeNearby(c, dt, day);
            }
            return;
        }
    }
    // filho adulto: se o pai está PERTO, reforça o laço (não puxa de longe)
    if (c.em.fear < 0.4f && c.energy > 40.f && c.em.mateId < 0) {
        Creature* parent = nullptr;
        float bd = 1e9f;
        for (int pid : {c.parentA, c.parentB}) {
            if (pid < 0) continue;
            Creature* p = get(pid);
            if (!p || !p->alive()) continue;
            float d = dist2(c.x, c.y, p->x, p->y);
            if (d < bd) { bd = d; parent = p; }
        }
        // só se o pai está a 4-10 tiles (perto, mas se afastando)
        if (parent && bd > 16.f && bd < 64.f) {
            c.state = CState::Gather;
            moveToward(c, parent->x, parent->y, dt, 0.4f);
            return;
        }
    }

    // 5) doente: busca antídoto (planta-cura) — a vontade de viver
    if (c.sickT > 0.f || c.state == CState::Sick) {
        c.sickT = clampf(c.sickT - dt, 0.f, 99.f);
        if (c.sickT > 0.f) {
            c.state = CState::Sick;
            // procura planta com gene cure alto
            float bx = c.x, by = c.y, best = -1.f;
            int r = (int)(c.g.vision + 3.f);
            int cx = (int)c.x, cy = (int)c.y;
            for (int y = cy - r; y <= cy + r; y++) {
                for (int x = cx - r; x <= cx + r; x++) {
                    if (!world_.inBounds(x, y)) continue;
                    const Plant& p = world_.at(x, y).plant;
                    if (p.kind == PlantKind::None || p.stage < 1) continue;
                    float sc = p.gene.cure * 2.f - dist2(c.x, c.y, x + 0.5f, y + 0.5f) * 0.05f;
                    if (p.gene.cure < 0.15f) sc = -1.f;
                    if (sc > best) { best = sc; bx = x + 0.5f; by = y + 0.5f; }
                }
            }
            if (best > 0.f) {
                moveToward(c, bx, by, dt, 1.1f);
                if (dist2(c.x, c.y, bx, by) < 0.4f) {
                    // come como remédio
                    c.state = CState::Eat;
                    c.stateT = 1.f;
                    // reutiliza tryEat
                    tryEat(c, dt);
                }
            } else {
                wander(c, dt * 0.3f);
                c.energy -= 0.15f * dt;
            }
            if (c.sickT <= 0.f) {
                c.state = CState::Wander;
                addEvent(c.name + " se recuperou do veneno.");
            }
            return;
        }
    }

    // satisfação: quem comeu bem não precisa de tanto
    if (c.em.satisfaction > 0.f) {
        c.em.satisfaction = clampf(c.em.satisfaction - 0.05f * dt, 0.f, 1.f);
    }

    // ---- NOITE: pensantes dormem se têm sono ----
    bool night = world_.isNight();
    if (night && c.state != CState::Fight && c.state != CState::SeekShelter &&
        c.hydration > 20.f && c.energy > 12.f) {
        // sono: fadiga + preguiça + conforto alto = cochilo
        float sleepNeed = (1.f - c.energy / 100.f)
                        + c.pers.laziness * 0.25f
                        + (1.f - c.em.comfort) * 0.15f
                        - c.pers.boldness * 0.1f;
        // casal ou comunidade: dorme mais fácil (segurança)
        Creature* sleepMate = c.em.mateId >= 0 ? get(c.em.mateId) : nullptr;
        bool sleepNearMate = sleepMate && sleepMate->alive() &&
                             dist2(c.x, c.y, sleepMate->x, sleepMate->y) < 16.f;
        if (sleepNearMate) sleepNeed += 0.15f;
        if (c.communityId >= 0) sleepNeed += 0.1f;
        if (sleepNeed > 0.4f || (sleepNearMate && c.em.fear < 0.25f)) {
            c.state = CState::Rest;
            float regen = 2.2f + c.em.comfort * 1.5f + (sleepNearMate ? 1.2f : 0.f);
            c.energy += regen * dt;
            c.hydration += 0.3f * dt;
            // dormir com o par: conforto e segurança crescem juntos
            if (sleepNearMate) {
                c.em.comfort = clampf(c.em.comfort + 0.08f * dt, 0.f, 1.f);
                c.em.security = clampf(c.em.security + 0.06f * dt, 0.f, 1.f);
                c.em.loneliness = clampf(c.em.loneliness - 0.15f * dt, 0.f, 1.f);
            }
            // bem descansado, acorda; com muita fome, acorda pra comer
            if (c.energy > 75.f) c.state = CState::Wander;
            return;
        }
    }

    // 1) LUTO COM RESPEITO: lembra dos mortos, visita o túmulo
    if (c.em.grief > 0.45f) {
        c.state = CState::Mourn;
        c.energy += 1.2f * dt;
        // respeito cresce no luto
        c.em.respect = clampf(c.em.respect + 0.02f * dt, 0.f, 1.f);
        // se tem túmulo marcado, vai até lá lembrar
        if (c.graveX >= 0.f) {
            float dGrave = dist2(c.x, c.y, c.graveX, c.graveY);
            if (dGrave > 4.f) {
                moveToward(c, c.graveX, c.graveY, dt, 0.5f);
            } else {
                // no túmulo: lembrança e paz momentânea
                c.em.grief = clampf(c.em.grief - 0.05f * dt, 0.f, 1.f);
                c.em.comfort = clampf(c.em.comfort + 0.03f * dt, 0.f, 1.f);
                if (chance(0.008f * dt * 30.f) && !c.findIdea("lembrar")) {
                    std::string who = c.graveName.empty() ? "quem partiu" : c.graveName;
                    c.formIdea(IdeaKind::Love, who + " ainda mora na minha memoria", 0.5f, day, 0.85f);
                    addEvent(c.name + " visitou o tumulo de " + who + ". Lembrar e respeitar.", EvKind::Death);
                }
            }
        } else {
            wander(c, dt * 0.2f);
        }
        if (c.em.grief < 0.3f) c.state = CState::Wander;
        return;
    }

    // 1b) VISITA AO TÚMULO: mesmo fora do luto profundo, lembra de vez em quando
    if (c.graveX >= 0.f && c.em.grief < 0.45f && c.em.grief > 0.05f &&
        c.stateT <= 0.f && chance(0.004f * dt * 30.f * (0.5f + c.em.respect))) {
        // visita rápida: conforto e lembrança
        if (dist2(c.x, c.y, c.graveX, c.graveY) < 9.f) {
            c.em.grief = clampf(c.em.grief - 0.04f, 0.f, 1.f);
            c.em.comfort = clampf(c.em.comfort + 0.03f, 0.f, 1.f);
            c.em.respect = clampf(c.em.respect + 0.02f, 0.f, 1.f);
            c.addChronicle(day, "visitou o tumulo de " +
                (c.graveName.empty() ? std::string("alguem querido") : c.graveName));
            c.stateT = frand(30.f, 60.f); // não visita toda hora
        } else {
            c.state = CState::Gather;
            moveToward(c, c.graveX, c.graveY, dt, 0.55f);
            return;
        }
    }

    // 2) PROTEGER A CRIANÇA: adulto não foge de quem ameaça filho
    if (!c.juvenile() && c.energy > 20.f) {
        // acha filho em perigo
        Creature* kidDanger = nullptr;
        float kidFear = 0.35f;
        for (auto& o : creatures_) {
            if (!o.alive() || !o.juvenile()) continue;
            bool myKid = std::find(c.childrenIds.begin(), c.childrenIds.end(), o.id) != c.childrenIds.end();
            bool nearKid = dist2(c.x, c.y, o.x, o.y) < 49.f;
            if (!myKid && !nearKid) continue;
            if (o.em.fear > kidFear) { kidFear = o.em.fear; kidDanger = &o; }
        }
        if (kidDanger) {
            float px2, py2;
            int tid2 = -1;
            if (findThreat(*kidDanger, px2, py2, tid2)) {
                c.state = CState::Fight;
                c.em.anger = clampf(c.em.anger + 0.2f * dt, 0.f, 1.f);
                c.em.fear = clampf(c.em.fear - 0.15f * dt, 0.f, 1.f); // coragem
                moveToward(c, px2, py2, dt, 1.15f);
                if (dist2(c.x, c.y, px2, py2) < 1.4f && tid2 >= 0) {
                    // golpe de proteção contra outro pensante
                    if (Creature* th = get(tid2)) {
                        damage(*th, 8.f + c.g.size * 3.f + c.body.strength * 4.f, c.id, day);
                        if (!c.findIdea("proteger"))
                            c.formIdea(IdeaKind::Love, "proteger os pequenos e o certo", 0.55f, day, 0.9f);
                        c.em.pride = clampf(c.em.pride + 0.15f, 0.f, 1.f);
                        changeReputation(c, 0.06f, day, "protegeu uma crianca");
                        if (chance(0.2f))
                            addEvent(c.name + " defendeu uma crianca. Ninguem toca nos filhos.", EvKind::War);
                    }
                }
                return;
            }
        }
    }

    // 3) vingança: a raiva é mais forte que a fome — mas força de vontade controla
    if (c.em.anger > 0.45f && c.em.wrathTarget >= 0 && c.energy > 25.f &&
        c.mind.willpower > 0.3f) {
        seekRevenge(c, dt, day);
        return;
    }

    // 3) terror — mas força de vontade resiste
    if (c.em.fear > 0.6f && c.mind.willpower < 0.6f) {
        float px, py;
        int tid = -1;
        if (findThreat(c, px, py, tid)) {
            float dx = c.x - px, dy = c.y - py;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.01f) len = 1.f;
            moveToward(c, c.x + dx / len * 6.f, c.y + dy / len * 6.f, dt, 1.2f);
            c.state = CState::SeekShelter;
            return;
        }
    }

    // 3b) simbiose: predador que confia em NÓS não ataca
    // (já tratado no findThreat via speciesBonds)

    // 3c) investigar: curiosidade + pensador = cientista; ousado explora mais
    // INTELIGÊNCIA: pensador investiga muito mais; inteligência alta acelera
    float invChance = 0.006f * c.em.curiosity * (0.4f + c.pers.thinker) * (0.5f + c.pers.wanderlust * 0.5f);
    invChance *= (1.f + c.g.intelligence * 1.5f + c.mind.creativity);
    if (c.em.curiosity > 0.35f && c.energy > 35.f && c.age > 0.8f && chance(invChance)) {
        // acha algo interessante: carcaça recente, planta estranha, predador morto
        float bx = c.x, by = c.y, best = -1.f;
        int kind = 0; // 0 nada, 1 carcaça, 2 planta, 3 "por que existe isso?"
        int r = (int)(c.g.vision + 4.f);
        int cx = (int)c.x, cy = (int)c.y;
        for (int y = cy - r; y <= cy + r; y++) {
            for (int x = cx - r; x <= cx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                const TileData& t = world_.at(x, y);
                float sc = -1.f;
                int k = 0;
                if (t.corpse > 0) { sc = 2.f - dist2(c.x, c.y, x + 0.5f, y + 0.5f) * 0.04f; k = 1; }
                else if (t.plant.kind != PlantKind::None && t.plant.gene.cure > 0.2f) {
                    sc = 1.5f - dist2(c.x, c.y, x + 0.5f, y + 0.5f) * 0.04f; k = 2;
                } else if (t.plant.kind != PlantKind::None && t.plant.stage == 2) {
                    sc = 0.5f - dist2(c.x, c.y, x + 0.5f, y + 0.5f) * 0.02f; k = 3;
                }
                if (sc > best) { best = sc; bx = x + 0.5f; by = y + 0.5f; kind = k; }
            }
        }
        if (best > 0.f) {
            c.state = CState::Investigate;
            c.targetX = bx;
            c.targetY = by;
            c.stateT = 2.5f + c.g.intelligence * 2.f;
            // marca o que estudar no state via stateT
            (void)kind;
            moveToward(c, bx, by, dt);
            if (dist2(c.x, c.y, bx, by) < 0.5f) {
                // chegou — estuda (aprender é prazeroso)
                learnPleasure(c, 0.15f);
                if (kind == 1) {
                    c.k.predator_knowledge = clampf(c.k.predator_knowledge + 0.06f, 0.f, 1.f);
                    c.g.intelligence = clampf(c.g.intelligence + 0.02f, 0.f, 1.f);
                    learnPleasure(c, 0.12f);
                    c.formIdea(IdeaKind::Predator, "a morte tem um motivo", 0.4f, currentDay_, 0.45f);
                    if (chance(0.15f)) addEvent(c.name + " observou uma morte e entendeu o perigo.", EvKind::Discovery);
                } else if (kind == 2) {
                    c.k.antidote = clampf(c.k.antidote + 0.05f, 0.f, 1.f);
                    c.g.intelligence = clampf(c.g.intelligence + 0.015f, 0.f, 1.f);
                    c.formIdea(IdeaKind::Plant, "as plantas escondem respostas", 0.4f, currentDay_, 0.6f);
                    if (chance(0.2f)) addEvent(c.name + " estudou a planta-cura e aprendeu onde ela nasce.", EvKind::Discovery);
                } else {
                    c.em.curiosity = clampf(c.em.curiosity + 0.05f, 0.f, 1.f);
                    c.g.intelligence = clampf(c.g.intelligence + 0.01f, 0.f, 1.f);
                    if (chance(0.25f))
                        c.formIdea(IdeaKind::Mystery, "por que existe isso?", 0.35f + c.pers.thinker * 0.3f, currentDay_, 0.55f);
                    if (chance(0.08f)) {
                        addEvent(c.name + " olhou a fruta e perguntou: por que existe isso?", EvKind::Idea);
                    }
                }
                c.state = CState::Wander;
            }
            return;
        }
    }

    // 3d) comunidade: 3+ pensantes amigos por perto → reunião
    if (c.g.intelligence > 0.25f && c.em.loneliness > 0.05f) {
        int friends = 0;
        float fx = 0, fy = 0;
        for (auto& o : creatures_) {
            if (o.id == c.id || !o.alive() || o.juvenile()) continue;
            if (dist2(c.x, c.y, o.x, o.y) < 36.f) {
                Bond* b = c.bond(o.id);
                if (!b || b->trust > 0.1f) {
                    friends++;
                    fx += o.x;
                    fy += o.y;
                }
            }
        }
        // se tem comunidade, reúne lá
        if (c.communityId >= 0) {
            if (Community* comm = community(c.communityId)) {
                c.state = CState::Gather;
                if (dist2(c.x, c.y, comm->x, comm->y) > 4.f)
                    moveToward(c, comm->x, comm->y, dt, 0.75f);
                else {
                    c.g.intelligence = clampf(c.g.intelligence + 0.03f * dt, 0.f, 1.f);
                    c.em.loneliness = clampf(c.em.loneliness - 0.3f * dt, 0.f, 1.f);
                    c.em.satisfaction = clampf(c.em.satisfaction + 0.1f * dt, 0.f, 1.f);
                    // constrói se tem ferramenta
                    if (c.k.tool_skill > 0.25f && !comm->hasFire() && c.k.fire_knowledge > 0.1f &&
                        chance(0.02f * dt)) {
                        // MADEIRA: precisa de árvore/lenha por perto
                        bool hasWood = false;
                        int wx = 0, wy = 0;
                        for (int y = (int)comm->y - 6; y <= (int)comm->y + 6 && !hasWood; y++) {
                            for (int x = (int)comm->x - 6; x <= (int)comm->x + 6; x++) {
                                if (!world_.inBounds(x, y)) continue;
                                TileData& t = world_.at(x, y);
                                if (t.wood > 0) {
                                    t.wood--;
                                    hasWood = true;
                                    wx = x; wy = y;
                                    break;
                                }
                                if (t.trees > 0) {
                                    int got = 0;
                                    if (world_.chopTree(x, y, got) && got > 0) {
                                        t.wood = (uint8_t)std::max(0, (int)t.wood - 1); // usa uma
                                        hasWood = true;
                                        wx = x; wy = y;
                                        break;
                                    }
                                }
                            }
                        }
                        if (hasWood) {
                            Building b; b.type = 0; b.x = comm->x; b.y = comm->y;
                            comm->buildings.push_back(b);
                            addEvent(c.name + " cortou lenha e acendeu fogueira em " + comm->name + ".", EvKind::Build);
                            c.state = CState::Build;
                        }
                    }
                    // toca: precisa de madeira também
                    if (!comm->hasHut() && c.k.tool_skill > 0.35f && chance(0.008f * dt)) {
                        bool hasWood = false;
                        for (int y = (int)comm->y - 7; y <= (int)comm->y + 7 && !hasWood; y++) {
                            for (int x = (int)comm->x - 7; x <= (int)comm->x + 7; x++) {
                                if (!world_.inBounds(x, y)) continue;
                                TileData& t = world_.at(x, y);
                                if (t.wood > 0) { t.wood--; hasWood = true; break; }
                                if (t.trees > 0) {
                                    int got = 0;
                                    if (world_.chopTree(x, y, got)) { hasWood = true; break; }
                                }
                            }
                        }
                        if (hasWood) {
                            Building b; b.type = 1; b.x = comm->x + frand(-3, 3); b.y = comm->y + frand(-3, 3);
                            comm->buildings.push_back(b);
                            addEvent(comm->name + " ergueu a primeira toca com madeira.", EvKind::Build);
                        }
                    }
                    // cria ferramenta física (só se nível cultural permite)
                    if (c.tool == 0 && c.k.tool_skill > 0.3f && c.culturalLevel >= 1 && chance(0.015f * dt)) {
                        if (c.k.fire_knowledge > 0.2f) c.tool = 1; // faca
                        else c.tool = 2; // cesto
                        addEvent(c.name + " criou uma ferramenta.", EvKind::Discovery);
                    }
                    // ferramenta melhor: enxada (agricultura)
                    if (c.tool == 2 && c.k.tool_skill > 0.5f && c.culturalLevel >= 2 && chance(0.008f * dt)) {
                        c.tool = 4;
                        addEvent(c.name + " criou uma enxada. Pode plantar!", EvKind::Discovery);
                    }
                    // ferramenta melhor: arma
                    if (c.tool == 1 && c.g.aggression > 0.4f && c.k.tool_skill > 0.4f && c.culturalLevel >= 2 && chance(0.005f * dt)) {
                        c.tool = 3;
                        addEvent(c.name + " criou uma arma.", EvKind::Discovery);
                    }
                    // agricultura: planta de propósito com enxada (era agrícola)
                    if (c.tool == 4 && c.culturalLevel >= 3 && chance(0.02f * dt)) {
                        int tx = (int)c.x, ty = (int)c.y;
                        if (world_.inBounds(tx, ty) && world_.at(tx, ty).plant.kind == PlantKind::None) {
                            TileData& t = world_.at(tx, ty);
                            if (t.type == Tile::Grass || t.type == Tile::Soil || t.type == Tile::Forest) {
                                PlantKind k = c.k.antidote > 0.3f ? PlantKind::Fruit : PlantKind::Fruit;
                                world_.spawnPlant(tx, ty, k, 0);
                                addEvent(c.name + " plantou uma semente. (agricultura!)", EvKind::Discovery);
                            }
                        }
                    }
                    // cerimônia: reforça ideias do grupo
                    if (comm->hasFire() && !comm->groupIdeas.empty() && chance(0.01f * dt)) {
                        for (int id : comm->memberIds) {
                            Creature* m = get(id);
                            if (!m || !m->alive()) continue;
                            if (dist2(c.x, c.y, m->x, m->y) > 9.f) continue;
                            const Idea& gi = comm->groupIdeas[irand(0, (int)comm->groupIdeas.size() - 1)];
                            if (Idea* mine = m->findIdea(gi.text)) {
                                mine->strength = clampf(mine->strength + 0.08f, 0.f, 1.f);
                            }
                            Bond& bb = c.ensureBond(m->id, currentDay_);
                            bb.trust = clampf(bb.trust + 0.05f, -1.f, 1.f);
                        }
                        c.em.satisfaction = clampf(c.em.satisfaction + 0.15f, 0.f, 1.f);
                        c.state = CState::Gather;
                    }
                    // escrita: registra ideia forte em "pedra" (era da escrita)
                    if (!c.ideas.empty() && c.g.intelligence > 0.45f && c.culturalLevel >= 4 && chance(0.005f * dt)) {
                        const Idea& best = c.ideas[irand(0, (int)c.ideas.size() - 1)];
                        if (best.strength > 0.5f && (int)c.writtenIdeas.size() < 8) {
                            bool dup = false;
                            for (auto& wi : c.writtenIdeas) if (wi.text == best.text) { dup = true; break; }
                            if (!dup) {
                                c.writtenIdeas.push_back(best);
                                addEvent(c.name + " registrou: \"" + best.text + "\"", EvKind::Idea);
                            }
                        }
                    }
                }
                return;
            }
        }
        if (friends >= 2) {
            c.state = CState::Gather;
            fx /= friends;
            fy /= friends;
            if (dist2(c.x, c.y, fx, fy) > 2.f) moveToward(c, fx, fy, dt, 0.7f);
            else {
                // no círculo: inteligência cresce mais rápido, ensino mútuo
                c.g.intelligence = clampf(c.g.intelligence + 0.03f * dt, 0.f, 1.f);
                c.em.loneliness = clampf(c.em.loneliness - 0.25f * dt, 0.f, 1.f);
                c.em.satisfaction = clampf(c.em.satisfaction + 0.08f * dt, 0.f, 1.f);
                if (chance(0.02f))
                    c.formIdea(IdeaKind::Community, "juntos somos fortes", 0.35f + c.pers.empathy * 0.3f, currentDay_, 0.85f);
                // compartilha ideias e ensino com quem está perto
                for (auto& o : creatures_) {
                    if (o.id == c.id || !o.alive()) continue;
                    if (dist2(c.x, c.y, o.x, o.y) > 9.f) continue;
                    Bond& b = c.ensureBond(o.id, currentDay_);
                    b.trust = clampf(b.trust + 0.05f * dt, -1.f, 1.f);
                    float w = 0.04f * dt;
                    o.k.tool_skill = lerpf(o.k.tool_skill, std::max(o.k.tool_skill, c.k.tool_skill), w);
                    o.k.fire_knowledge = lerpf(o.k.fire_knowledge, std::max(o.k.fire_knowledge, c.k.fire_knowledge), w);
                    o.k.antidote = lerpf(o.k.antidote, std::max(o.k.antidote, c.k.antidote), w);
                    o.g.intelligence = lerpf(o.g.intelligence, std::max(o.g.intelligence, c.g.intelligence), w * 0.5f);
                    // ensina IDEIAS: quem confia mais, aprende mais
                    // MENTIRA: mente pode plantar crença falsa
                    if (b.trust > 0.15f) {
                        for (auto& idea : c.ideas) {
                            if (!chance(0.03f * dt * (0.5f + c.pers.empathy))) continue;
                            // mente: baixa moral + inteligência = manipulador
                            bool isLie = (c.morality < 0.35f && c.g.intelligence > 0.4f &&
                                          idea.morality > 0.6f && chance(0.15f));
                            std::string text = idea.text;
                            float moral = idea.morality;
                            if (isLie) {
                                // distorce ideia boa em ideia que serve ao mentiroso
                                text = "so os fortes merecem sobreviver";
                                moral = 0.1f;
                            }
                            if (Idea* exists = o.findIdea(text)) {
                                exists->strength = clampf(exists->strength + idea.strength * 0.02f, 0.f, 1.f);
                                if (isLie) exists->isLie = true;
                            } else {
                                o.formIdea(idea.kind, text, idea.strength * 0.55f, currentDay_, moral);
                                if (isLie) {
                                    if (Idea* mine = o.findIdea(text)) mine->isLie = true;
                                    // se a vítima tem mais moral que o mentiroso, pode pegar
                                    if (o.morality > c.morality + 0.2f && chance(0.3f)) {
                                        c.em.shame = clampf(c.em.shame + 0.4f, 0.f, 1.f);
                                        changeReputation(c, -0.1f, day, "mentira descoberta");
                                        addEvent(o.name + " percebeu a mentira de " + c.name + ".", EvKind::War);
                                    } else if (chance(0.12f)) {
                                        addEvent(o.name + " comecou a acreditar: \"" + text + "\"", EvKind::Idea);
                                    }
                                } else if (chance(0.1f)) {
                                    addEvent(o.name + " ouviu de " + c.name + ": \"" + text + "\"", EvKind::Idea);
                                }
                            }
                        }
                    }
                }
            }
            return;
        }
    }

    // JUSTIÇA: reputação ruim + comunidade + juízes morais
    if (c.communityId >= 0 && c.reputation < -0.3f) {
        Community* comm = community(c.communityId);
        if (comm && dist2(c.x, c.y, comm->x, comm->y) < comm->territoryR * comm->territoryR) {
            for (int id : comm->memberIds) {
                if (id == c.id) continue;
                Creature* judge = get(id);
                if (!judge || !judge->alive() || judge->morality < 0.55f) continue;
                if (dist2(c.x, c.y, judge->x, judge->y) > 16.f) continue;
                if (chance(0.05f * dt * judge->morality)) {
                    damage(c, 5.f, judge->id, currentDay_);
                    c.em.shame = clampf(c.em.shame + 0.4f, 0.f, 1.f);
                    addEvent(judge->name + " exigiu justica de " + c.name + ".", EvKind::War);
                    c.state = CState::Fight;
                    return;
                }
            }
            if (c.reputation < -0.65f && chance(0.015f * dt)) {
                addEvent(c.name + " foi banido de " + comm->name + ".", EvKind::War);
                c.communityId = -1;
            }
        }
    }

    // 4) NO GELO: sai correndo pro calor (não morre congelado esperando abrigo)
    if (world_.inBounds((int)c.x, (int)c.y) &&
        world_.at((int)c.x, (int)c.y).type == Tile::Ice) {
        c.state = CState::SeekShelter;
        c.energy -= 0.25f * dt; // congela
        // procura terra quente: grama, floresta, solo, areia (nunca gelo/rocha)
        float bx = c.x, by = c.y;
        float best = 1e9f;
        bool foundWarm = false;
        int r = (int)(c.g.vision + 4.f);
        int tx = (int)c.x, ty = (int)c.y;
        for (int y = ty - r; y <= ty + r; y++) {
            for (int x = tx - r; x <= tx + r; x++) {
                if (!world_.inBounds(x, y)) continue;
                Tile t = world_.at(x, y).type;
                if (t == Tile::Ice || t == Tile::Rock || t == Tile::DeepWater) continue;
                float score = 1.f;
                if (t == Tile::Forest) score = 1.5f;
                if (world_.at(x, y).cave) score = 3.f;
                float d = dist2(c.x, c.y, x + 0.5f, y + 0.5f) / score;
                if (d < best) { best = d; bx = x + 0.5f; by = y + 0.5f; foundWarm = true; }
            }
        }
        if (foundWarm) {
            moveToward(c, bx, by, dt, 1.2f);
            return;
        }
        // nada de warm no alcance: pelo menos anda em direção sul (mais quente)
        moveToward(c, c.x, c.y + 5.f, dt, 1.f);
        return;
    }

    // 4b) desconforto térmico / toca
    if (thermalStress(c) > 0.35f || (c.em.comfort < 0.3f && c.energy > 30.f)) {
        float bx, by;
        bool found = false;
        scanShelter(c, bx, by, found);
        if (found && dist2(c.x, c.y, bx, by) > 2.f) {
            c.state = CState::SeekShelter;
            moveToward(c, bx, by, dt, 0.9f);
            return;
        }
        // sem abrigo: agacha e espera
        c.state = CState::Rest;
        c.energy += 2.f * dt;
        return;
    }

    // 5) noite + fome de calor: quem não tem fogo recolhe (sono já tratado acima)
    float fireSafe = c.k.fire_knowledge * 0.7f;
    if (night && c.energy < 85.f && fireSafe < 0.5f && c.em.comfort < 0.5f) {
        c.state = CState::Rest;
        c.energy += 3.5f * dt;
        c.hydration += 1.5f * dt;
        if (c.energy > 50.f - fireSafe * 30.f && c.em.comfort > 0.35f) c.state = CState::Wander;
        return;
    }

    // 6) sede — bebe ANTES de ter sede demais, evita loop
    // fruta já deu alguma água; bebe com menos urgência se comeu bem
    float drinkThreshold = 40.f + c.em.satisfaction * 10.f;
    if (c.hydration < drinkThreshold) {
        c.state = CState::SeekWater;
        tryDrink(c, dt);
        if (c.hydration > 70.f) {
            // hidratado: agora pode comer em paz
        }
        return;
    }

    // 7) fome — come antes de morrer de fome
    float eatUrgency = c.energy < (35.f + c.pers.greed * 25.f);

    // 7b) CAÇADA: fome extrema + coragem = ataca bicho pequeno (descoberta rara)
    //     reputação baixa também abre a porta (nada a perder)
    if ((c.energy < 14.f || (c.reputation < -0.4f && c.energy < 25.f)) && eco_) {
        if (tryHuntFauna(c, dt, day)) return;
    }

    if (c.energy < 15.f) {
        c.state = CState::Rest;
        c.energy += 4.f * dt;
        if (c.energy > 32.f) c.state = CState::Wander;
        return;
    }
    if (eatUrgency || (c.energy < 55.f && c.pers.greed > 0.4f) ||
        (c.energy < 75.f && c.k.plant_value > 0.4f && c.pers.greed > 0.6f)) {
        // se tem cadáver por perto e está com medo, prefere carne morta (mais seguro)
        float cx, cy;
        bool hasCorpse = false;
        scanCorpse(c, cx, cy, hasCorpse);
        if (hasCorpse && (c.energy < 35.f || c.em.fear > 0.3f)) {
            c.state = CState::SeekFood;
            tryEatCorpse(c, dt);
            return;
        }
        c.state = CState::SeekFood;
        tryEat(c, dt);
        return;
    }

    // 8) COOPERAÇÃO DE CASAL: os dois trabalham juntos para sobreviver
    if (c.em.mateId >= 0) {
        Creature* mate = get(c.em.mateId);
        if (mate && mate->alive()) {
            float d2 = dist2(c.x, c.y, mate->x, mate->y);
            // par unido: ficar juntos é o objetivo mais alto de quem ama
            if (c.em.love > 0.25f || c.em.desireFamily > 0.3f) {
                c.state = CState::SeekMate;
                if (d2 > 2.5f) {
                    moveToward(c, mate->x, mate->y, dt, 0.75f);
                    return;
                }
                // juntos: reforça o vínculo
                c.em.comfort = clampf(c.em.comfort + 0.03f * dt, 0.f, 1.f);
                c.em.loneliness = clampf(c.em.loneliness - 0.2f * dt, 0.f, 1.f);
            }
            // compartilhar comida: um alimenta o outro
            if (d2 < 9.f) {
                if (mate->energy < 40.f && c.energy > 65.f) {
                    float give = 6.f * dt;
                    mate->energy = clampf(mate->energy + give, 0.f, 100.f);
                    c.energy -= give * 0.6f;
                    c.em.satisfaction = clampf(c.em.satisfaction + 0.03f * dt, 0.f, 1.f);
                    c.em.happiness = clampf(c.em.happiness + 0.02f * dt, 0.f, 1.f);
                    if (!c.findIdea("compartilhar"))
                        c.formIdea(IdeaKind::Love, "compartilhar alimenta a alma", 0.4f, currentDay_, 0.85f);
                    c.state = CState::Gather;
                    // se eu também tenho fome, procuro comida pros dois
                    if (c.energy < 55.f) {
                        c.state = CState::SeekFood;
                        tryEat(c, dt);
                        return;
                    }
                    return;
                }
                // ajudar o par ameaçado
                float px2, py2;
                int tid2 = -1;
                if (findThreat(*mate, px2, py2, tid2) && c.em.fear < 0.5f && c.g.aggression > 0.25f) {
                    c.state = CState::Fight;
                    moveToward(c, px2, py2, dt, 1.1f);
                    if (dist2(c.x, c.y, px2, py2) < 1.2f) {
                        // protege o par
                        if (tid2 >= 0) {
                            damage(*get(tid2), 8.f + c.g.size * 3.f, c.id, currentDay_);
                        }
                        c.em.love = clampf(c.em.love + 0.05f, 0.f, 1.f);
                        c.em.pride = clampf(c.em.pride + 0.1f, 0.f, 1.f);
                    }
                    return;
                }
            }
        }
    }

    // 9) DESEJO DE FAMÍLIA: quem quer lar procura par com mais garra
    float mateUrgency = c.pers.sexuality * (1.f - c.pers.laziness * 0.3f)
                      + c.em.desireFamily * 0.5f + c.pers.attachment * 0.3f;
    if (!c.juvenile() && c.mateCooldown <= 0.f && c.energy > 50.f &&
        c.em.loneliness > 0.05f && c.em.fear < 0.45f - c.pers.boldness * 0.2f &&
        chance(0.02f + mateUrgency * 0.1f)) {
        float bx, by;
        bool found = false;
        scanMate(c, bx, by, found);
        if (found) {
            c.state = CState::SeekMate;
            if (dist2(c.x, c.y, bx, by) > 1.1f) moveToward(c, bx, by, dt);
            return;
        }
        // quer família mas não acha par: ideia de solidão
        if (c.em.loneliness > 0.4f && chance(0.005f * dt))
            c.formIdea(IdeaKind::Self, "sozinho nao e o mesmo que livre", 0.35f, currentDay_, 0.5f);
    }

    // 10) viver: vagar, sentir o mundo, existir
    c.state = CState::Wander;
    wander(c, dt);
    socializeNearby(c, dt, day);
}

void CreatureSim::updateOne(Creature& c, float dt, unsigned day) {
    // REDE DE SEGURANÇA: pensante nunca deve estar na água nem no gelo
    if (!world_.walkable(c.x, c.y)) {
        snapPenToLand(world_, c.x, c.y);
    } else if (world_.inBounds((int)c.x, (int)c.y) &&
               world_.at((int)c.x, (int)c.y).type == Tile::Ice) {
        snapPenToLand(world_, c.x, c.y); // gelo também é pra sair
    }
    updateEmotions(c, dt, day);

    // doença: não mata — só enfraquece (usuário pediu: doenças não matam)
    if (c.sick_ > 0.f) {
        c.sick_ -= dt;
        // sem dano de energia; só saúde cai um pouco (já abaixo)
        if (c.sick_ <= 0.f) {
            addEvent(c.name + " se recuperou da doença.", EvKind::Health);
        }
    }

    c.age += dt / SECONDS_PER_DAY;
    if (c.memFoodT > 0.f) c.memFoodT -= dt;
    // corpo e mente crescem com a idade
    float ageFactor = clampf(c.age / c.g.lifespan, 0.f, 1.f);
    c.body.strength = clampf(c.body.strength + 0.003f * dt * (1.f - ageFactor * 0.5f), 0.f, 1.f);
    c.body.endurance = clampf(c.body.endurance + 0.002f * dt * (1.f - ageFactor * 0.5f), 0.f, 1.f);
    c.body.agility = clampf(c.body.agility - 0.001f * dt * ageFactor, 0.f, 1.f); // perde com idade
    c.mind.wisdom = clampf(c.mind.wisdom + 0.004f * dt, 0.f, 1.f); // sabedoria sempre cresce
    c.mind.memory = clampf(c.mind.memory + 0.001f * dt, 0.f, 1.f);
    c.mind.creativity = clampf(c.mind.creativity + 0.002f * dt * (1.f - ageFactor * 0.3f), 0.f, 1.f);
    // saúde: influenciada por doença e bem-estar
    if (c.sick_ > 0.f) c.body.health = clampf(c.body.health - 0.01f * dt, 0.f, 1.f);
    else c.body.health = clampf(c.body.health + 0.002f * dt, 0.f, 1.f);
    if (c.age > c.g.lifespan) {
        c.energy = 0.f;
        c.state = CState::Dead;
        deaths_++;
        if (chance(0.35f)) {
            std::string legacy = "";
            if (c.children > 0) legacy = " pai de " + std::to_string(c.children);
            if (c.surname.empty()) {
                addEvent(c.name + " viveu " + std::to_string((int)c.age) + " dias e partiu." + legacy, EvKind::Death);
            } else {
                addEvent(c.fullName() + " viveu " + std::to_string((int)c.age) + " dias e partiu." + legacy, EvKind::Death);
            }
        }
        return;
    }

    float drain = 0.62f * c.g.metabolism;
    // corpo resistente gasta menos
    drain *= (1.2f - c.body.endurance * 0.4f);
    // saúde baixa = gasta mais
    if (c.body.health < 0.5f) drain *= 1.3f;
    if (c.state == CState::Wander || c.state == CState::SeekFood ||
        c.state == CState::SeekWater || c.state == CState::SeekMate ||
        c.state == CState::Fight || c.state == CState::SeekShelter) {
        drain += 0.28f * c.g.speed;
    }
    if (c.state == CState::Rest || c.state == CState::Mourn) drain *= 0.45f;
    // raiva e medo consomem corpo — mas pouco (viver bem é o padrão)
    drain *= (1.f + c.em.anger * 0.10f + c.em.fear * 0.08f);
    // doente: metabolismo mais lento, gasta MENOS (não mais)
    if (c.sick_ > 0.f) drain *= 0.7f;
    c.energy -= drain * dt;
    c.hydration -= 0.35f * c.g.metabolism * dt;

    if (c.mateCooldown > 0.f) c.mateCooldown -= dt;

    if (c.energy <= 0.f) {
        c.energy = 0.f;
        c.state = CState::Dead;
        deaths_++;
        if (chance(0.4f)) addEvent(c.name + " caiu de exaustao.", EvKind::Death);
        return;
    }

    if (c.state == CState::Sick) {
        c.stateT -= dt;
        wander(c, dt * 0.4f);
        if (c.stateT <= 0.f) c.state = CState::Wander;
        return;
    }

    if (c.state == CState::Eat && c.stateT > 0.f) {
        c.stateT -= dt;
        if (c.stateT <= 0.f) c.state = CState::Wander;
        return;
    }

    behavior(c, dt, day);
}

void CreatureSim::update(float dt, unsigned day) {
    currentDay_ = day;
    teachNearby(dt);
    updateCommunities(dt, day);

    // era cultural: sobe com gerações totais
    totalGens_ = births_;
    int newEra = 0;
    if (totalGens_ > 5) newEra = 1;   // fogo
    if (totalGens_ > 15) newEra = 2;  // ferramentas
    if (totalGens_ > 40) newEra = 3;  // agricultura
    if (totalGens_ > 80) newEra = 4;  // escrita
    if (totalGens_ > 150) newEra = 5; // civilização
    if (newEra > culturalEra_) {
        culturalEra_ = newEra;
        addEvent("=== " + std::string(culturalEraName()) + " ===");
    }

    // DOENÇA: densidade alta + pouca higiene → epidemia
    for (auto& c : creatures_) {
        if (!c.alive() || c.sick_ > 0.f) continue;
        int neighbors = 0;
        for (auto& o : creatures_) {
            if (o.id == c.id || !o.alive()) continue;
            if (dist2(c.x, c.y, o.x, o.y) < 9.f) neighbors++;
        }
        // mais gente por perto = mais chance de doença
        float density = (float)neighbors / 9.f;
        float chance_ = 0.00008f * density * (1.f - c.g.robustness * 0.5f);
        if (world_.isNight()) chance_ *= 1.3f; // noite facilita
        if (chance(chance_)) {
            c.sick_ = frand(20.f, 60.f);
            addEvent(c.name + " adoeceu.", EvKind::Health);
        }
    }

    std::vector<Creature> newborns;
    for (auto& c : creatures_) {
        if (!c.alive()) continue;
        updateOne(c, dt, day);
    }
    for (auto& c : creatures_) {
        if (!c.alive()) continue;
        if (c.state == CState::Dead) continue;
        tryReproduce(c, newborns);
    }
    for (auto& nb : newborns) creatures_.push_back(nb);

    // remove mortos — carne e ossos ficam no chão; pais e AMIGOS sentem a perda
    // e o TÚMULO fica: ossos + crônica clicável
    for (auto& c : creatures_) {
        if (c.state != CState::Dead) continue;
        notifyChildDeath(c);
        // TÚMULO: guarda a linha do tempo pra sempre
        {
            Grave g;
            g.id = nextGraveId_++;
            g.x = c.x;
            g.y = c.y;
            g.name = c.name;
            g.surname = c.surname;
            g.age = c.age;
            g.male = c.male;
            g.children = c.children;
            g.communityId = c.communityId;
            g.parentA = c.parentA;
            g.parentB = c.parentB;
            g.chronicle = c.chronicle;
            g.chronicle.push_back(LifeEvent{currentDay_,
                "partiu aos " + std::to_string((int)c.age) + " dias" +
                (c.children > 0 ? " — pai de " + std::to_string(c.children) : "")});
            // ideias fortes viram última linha
            if (!c.ideas.empty()) {
                const Idea* best = &c.ideas[0];
                for (auto& i : c.ideas) if (i.strength > best->strength) best = &i;
                g.chronicle.push_back(LifeEvent{currentDay_, "crença final: \"" + best->text + "\""});
            }
            graves_.push_back(g);
            if (graves_.size() > 200) graves_.erase(graves_.begin());
        }
        // amigos sentem
        for (auto& w : creatures_) {
            if (w.id == c.id || !w.alive()) continue;
            Bond* b = w.bond(c.id);
            if (b && b->affection > 0.35f && w.em.mateId != c.id) {
                w.em.grief = clampf(w.em.grief + 0.25f, 0.f, 1.f);
                w.em.loneliness = clampf(w.em.loneliness + 0.15f, 0.f, 1.f);
                if (w.em.grief < 0.3f)
                    addEvent(w.name + " perdeu um amigo. " + c.name + " nao voltou.", EvKind::Death);
            }
        }
        int tx = (int)c.x, ty = (int)c.y;
        if (world_.inBounds(tx, ty)) {
            TileData& t = world_.at(tx, ty);
            t.corpse = (uint8_t)clampf(t.corpse + 5.f + c.g.size * 4.f, 0.f, 15.f);
            t.bones = (uint8_t)clampf(t.bones + 3.f + c.g.size * 2.f, 0.f, 15.f);
            // sementes venenosas viajam na morte: o corpo vira berçário
            if (c.seedLoad > 0.f && c.seedKind == 2) {
                if ((t.type == Tile::Grass || t.type == Tile::Forest || t.type == Tile::Soil) &&
                    t.plant.kind == PlantKind::None) {
                    t.plant.kind = PlantKind::Toxic;
                    t.plant.stage = 0;
                    t.plant.gene.animal_spread = c.seedSpreadGene;
                    t.plant.gene.toxicity = clampf(gauss(0.65f, 0.15f), 0.3f, 0.95f);
                    t.plant.gene.expansion = clampf(gauss(0.5f, 0.15f), 0.1f, 0.95f);
                    addEvent("Onde o corpo repousa, uma planta venenosa nasceu.");
                }
            } else if (c.seedLoad > 0.f && c.seedKind == 1 && chance(0.5f)) {
                if ((t.type == Tile::Grass || t.type == Tile::Forest || t.type == Tile::Soil) &&
                    t.plant.kind == PlantKind::None) {
                    t.plant.kind = PlantKind::Fruit;
                    t.plant.stage = 0;
                    t.plant.gene.animal_spread = c.seedSpreadGene;
                    t.plant.gene.expansion = clampf(gauss(0.4f, 0.12f), 0.1f, 0.9f);
                }
            }
        }
    }
    creatures_.erase(
        std::remove_if(creatures_.begin(), creatures_.end(),
                       [](const Creature& c) { return c.state == CState::Dead; }),
        creatures_.end());

    for (auto& e : events_) e.t += dt;
}

}
