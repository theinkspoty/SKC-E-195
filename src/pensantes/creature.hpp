#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "world.hpp"

namespace skc {

class Ecosystem; // fwd

// personalidade única — muda as decisões de TODO mundo
struct Personality {
    float sexuality = 0.5f;   // desejo sexual / foco em par
    float boldness = 0.5f;    // ousado vs contradito (medroso)
    float thinker = 0.5f;     // 0 age sem pensar .. 1 pensa antes
    float greed = 0.5f;       // oportunista / acumulador
    float laziness = 0.5f;    // preguiçoso vs agitado
    float empathy = 0.5f;     // sente o outro vs egoísta
    float wanderlust = 0.5f;  // ama explorar vs fica em casa
};

struct Genome {
    float speed = 1.f;
    float metabolism = 1.f;
    float size = 1.f;
    float vision = 7.f;
    float fertility = 1.f;
    float robustness = 0.5f;
    float lifespan = 10.f;
    float sociability = 0.5f;
    float aggression = 0.3f;
    float intelligence = 0.2f;
};

// o que alguém acha atraente (cada um tem as suas)
struct Preferences {
    float likeStrength = 0.5f;
    float likeSmarts = 0.5f;
    float likeSkill = 0.5f;
    float likeSize = 0.5f;
    float likeCalm = 0.5f;
    float likeYoung = 0.5f;
};

struct Knowledge {
    float plant_value = 0.5f;
    float toxic_aversion = 0.f;
    float discrimination = 0.f;
    float tool_skill = 0.f;
    float fire_knowledge = 0.f;
    float antidote = 0.f;
    float predator_knowledge = 0.f;
};

struct Emotions {
    float fear = 0.f;
    float anger = 0.f;
    float love = 0.f;
    float grief = 0.f;
    float curiosity = 0.3f;
    float comfort = 0.5f;
    float loneliness = 0.f;
    float satisfaction = 0.f;
    float guilt = 0.f;    // fez algo errado
    float shame = 0.f;    // errado na frente dos outros
    float pride = 0.f;    // fez algo certo e foi visto
    int wrathTarget = -1;
    int mateId = -1;
    float shelterX = -1.f, shelterY = -1.f;
};

struct Bond {
    int id = -1;
    float trust = 0.f;
    float affection = 0.f;
    float grudge = 0.f;
    unsigned lastContactDay = 0;
};

// simbiose: confiança com outra espécie
struct SpeciesBond {
    int faunaType = 0;
    float trust = 0.f;
    float affection = 0.f;
};

// lição que um filho recebeu
struct TeachingRecord {
    int teacherId = -1;
    std::string teacherName;
    std::string lesson;
    float value = 0.f;
};

// uma ideia / crença — nasce da experiência, molda decisões, se propaga
enum class IdeaKind : uint8_t {
    World = 0,     // "o mundo é assim"
    Social,        // "os outros são assim"
    Self,          // "eu sou assim"
    Plant,         // "plantas são..."
    Predator,      // "lobo é..."
    Love,          // "amar é..."
    Food,          // "comida é..."
    Community,     // "juntos..."
    Mystery        // "por que existe...?"
};

struct Idea {
    IdeaKind kind = IdeaKind::World;
    std::string text;
    float strength = 0.5f;
    float morality = 0.5f; // 0 ideia má/egoísta .. 0.5 neutra .. 1 ideia boa/justa
    bool isLie = false;    // foi plantada como mentira
    unsigned formedDay = 0;
    int fromId = -1;
    std::string fromName;
};

enum class CState {
    Wander,
    SeekFood,
    SeekWater,
    SeekMate,
    SeekShelter,
    Eat,
    Drink,
    Rest,
    Fight,
    Mourn,
    Investigate,
    Gather,
    Build,       // constrói a comunidade
    Sick,
    Dead
};

// comunidade fixa: a cultura ganha corpo
struct Community {
    int id = 0;
    std::string name;
    float x = 0.f, y = 0.f;
    float territoryR = 12.f;
    std::vector<int> memberIds;
    bool hasFire = false;
    bool hasHut = false;
    unsigned foundingDay = 0;
    std::vector<Idea> groupIdeas; // crenças da tribo

    int size() const { return (int)memberIds.size(); }
    bool hasMember(int id) const {
        for (int m : memberIds) if (m == id) return true;
        return false;
    }
};

std::string makeCommunityName();

const char* stateName(CState s);

struct Creature {
    int id = 0;
    std::string name;
    std::string surname;  // herdado do pai
    float x = 0.f, y = 0.f;
    bool male = true;
    float energy = 80.f;
    float hydration = 80.f;
    float age = 0.f;
    Genome g;
    Knowledge k;
    Personality pers;
    Preferences prefs;
    Emotions em;
    std::vector<Bond> bonds;
    std::vector<SpeciesBond> speciesBonds;
    std::vector<TeachingRecord> teachings;
    std::vector<Idea> ideas;
    int communityId = -1;
    float morality = 0.5f;
    float reputation = 0.f;

    void formIdea(IdeaKind kind, const std::string& text, float power, unsigned day, float morality = 0.5f);
    Idea* findIdea(const std::string& fragment);
    const Idea* findIdea(const std::string& fragment) const;
    void applyIdeas(float dt);
    CState state = CState::Wander;
    float stateT = 0.f;
    float targetX = 0.f, targetY = 0.f;
    float mateCooldown = 0.f;
    int parentA = -1, parentB = -1;
    std::vector<int> childrenIds; // filhos diretos
    int children = 0;
    unsigned bornDay = 0;
    float lastDamageFrom = -1.f;
    float sickT = 0.f;
    float seedLoad = 0.f;
    float seedSpreadGene = 0.f;
    int seedKind = 0;

