#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "world.hpp"

namespace skc {

class Ecosystem; // fwd

// personalidade única — muda as decisões de TODO mundo
struct Personality {
    float sexuality = 0.5f;
    float boldness = 0.5f;
    float thinker = 0.5f;
    float greed = 0.5f;
    float laziness = 0.5f;
    float empathy = 0.5f;
    float wanderlust = 0.5f;
    float attachment = 0.5f; // necessidade de vínculo / família
    float optimism = 0.5f;   // tendência ao bem-estar (recupera mais rápido)
};

// corpo: o físico do pensante
struct Body {
    float strength = 0.5f;    // força bruta
    float endurance = 0.5f;   // resistência (aguenta mais fome/sede)
    float agility = 0.5f;     // agilidade (foge melhor)
    float health = 1.f;       // saúde 0..1
};

// mente: a psique do pensante
struct Mind {
    float wisdom = 0.2f;      // sabedoria (sobe com idade + experiência)
    float memory = 0.3f;      // memória (retém ideias melhor)
    float creativity = 0.3f;  // criatividade (gera ideias novas)
    float willpower = 0.5f;   // força de vontade (resiste a medo/raiva)
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
    // preferências específicas — cada fêmea é ÚNICA
    float likeHair = 0.5f;       // "cabelo" (porte)
    float likeVoice = 0.5f;      // voz forte (agressividade moderada)
    float likeGenerosity = 0.5f; // generosidade (baixa ganância)
    float likeCourage = 0.5f;    // coragem (ousadia)
    float likeLoyalty = 0.5f;    // lealdade (moral alta)
    float likeHumor = 0.5f;      // senso de humor (curiosidade + empatia)
    float likeWealth = 0.5f;     // riqueza (energia + ferramentas)
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
    float satisfaction = 0.f; // prazer: comer, amar, aprender, criar
    float guilt = 0.f;    // fez algo errado
    float shame = 0.f;    // errado na frente dos outros
    float pride = 0.f;    // fez algo certo e foi visto
    // ---- novos: economia de bem-estar ----
    float happiness = 0.5f;     // felicidade geral (filhotes, amor, conforto)
    float security = 0.5f;      // se sente seguro (casa, par, comunidade, fogo)
    float desireFamily = 0.f;   // vontade de constituir família
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
    bool selfTaught = true; // true = descoberta própria (experiência direta)

    bool isOwnDiscovery() const { return selfTaught && fromId < 0 && fromName.empty(); }
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

// construção: cada uma tem nível e tipo
struct Building {
    int type = 0;     // 0 fogueira, 1 toca, 2 casa, 3 celeiro, 4 totem, 5 muralha, 6 templo
    float x = 0.f, y = 0.f;
    int level = 1;    // 1 básico .. 3 avançado
    int hp = 10;      // integridade
};

const char* buildingName(int type);
const char* buildingDesc(int type);

// comunidade fixa: a cultura ganha corpo
struct Community {
    int id = 0;
    std::string name;
    float x = 0.f, y = 0.f;
    float territoryR = 12.f;
    std::vector<int> memberIds;
    unsigned foundingDay = 0;
    std::vector<Idea> groupIdeas;
    std::vector<Building> buildings; // construções da comunidade

    // helpers
    bool hasFire() const { return hasBuilding(0); }
    bool hasHut() const { return hasBuilding(1); }
    bool hasBuilding(int type) const {
        for (auto& b : buildings) if (b.type == type) return true;
        return false;
    }
    int buildingCount(int type) const {
        int n = 0;
        for (auto& b : buildings) if (b.type == type) n++;
        return n;
    }
    int totalBuildings() const { return (int)buildings.size(); }

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
    std::string surname;
    float x = 0.f, y = 0.f;
    bool male = true;
    float energy = 80.f;
    float hydration = 80.f;
    float age = 0.f;
    Genome g;
    Knowledge k;
    Personality pers;
    Body body;
    Mind mind;
    Preferences prefs;
    Emotions em;
    std::vector<Bond> bonds;
    std::vector<SpeciesBond> speciesBonds;
    std::vector<TeachingRecord> teachings;
    std::vector<Idea> ideas;
    int communityId = -1;
    float morality = 0.5f;
    float reputation = 0.f;
    int culturalLevel = 0; // 0 primitivo .. avança com gerações

    // ferramenta física (0 nenhuma, 1 faca, 2 cesto, 3 arma, 4 enxada)
    int tool = 0;
    // doença
    float sick_ = 0.f;
    // escrita: ideias registradas em pedra (sobrevivem à morte)
    std::vector<Idea> writtenIdeas;

    void formIdea(IdeaKind kind, const std::string& text, float power, unsigned day, float morality = 0.5f);
    Idea* findIdea(const std::string& fragment);
    const Idea* findIdea(const std::string& fragment) const;
    void applyIdeas(float dt);
    // contagem auxiliar (preenchida pelo sim quando relevante)
    int livingChildren = 0;    // filhos vivos conhecidos
    int totalChildrenBorn = 0; // nascidos (para detectar perda)
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
        score += prefs.likeStrength * (o.g.aggression * 0.4f + o.g.size * 0.3f);
        score += prefs.likeSmarts * (o.g.intelligence + o.g.vision * 0.05f);
        score += prefs.likeSkill * (o.k.tool_skill + o.k.fire_knowledge);
        score += prefs.likeSize * o.g.size;
        score += prefs.likeCalm * (1.f - o.g.aggression);
        score += prefs.likeYoung * (1.f - o.age / std::max(1.f, o.g.lifespan));
        score += prefs.likeHair * (o.g.size * 0.3f); // maior = mais "cabelo"
        score += prefs.likeVoice * (o.g.aggression * 0.3f);
        score += prefs.likeGenerosity * (1.f - o.pers.greed);
        score += prefs.likeCourage * o.pers.boldness;
        score += prefs.likeLoyalty * o.morality;
        score += prefs.likeHumor * (o.em.curiosity * 0.5f + o.pers.empathy * 0.5f);
        score += prefs.likeWealth * (o.energy / 100.f + o.tool * 0.15f);
        score += o.em.satisfaction * 0.1f;
        return score / 7.f; // normaliza
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

    // nível cultural da civilização (sobe com gerações)
    int culturalEra() const { return culturalEra_; }
    const char* culturalEraName() const;
    int totalGenerations() const { return totalGens_; }

    void damage(Creature& c, float amount, int attackerId, unsigned day);
    // prazer de aprender (reforço positivo)
    void learnPleasure(Creature& c, float amount);
    // detecta morte de filhos e avisa os pais (luto)
    void notifyChildDeath(const Creature& child);

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
    int totalGens_ = 0;    // gerações totais de pensantes
    int culturalEra_ = 0;  // era cultural global

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
