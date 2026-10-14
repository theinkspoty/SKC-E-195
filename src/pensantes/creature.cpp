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
            // fundam comunidade num bom lugar: perto de água e plantas
            float cx = (a.x + b.x) * 0.5f;
            float cy = (a.y + b.y) * 0.5f;
            if (!world_.nearWater(cx, cy, 6.f)) continue;
            if (world_.plantDensity((int)cx, (int)cy, 8) < 0.02f) continue;

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
            addEvent(comm.name + " nasceu. " + a.name + " e " + b.name + " fincaram a primeira fogueira.");
            return; // uma fundação por tick
        }
    }

    // construções e encontros
    for (auto& comm : communities_) {
        if (comm.memberIds.empty()) continue;
        // fogueira e toca: precisam de ferramenta em alguém
        if (!comm.hasFire || !comm.hasHut) {
            for (int id : comm.memberIds) {
                Creature* m = get(id);
                if (!m || !m->alive()) continue;
                if (m->k.tool_skill > 0.25f) {
                    if (!comm.hasFire && m->k.fire_knowledge > 0.1f && chance(0.01f * dt * 30.f)) {
                        comm.hasFire = true;
                        addEvent(comm.name + " acendeu a primeira fogueira.");
                    }
                    if (!comm.hasFire && m->k.tool_skill > 0.35f && chance(0.008f * dt * 30.f)) {
                        comm.hasHut = true;
                        addEvent(comm.name + " ergueu a primeira toca.");
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
                // puxa pra casa
                if (chance(0.3f * dt)) {
                    m->targetX = comm.x + gauss(0.f, 3.f);
                    m->targetY = comm.y + gauss(0.f, 3.f);
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
                    bool war = ma->g.aggression > 0.55f && mb->g.aggression > 0.5f && chance(0.02f * dt);
                    if (war) {
                        // conflito
                        damage(*mb, 8.f, ma->id, day);
                        addEvent("Tensao entre " + A.name + " e " + B.name + ".");
                    } else {
                        // troca de ideias entre tribos
                        b.trust = clampf(b.trust + 0.04f * dt, -1.f, 1.f);
                        if (!A.groupIdeas.empty() && chance(0.04f * dt)) {
                            const Idea& idea = A.groupIdeas[irand(0, (int)A.groupIdeas.size() - 1)];
                            // leva ideia de A para a comunidade de B
                            bool dup = false;
                            for (auto& gi : B.groupIdeas) if (gi.text == idea.text) { dup = true; break; }
                            if (!dup && B.groupIdeas.size() < 10) {
                                B.groupIdeas.push_back(idea);
                                addEvent(B.name + " recebeu uma ideia de " + A.name + ".");
                            }
                        }
                        if (!B.groupIdeas.empty() && chance(0.04f * dt)) {
                            const Idea& idea = B.groupIdeas[irand(0, (int)B.groupIdeas.size() - 1)];
                            bool dup = false;
                            for (auto& gi : A.groupIdeas) if (gi.text == idea.text) { dup = true; break; }
                            if (!dup && A.groupIdeas.size() < 10) {
                                A.groupIdeas.push_back(idea);
                                addEvent(A.name + " recebeu uma ideia de " + B.name + ".");
                            }
                        }
                        // casamento entre tribos
                        if (ma->male != mb->male && ma->mateCooldown <= 0.f && mb->mateCooldown <= 0.f &&
                            ma->energy > 60.f && mb->energy > 60.f && chance(0.01f * dt)) {
                            ma->em.mateId = mb->id;
                            mb->em.mateId = ma->id;
                            ma->communityId = B.id; // se muda pra comunidade do par
                            addEvent(ma->name + " se juntou a " + B.name + " por amor a " + mb->name + ".");
                        }
                    }
                    return; // um encontro por tick
                }
            }
        }
    }
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
    ideas.push_back(idea);
    if (ideas.size() > 12) {
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

    if (const Idea* i = findIdea("lobo")) {
        if (i->text.find("inimigo") != std::string::npos || i->text.find("perigo") != std::string::npos)
            em.fear = clampf(em.fear + i->strength * 0.03f * dt, 0.f, 1.f);
    }
    if (const Idea* i = findIdea("juntos")) {
        em.loneliness = clampf(em.loneliness - i->strength * 0.06f * dt, 0.f, 1.f);
    }
    if (const Idea* i = findIdea("amar")) {
        if (i->text.find("doe") != std::string::npos || i->text.find("machuca") != std::string::npos)
            em.love = clampf(em.love - i->strength * 0.02f * dt, 0.f, 1.f);
        else
            em.love = clampf(em.love + i->strength * 0.02f * dt, 0.f, 1.f);
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
}

CreatureSim::CreatureSim(World& world) : world_(world) {}

void CreatureSim::addEvent(const std::string& s) {
    events_.push_back(SimEvent{s, 0.f});
    if (events_.size() > 40) events_.erase(events_.begin());
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
        c.g.aggression = clampf(gauss(0.3f, 0.1f), 0.05f, 0.9f);
        c.g.intelligence = clampf(gauss(0.25f, 0.1f), 0.05f, 0.7f);
        c.morality = clampf(0.5f + (c.pers.empathy - 0.5f) * 0.5f - (c.g.aggression - 0.3f) * 0.3f, 0.1f, 0.9f);
        // personalidade: cada um é UM
        c.pers.sexuality = clampf(gauss(0.5f, 0.28f), 0.f, 1.f);
        c.pers.boldness = clampf(gauss(0.5f, 0.28f), 0.f, 1.f);
        c.pers.thinker = clampf(gauss(0.5f, 0.28f), 0.f, 1.f);
        c.pers.greed = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.pers.laziness = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.pers.empathy = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.pers.wanderlust = clampf(gauss(0.5f, 0.28f), 0.f, 1.f);
        // cada um quer um tipo diferente de par
        c.prefs.likeStrength = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.prefs.likeSmarts = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.prefs.likeSkill = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.prefs.likeSize = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.prefs.likeCalm = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        c.prefs.likeYoung = clampf(gauss(0.5f, 0.25f), 0.f, 1.f);
        creatures_.push_back(c);
        made++;
    }
    addEvent("As primeiras criaturas despertaram no mundo.");
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
                atk->reputation = clampf(atk->reputation - 0.15f, -1.f, 1.f);
                if (chance(0.3f * atk->morality)) {
                    atk->formIdea(IdeaKind::Self, "machucar doeu em ambos", 0.35f, day, 0.65f);
                }
            } else {
                // impunidade: sem testemunhas fortes, reputação quase não cai
                atk->reputation = clampf(atk->reputation - 0.03f, -1.f, 1.f);
            }
            // a vítima espalha o que aconteceu (reputação do agressor)
            if (chance(0.4f)) {
                for (auto& w : creatures_) {
                    if (w.id == c.id || w.id == attackerId || !w.alive()) continue;
                    if (dist2(c.x, c.y, w.x, w.y) > 40.f) continue;
                    Bond& wb = w.ensureBond(attackerId, day);
                    wb.trust = clampf(wb.trust - 0.3f, -1.f, 1.f);
                    wb.grudge = clampf(wb.grudge + 0.2f, 0.f, 1.f);
                    atk->reputation = clampf(atk->reputation - 0.08f, -1.f, 1.f);
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
    e.fear = clampf(e.fear - 0.06f * dt, 0.f, 1.f);
    e.anger = clampf(e.anger - 0.04f * dt, 0.f, 1.f);
    e.grief = clampf(e.grief - 0.02f * dt, 0.f, 1.f);
    e.loneliness = clampf(e.loneliness + 0.015f * dt, 0.f, 1.f);

    // conforto térmico: o corpo sente o mundo
    float stress = thermalStress(c);
    e.comfort = lerpf(e.comfort, 1.f - stress, clampf(2.f * dt, 0.f, 1.f));
    if (stress > 0.4f) {
        // desconforto rouba energia
        c.energy -= stress * 0.35f * dt;
        if (stress > 0.55f) e.fear = clampf(e.fear + 0.08f * dt, 0.f, 1.f);
    }

    // medo: predador por perto (animais ou pensantes hostis)
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
            e.wrathTarget = -1; // descanse
            e.grief = clampf(e.grief + 0.15f, 0.f, 1.f); // ou luto pela perda da "oportunidade"
        } else {
            float d = dist2(c.x, c.y, enemy->x, enemy->y);
            if (d < 100.f) e.anger = clampf(e.anger + 0.2f * dt, 0.f, 1.f);
        }
    }

    // amor / luto pelo par
    if (e.mateId >= 0) {
        Creature* mate = get(e.mateId);
        if (!mate || !mate->alive()) {
            if (e.grief < 0.2f) {
                addEvent(c.name + " sente a falta de quem amava.");
                c.formIdea(IdeaKind::Love, "amar pode doer", 0.45f, day, 0.5f);
            }
            e.grief = clampf(e.grief + 0.1f, 0.f, 1.f);
            e.love = clampf(e.love - 0.15f * dt, 0.f, 1.f);
            Bond* b = c.bond(e.mateId);
            if (b) b->grudge = clampf(b->grudge + 0.02f * dt, 0.f, 1.f); // rancor do mundo
            if (e.grief > 0.9f && chance(0.002f)) e.mateId = -1;
        } else {
            float d = dist2(c.x, c.y, mate->x, mate->y);
            if (d < 25.f) {
                e.love = clampf(e.love + 0.12f * dt, 0.f, 1.f);
                e.loneliness = clampf(e.loneliness - 0.2f * dt, 0.f, 1.f);
                Bond& b = c.ensureBond(mate->id, day);
                b.trust = clampf(b.trust + 0.05f * dt, -1.f, 1.f);
                b.affection = clampf(b.affection + 0.08f * dt, -1.f, 1.f);
            }
        }
    }

    // solidão: pensantes são sociais demais para viver sozinhos
    int friends = 0;
    for (auto& o : creatures_) {
        if (o.id == c.id || !o.alive()) continue;
        Bond* b = c.bond(o.id);
        if (b && b->affection > 0.2f) friends++;
        else if (dist2(c.x, c.y, o.x, o.y) < 16.f) friends++;
    }
    if (friends == 0) e.loneliness = clampf(e.loneliness + 0.04f * dt, 0.f, 1.f);
    else e.loneliness = clampf(e.loneliness - 0.08f * dt, 0.f, 1.f);

    // curiosidade flutua com o dia
    e.curiosity = clampf(0.25f + 0.25f * world_.lightLevel() + 0.1f * (1.f - e.fear), 0.f, 1.f);
}

bool CreatureSim::findThreat(const Creature& c, float& px, float& py, int& threatId) const {
    threatId = -1;
    float bd = 1e9f;
    bool found = false;

    for (const auto& o : creatures_) {
        if (o.id == c.id || !o.alive()) continue;
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
            if (t == Tile::Forest) score = 1.f;
            else if (t == Tile::Rock) score = 0.9f; // toca / caverna na rocha
            else if (t == Tile::Soil) score = 0.25f;
            if (score < 0.f) continue;
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
        c.state = CState::Eat;
        c.stateT = 0.8f;
        return true;
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
    c.g.intelligence = clampf(gauss(0.4f, 0.12f), 0.15f, 0.9f); // animais que viram pensantes já eram espertos
    // pensantes nascem com curiosidade
    c.k.plant_value = clampf(gauss(0.7f, 0.08f), 0.4f, 0.95f);
    c.k.discrimination = clampf(gauss(0.2f, 0.05f), 0.f, 0.5f);
    c.parentA = parentA;
    c.parentB = parentB;
    c.state = CState::Wander;
    c.stateT = 1.f;
    c.bornDay = currentDay_;
    creatures_.push_back(c);
    births_++;
    addEvent("Um animal se ergueu: " + c.name + " nasceu pensante.");
}

bool CreatureSim::canSenseToxic(const Creature& c) const {
    return c.k.discrimination > frand(0.55f, 1.05f);
}

void CreatureSim::scanFood(const Creature& c, float& bx, float& by, float& best) const {
    best = -1e9f;
    bx = by = 0.f;
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
    for (const auto& o : creatures_) {
        if (o.id == c.id || !o.alive()) continue;
        if (o.male == c.male) continue;
        if (o.juvenile() || o.mateCooldown > 0.f) continue;
        if (o.energy < 50.f) continue;
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
        c.targetX = clampf(c.x + gauss(0.f, range), 1.f, (float)world_.width() - 2.f);
        c.targetY = clampf(c.y + gauss(0.f, range), 1.f, (float)world_.height() - 2.f);
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
        // sementes no corpo: a planta usa o animal como carro-chefe
        c.seedLoad = clampf(c.seedLoad + 1.f + c.pers.greed * 2.f, 0.f, 10.f);
        c.seedKind = (kind == PlantKind::Fruit) ? 1 : 2;
        c.seedSpreadGene = geneSpread;
        if (c.k.tool_skill > 0.3f) c.energy = clampf(c.energy + 4.f * c.k.tool_skill, 0.f, 100.f);
        if (c.k.fire_knowledge > 0.4f) c.energy = clampf(c.energy + 6.f * c.k.fire_knowledge, 0.f, 100.f);
        c.k.plant_value = clampf(c.k.plant_value + 0.07f, 0.f, 1.f);
        c.k.discrimination = clampf(c.k.discrimination + 0.02f, 0.f, 1.f);
        // planta-cura: aprende antídoto e se cura
        if (geneCure > 0.25f) {
            c.k.antidote = clampf(c.k.antidote + geneCure * 0.25f, 0.f, 1.f);
            if (c.sickT > 0.f) {
            c.sickT = clampf(c.sickT - geneCure * 3.f, 0.f, 99.f);
                c.energy = clampf(c.energy + 10.f * geneCure, 0.f, 100.f);
                addEvent(c.name + " achou o antídoto e se curou.");
                c.formIdea(IdeaKind::Plant, "existe cura para o veneno", 0.55f + geneCure * 0.3f, currentDay_, 0.85f);
                c.formIdea(IdeaKind::Mystery, "as plantas escondem respostas", 0.3f, currentDay_, 0.6f);
                c.state = CState::Wander;
            }
        }
        c.em.satisfaction = clampf(c.em.satisfaction + 0.35f, 0.f, 1.f);
        if (c.age > 2.f && c.k.discrimination > 0.3f && c.k.tool_skill < 1.f && chance(0.012f)) {
            c.k.tool_skill = clampf(c.k.tool_skill + 0.15f, 0.f, 1.f);
            addEvent(c.name + " aprendeu a usar um galho como ferramenta.");
            c.formIdea(IdeaKind::Self, "minhas maos podem mudar o mundo", 0.5f, currentDay_, 0.7f);
            c.formIdea(IdeaKind::World, "os galhos servem para algo", 0.45f, currentDay_, 0.55f);
        }
        if (c.k.tool_skill > 0.4f && c.k.fire_knowledge < 1.f && chance(0.006f)) {
            c.k.fire_knowledge = clampf(c.k.fire_knowledge + 0.2f, 0.f, 1.f);
            addEvent(c.name + " trouxe luz para a noite: fogo.");
            c.formIdea(IdeaKind::World, "a luz afasta o medo", 0.6f, currentDay_, 0.8f);
            c.formIdea(IdeaKind::Self, "eu posso trazer calor", 0.5f, currentDay_, 0.75f);
        }
        c.state = CState::Wander;
        if (chance(0.08f)) addEvent(c.name + " encontrou comida boa.");
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
            addEvent(c.name + " errou e aprendeu com a dor.");
        } else {
            addEvent(c.name + " nao aprendeu a tempo.");
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
        c.state = CState::Wander;
        return false;
    }
    float d = dist2(c.x, c.y, bx, by);
    if (d > 1.1f) {
        moveToward(c, bx, by, dt);
        return false;
    }
    c.hydration = clampf(c.hydration + 35.f * dt, 0.f, 100.f);
    c.state = CState::Drink;
    if (c.hydration > 85.f) c.state = CState::Wander;
    return true;
}

void CreatureSim::tryReproduce(Creature& a, std::vector<Creature>& out) {
    if (a.juvenile() || a.mateCooldown > 0.f || a.energy < 55.f) return;
    for (auto& b : creatures_) {
        if (b.id == a.id || !b.alive()) continue;
        if (b.male == a.male) continue;
        if (b.juvenile() || b.mateCooldown > 0.f || b.energy < 55.f) continue;
        if (dist2(a.x, a.y, b.x, b.y) > 1.44f) continue;
        float p = 0.2f * a.g.fertility * b.g.fertility;
        if (!chance(p)) continue;

        a.energy -= 25.f;
        b.energy -= 25.f;
        a.mateCooldown = 22.f;
        b.mateCooldown = 22.f;
        a.children++;
        b.children++;
        // amor: encontraram alguém para não estar sozinho
        a.em.mateId = b.id;
        b.em.mateId = a.id;
        a.em.love = clampf(a.em.love + 0.3f, 0.f, 1.f);
        b.em.love = clampf(b.em.love + 0.3f, 0.f, 1.f);
        a.em.loneliness = 0.f;
        b.em.loneliness = 0.f;
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
        addEvent(child.fullName() + " nasceu de " + mom.fullName() + " e " + dad.fullName() + " — amor virou vida.");
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
    // preferências: meio dos pais + mistério (nem filho é igual ao pai)
    c.prefs.likeStrength = clampf(gauss((a.prefs.likeStrength + b.prefs.likeStrength) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeSmarts = clampf(gauss((a.prefs.likeSmarts + b.prefs.likeSmarts) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeSkill = clampf(gauss((a.prefs.likeSkill + b.prefs.likeSkill) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeSize = clampf(gauss((a.prefs.likeSize + b.prefs.likeSize) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeCalm = clampf(gauss((a.prefs.likeCalm + b.prefs.likeCalm) * 0.5f, 0.15f), 0.f, 1.f);
    c.prefs.likeYoung = clampf(gauss((a.prefs.likeYoung + b.prefs.likeYoung) * 0.5f, 0.15f), 0.f, 1.f);

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
            }
        }
    };
    inherit(a);
    inherit(b);
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
                }
            }
        }
    }

    c.parentA = a.id;
    c.parentB = b.id;
    c.state = CState::Wander;
    c.stateT = 1.f;
    c.bornDay = currentDay_;
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
            addEvent(c.name + " desabafou a raiva em " + enemy->name + ". Rancor nasceu.");
        } else {
            addEvent(c.name + " vingou-se. O inimigo nao se levantou.");
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
            b.trust = clampf(first, -0.3f, 0.4f);
        }

        // convivência pacífica constrói confiança
        if (c.em.anger < 0.2f && o.em.anger < 0.2f) {
            float w = 0.03f * dt * (0.5f + c.g.sociability);
            b.trust = clampf(b.trust + w, -1.f, 1.f);
            if (b.trust > 0.3f) b.affection = clampf(b.affection + w * 0.5f, -1.f, 1.f);
            o.em.loneliness = clampf(o.em.loneliness - 0.1f * dt, 0.f, 1.f);
            c.em.loneliness = clampf(c.em.loneliness - 0.1f * dt, 0.f, 1.f);

            // COMO ESTRANHOS CONVERTEM: confiança + empatia + ideia forte = proselitismo
                    if (b.trust > 0.35f && !c.ideas.empty() && chance(0.015f * dt * 30.f * c.pers.empathy * (0.3f + c.g.intelligence))) {
                        const Idea& idea = c.ideas[irand(0, (int)c.ideas.size() - 1)];
                        if (idea.strength > 0.35f) {
                            if (Idea* theirs = o.findIdea(idea.text)) {
                                theirs->strength = clampf(theirs->strength + idea.strength * 0.04f, 0.f, 1.f);
                            } else {
                                o.formIdea(idea.kind, idea.text, idea.strength * 0.4f, day);
                                if (chance(0.25f))
                                    addEvent(o.name + " comecou a acreditar: \"" + idea.text + "\"");
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

    // 1) luto profundo: para de viver por um tempo
    if (c.em.grief > 0.65f) {
        c.state = CState::Mourn;
        c.energy += 1.5f * dt;
        wander(c, dt * 0.25f);
        if (c.em.grief < 0.4f) c.state = CState::Wander;
        return;
    }

    // 2) vingança: a raiva é mais forte que a fome
    if (c.em.anger > 0.45f && c.em.wrathTarget >= 0 && c.energy > 25.f) {
        seekRevenge(c, dt, day);
        return;
    }

    // 3) terror
    if (c.em.fear > 0.6f) {
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
    float invChance = 0.006f * c.em.curiosity * (0.4f + c.pers.thinker) * (0.5f + c.pers.wanderlust * 0.5f);
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
                // chegou — estuda
                if (kind == 1) {
                    c.k.predator_knowledge = clampf(c.k.predator_knowledge + 0.06f, 0.f, 1.f);
                    c.g.intelligence = clampf(c.g.intelligence + 0.02f, 0.f, 1.f);
                    c.formIdea(IdeaKind::Predator, "a morte tem um motivo", 0.4f, currentDay_, 0.45f);
                    if (chance(0.15f)) addEvent(c.name + " observou uma morte e entendeu o perigo.");
                } else if (kind == 2) {
                    c.k.antidote = clampf(c.k.antidote + 0.05f, 0.f, 1.f);
                    c.g.intelligence = clampf(c.g.intelligence + 0.015f, 0.f, 1.f);
                    c.formIdea(IdeaKind::Plant, "as plantas escondem respostas", 0.4f, currentDay_, 0.6f);
                    if (chance(0.2f)) addEvent(c.name + " estudou a planta-cura e aprendeu onde ela nasce.");
                } else {
                    c.em.curiosity = clampf(c.em.curiosity + 0.05f, 0.f, 1.f);
                    c.g.intelligence = clampf(c.g.intelligence + 0.01f, 0.f, 1.f);
                    if (chance(0.25f))
                        c.formIdea(IdeaKind::Mystery, "por que existe isso?", 0.35f + c.pers.thinker * 0.3f, currentDay_, 0.55f);
                    if (chance(0.08f)) {
                        addEvent(c.name + " olhou a fruta e perguntou: por que existe isso?");
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
                    if (c.k.tool_skill > 0.25f && !comm->hasFire && c.k.fire_knowledge > 0.1f &&
                        chance(0.02f * dt)) {
                        comm->hasFire = true;
                        addEvent(c.name + " acendeu fogueira em " + comm->name + ".");
                        c.state = CState::Build;
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
                                        c.reputation = clampf(c.reputation - 0.12f, -1.f, 1.f);
                                        addEvent(o.name + " percebeu a mentira de " + c.name + ".");
                                    } else if (chance(0.12f)) {
                                        addEvent(o.name + " comecou a acreditar: \"" + text + "\"");
                                    }
                                } else if (chance(0.1f)) {
                                    addEvent(o.name + " ouviu de " + c.name + ": \"" + text + "\"");
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
                    addEvent(judge->name + " exigiu justica de " + c.name + ".");
                    c.state = CState::Fight;
                    return;
                }
            }
            if (c.reputation < -0.65f && chance(0.015f * dt)) {
                addEvent(c.name + " foi banido de " + comm->name + ".");
                c.communityId = -1;
            }
        }
    }

    // 4) desconforto térmico / toca
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

    // 5) noite + fome de calor: quem não tem fogo recolhe
    bool night = world_.isNight();
    float fireSafe = c.k.fire_knowledge * 0.7f;
    if (night && c.energy < 85.f && fireSafe < 0.5f && c.em.comfort < 0.5f) {
        c.state = CState::Rest;
        c.energy += 3.5f * dt;
        c.hydration += 1.5f * dt;
        if (c.energy > 50.f - fireSafe * 30.f && c.em.comfort > 0.35f) c.state = CState::Wander;
        return;
    }

    // 6) sede
    if (c.hydration < 28.f) {
        c.state = CState::SeekWater;
        tryDrink(c, dt);
        return;
    }

    // 7) fome — ganancioso come mais cedo; contradito só come se quase morrendo
    float eatUrgency = c.energy < (30.f + c.pers.greed * 25.f);
    if (c.energy < 15.f) {
        c.state = CState::Rest;
        c.energy += 4.f * dt;
        if (c.energy > 32.f) c.state = CState::Wander;
        return;
    }
    if (eatUrgency || (c.energy < 45.f && c.pers.greed > 0.4f) ||
        (c.energy < 70.f && c.k.plant_value > 0.4f && c.pers.greed > 0.6f)) {
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

    // 8) amor: ficar perto do par é o objetivo mais alto de quem ama
    if (c.em.mateId >= 0 && c.em.love > 0.3f) {
        Creature* mate = get(c.em.mateId);
        if (mate && mate->alive()) {
            c.state = CState::SeekMate;
            if (dist2(c.x, c.y, mate->x, mate->y) > 2.f) {
                moveToward(c, mate->x, mate->y, dt, 0.7f);
            }
            return;
        }
    }

    // 9) acaslar — gente com mais sexualidade busca par mais; contraditos esperam
    float mateUrgency = c.pers.sexuality * (1.f - c.pers.laziness * 0.3f);
    if (!c.juvenile() && c.mateCooldown <= 0.f && c.energy > 55.f &&
        c.em.loneliness > 0.1f * (1.f - mateUrgency) && c.em.fear < 0.45f - c.pers.boldness * 0.2f &&
        chance(0.02f + mateUrgency * 0.08f)) {
        float bx, by;
        bool found = false;
        scanMate(c, bx, by, found);
        if (found) {
            c.state = CState::SeekMate;
            if (dist2(c.x, c.y, bx, by) > 1.1f) moveToward(c, bx, by, dt);
            return;
        }
    }

    // 10) viver: vagar, sentir o mundo, existir
    c.state = CState::Wander;
    wander(c, dt);
    socializeNearby(c, dt, day);
}

void CreatureSim::updateOne(Creature& c, float dt, unsigned day) {
    updateEmotions(c, dt, day);

    c.age += dt / SECONDS_PER_DAY;
    if (c.age > c.g.lifespan) {
        c.energy = 0.f;
        c.state = CState::Dead;
        deaths_++;
        if (chance(0.35f)) addEvent(c.name + " viveu " + std::to_string((int)c.age) + " dias e partiu.");
        return;
    }

    float drain = 0.62f * c.g.metabolism;
    if (c.state == CState::Wander || c.state == CState::SeekFood ||
        c.state == CState::SeekWater || c.state == CState::SeekMate ||
        c.state == CState::Fight || c.state == CState::SeekShelter) {
        drain += 0.28f * c.g.speed;
    }
    if (c.state == CState::Rest || c.state == CState::Mourn) drain *= 0.45f;
    // raiva e medo consomem corpo
    drain *= (1.f + c.em.anger * 0.25f + c.em.fear * 0.2f);
    c.energy -= drain * dt;
    c.hydration -= 0.45f * c.g.metabolism * dt;

    if (c.mateCooldown > 0.f) c.mateCooldown -= dt;

    if (c.energy <= 0.f) {
        c.energy = 0.f;
        c.state = CState::Dead;
        deaths_++;
        if (chance(0.4f)) addEvent(c.name + " caiu de exaustao.");
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

    // remove mortos — carne e ossos ficam no chão
    for (auto& c : creatures_) {
        if (c.state != CState::Dead) continue;
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