    bool juvenile() const { return age < 1.5f; }
    bool alive() const { return state != CState::Dead && energy > 0.f; }
    std::string fullName() const {
        return surname.empty() ? name : name + " " + surname;
    }

    Bond* bond(int otherId) {
        for (auto& b : bonds) if (b.id == otherId) return &b;
        return nullptr;
    }
    const Bond* bond(int otherId) const {
        for (auto& b : bonds) if (b.id == otherId) return &b;
        return nullptr;
    }
    Bond& ensureBond(int otherId, unsigned day) {
        Bond* b = bond(otherId);
        if (b) { b->lastContactDay = day; return *b; }
        Bond nb;
        nb.id = otherId;
        nb.lastContactDay = day;
        bonds.push_back(nb);
        if (bonds.size() > 10) bonds.erase(bonds.begin());
        return bonds.back();
    }

    SpeciesBond* speciesBond(int type) {
        for (auto& s : speciesBonds) if (s.faunaType == type) return &s;
        return nullptr;
    }
    SpeciesBond& ensureSpeciesBond(int type) {
        SpeciesBond* s = speciesBond(type);
        if (s) return *s;
        SpeciesBond ns;
        ns.faunaType = type;
        speciesBonds.push_back(ns);
        return speciesBonds.back();
    }

    // pontuação de atração: cada um quer algo diferente
    float attractionTo(const Creature& o) const {
        float score = 0.f;
        score += prefs.likeStrength * (o.g.aggression * 0.5f + o.g.size * 0.2f);
        score += prefs.likeSmarts * (o.g.intelligence + o.g.vision * 0.05f);
        score += prefs.likeSkill * (o.k.tool_skill + o.k.fire_knowledge);
        score += prefs.likeSize * o.g.size;
        score += prefs.likeCalm * (1.f - o.g.aggression);
        score += prefs.likeYoung * (1.f - o.age / std::max(1.f, o.g.lifespan));
        score += o.em.satisfaction * 0.1f; // quem está bem atrai
        return score; // ~0..2
    }
};

std::string makeName();

struct SimEvent {
    std::string text;
    float t = 0.f;
};

class CreatureSim {
public:
    explicit CreatureSim(World& world);

    void setEcosystem(Ecosystem* eco) { eco_ = eco; }

    void spawnStarter(int count);
    void update(float dt, unsigned day);
    Creature* get(int id);
    const std::vector<Creature>& all() const { return creatures_; }
    std::vector<Creature>& all() { return creatures_; }
    int aliveCount() const;
    int deathsTotal() const { return deaths_; }
    int birthsTotal() const { return births_; }
    std::vector<SimEvent>& events() { return events_; }

    struct Stats {
        float avgSpeed = 0, avgVision = 0, avgLife = 0, avgRobust = 0;
        float avgTool = 0, avgFire = 0, avgPlantValue = 0;
        float avgFear = 0, avgAnger = 0, avgLove = 0;
        int generations = 0;
    };
    Stats stats() const;

    void killInRadius(float cx, float cy, float radius);

    void evolveFromAnimal(float x, float y, float speed, float size, float vision,
                          float metabolism, float lifespan, int parentA = -1, int parentB = -1);

    // comunidades
    std::vector<Community>& communities() { return communities_; }
    const std::vector<Community>& communities() const { return communities_; }
    Community* community(int id);
    void updateCommunities(float dt, unsigned day);

    // dano externo (predador, briga) — ativa raiva e vínculo de rancor
    void damage(Creature& c, float amount, int attackerId, unsigned day);

private:
    World& world_;
    Ecosystem* eco_ = nullptr;
    std::vector<Creature> creatures_;
    std::vector<SimEvent> events_;
    std::vector<Community> communities_;
    int nextCommunityId_ = 1;
    int nextId_ = 1;
    int deaths_ = 0;
    int births_ = 0;
    unsigned currentDay_ = 1;

    void addEvent(const std::string& s);
    void updateOne(Creature& c, float dt, unsigned day);
    void updateEmotions(Creature& c, float dt, unsigned day);
    void behavior(Creature& c, float dt, unsigned day);
    void moveToward(Creature& c, float tx, float ty, float dt, float mul = 1.f);
    void wander(Creature& c, float dt);
    bool tryEat(Creature& c, float dt);
    bool tryDrink(Creature& c, float dt);
    bool tryEatCorpse(Creature& c, float dt);
    void tryReproduce(Creature& a, std::vector<Creature>& out);
    void teachNearby(float dt);
    void socializeNearby(Creature& c, float dt, unsigned day);
    Creature makeChild(const Creature& a, const Creature& b);
    bool canSenseToxic(const Creature& c) const;
    void scanFood(const Creature& c, float& bx, float& by, float& best) const;
    void scanMate(const Creature& c, float& bx, float& by, bool& found) const;
    void scanWater(const Creature& c, float& bx, float& by, bool& found) const;
    void scanCorpse(const Creature& c, float& bx, float& by, bool& found) const;
    void scanShelter(const Creature& c, float& bx, float& by, bool& found) const;
    float thermalStress(const Creature& c) const;
    // true se há ameaça; threatId >= 0 se o algozo é outro pensante (vingança possível)
    bool findThreat(const Creature& c, float& px, float& py, int& threatId) const;
    void seekRevenge(Creature& c, float dt, unsigned day);
};

}
