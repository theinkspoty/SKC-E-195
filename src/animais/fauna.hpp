#pragma once
#include <vector>
#include <string>
#include "world.hpp"
#include "creature.hpp"

namespace skc {

// ordem = ordem evolutiva (como na Terra)
enum class FaunaType : uint8_t {
    Medusa = 0,
    Fish,
    Amphibian,
    Herbivore,
    Scavenger,
    SeaPredator,
    LandPredator,
    SkyPredator,
    SmallBird
};
constexpr int FAUNA_TYPE_COUNT = 9;

enum class FaunaState : uint8_t {
    Idle = 0,
    Forage,
    Flee,
    Mate,
    Drink,
    Rest,
    Migrate // foi procurar recursos em outro lugar
};

const char* faunaName(FaunaType t);
const char* faunaGoal(FaunaState s, FaunaType t);

enum class Era : uint8_t {
    Silence = 0,
    Microbial,
    Ocean,
    Land,
    Hunt,
    Mind,
    Culture
};

const char* eraName(Era e);
const char* eraStory(Era e);

struct FaunaGenome {
    float speed = 1.f;
    float size = 1.f;
    float vision = 5.f;
    float metabolism = 1.f;
    float lifespan = 6.f;
    float aggression = 0.3f;
    // NICHO: preferências genéticas que a evolução molda
    float dietPlankton = 0.5f; // quanto prefere plâncton
    float dietPlants = 0.5f;   // quanto prefere plantas
    float dietMeat = 0.3f;     // quanto prefere carne
    float habitatWet = 0.5f;   // prefere água
    float habitatDry = 0.5f;   // prefere terra seca
    float nocturnal = 0.3f;    // prefere caçar de noite
};

struct Fauna {
    int id = 0;
    FaunaType type = FaunaType::Fish;
    FaunaState state = FaunaState::Idle;
    float x = 0.f, y = 0.f;
    float targetX = 0.f, targetY = 0.f;
    float energy = 60.f;
    float age = 0.f;
    bool male = true;
    float mateCooldown = 0.f;
    float stateT = 0.f;
    float fear = 0.f;
    float anger = 0.f;
    float satisfaction = 0.f; // plenitude após comer / não ter fome
    float trustSages = 0.f;   // simbiose: confia em pensantes
    // medo aprendido de quem me caça (por tipo)
    float fearOf[FAUNA_TYPE_COUNT] = {0, 0, 0, 0, 0, 0};
    int children = 0;
    int parentA = -1, parentB = -1;
    FaunaGenome g;

    bool alive() const { return energy > 0.f; }
    bool isPredator() const {
        return type == FaunaType::SeaPredator ||
               type == FaunaType::LandPredator ||
               type == FaunaType::SkyPredator;
    }
    bool canFly() const { return type == FaunaType::SkyPredator; }
};

struct LifeLadder {
    bool microbe = false;
    bool plankton = false;
    bool plants = false;
    bool medusa = false;
    bool fish = false;
    bool amphibian = false;
    bool herbivores = false;
    bool seaPred = false;
    bool landPred = false;
    bool skyPred = false;
    bool scavengers = false;
    bool sages = false;

    float planktonTiles = 0.f;
    float plantDensity = 0.f;
    int medusaCount = 0;
    int fishCount = 0;
    int amphibCount = 0;
    int herbCount = 0;
    int scavCount = 0;
    int seaPredCount = 0;
    int landPredCount = 0;
    int skyPredCount = 0;
    int sageCount = 0;

    std::string nextHint() const;
};

class Ecosystem {
public:
    Ecosystem(World& world, CreatureSim& sages);

    void update(float dt, unsigned day);
    void killInRadius(float cx, float cy, float radius);
    void dropCorpse(float x, float y, float size);

    const std::vector<Fauna>& all() const { return fauna_; }
    std::vector<Fauna>& all() { return fauna_; }
    Fauna* get(int id);
    int countType(FaunaType t) const;
    LifeLadder ladder() const;

    int emergencesTotal() const { return emergences_; }
    const std::vector<std::string>& lore() const { return lore_; }

    Era era() const { return era_; }
    void updateEra(unsigned day);

    struct PopStats {
        float avgSpeed[FAUNA_TYPE_COUNT] = {};
        float avgLife[FAUNA_TYPE_COUNT] = {};
        float avgAggro[FAUNA_TYPE_COUNT] = {};
        float avgSatis[FAUNA_TYPE_COUNT] = {};
        int counts[FAUNA_TYPE_COUNT] = {};
        int totalBirths = 0;
    };
    PopStats popStats() const { return pop_; }

    static std::string goalOf(const Fauna& f);
    static std::string nicheOf(const Fauna& f); // descreve o nicho

    // predador que caça este tipo (para medo aprendido)
    static FaunaType hunterOf(FaunaType prey);

private:
    World& world_;
    CreatureSim& sages_;
    std::vector<Fauna> fauna_;
    std::vector<std::string> lore_;
    int nextId_ = 1;
    int emergences_ = 0;
    float emergeTimer_ = 5.f;
    float sageCooldown_ = 0.f;
    unsigned lastDay_ = 1;
    Era era_ = Era::Silence;
    PopStats pop_;
    // pioneiros: cada nicho só surge UMA vez (depois, só nasce/morre)
    bool pioneerDone_[FAUNA_TYPE_COUNT] = {};

    void loreAdd(const std::string& s);
    void tryEmergence(unsigned day);
    // cria o primeiro indivíduo de um nicho (abiogênese rara) — nunca mais repete
    bool spawnPioneer(FaunaType t, float x, float y);
    void spawnFauna(FaunaType t, float x, float y, int parentA = -1, int parentB = -1);

    void updateMedusa(Fauna& f, float dt);
    void updateFish(Fauna& f, float dt);
    void schoolFish(Fauna& f, float dt); // cardume: coesão + alinhamento
    void updateAmphibian(Fauna& f, float dt);
    void updateHerbivore(Fauna& f, float dt);
    void updateScavenger(Fauna& f, float dt);
    void updateSeaPredator(Fauna& f, float dt);
    void updateLandPredator(Fauna& f, float dt);
    void updateSkyPredator(Fauna& f, float dt);
    void updateSmallBird(Fauna& f, float dt);
    void schoolBirds(Fauna& f, float dt); // bando de passarinhos
    void huntPrey(Fauna& f, float dt, const FaunaType* preyTypes, int nPrey, float speedMul);

    void wanderIn(Fauna& f, float dt, bool waterOnly);
    void moveToward(Fauna& f, float tx, float ty, float dt, float mul, bool waterOnly);
    bool nearestOfTypes(Fauna& f, const FaunaType* types, int nTypes,
                        float& px, float& py, int& pid) const;
    bool nearestPredatorThreat(const Fauna& prey, float& px, float& py, FaunaType& ptype) const;
    void learnFear(Fauna& prey, FaunaType hunter);
    void tryReproduce(Fauna& a, std::vector<Fauna>& out);
    Fauna makeChild(const Fauna& a, const Fauna& b);
    void trySageBirth(unsigned day);
    void dropCorpseFrom(const Fauna& f, float remaining = 1.f);
};

}
