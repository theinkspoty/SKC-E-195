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
    SmallBird,
    Turtle,    // tartaruga: anfíbio com casco, costa + mar
    Shark,     // tubarão: ápice do mar, evolui do piscador
    Bear,      // urso: lobo que aprendeu a pescar
    Whale,     // baleia: herbívoro que voltou ao mar
    Worm,      // verme: decompositor — acelera a morte a virar vida
    Trilobite, // trilobita: fundo do mar, ícone do Cambriano
    Insect,    // inseto: poliniza e é comida de passarinho
    Placoderm, // placodermio (Dunkleosteus): armadura de Devoniano
    Lungfish   // peixe-pulmão: ponte entre mar e terra
};
constexpr int FAUNA_TYPE_COUNT = 18;

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

// eras geológicas da vida (inspiradas na Terra real)
enum class Era : uint8_t {
    Silence = 0,    // hadeano: só rocha e água
    Microbial,      // arqueano/proterozoico: microbioma, plâncton
    Cambrian,       // EXPLOSÃO CAMBRIANA: formas novas explodem no mar
    Ocean,          // ordoviciano/siluriano: diversidade marinha
    Land,           // devoniano/carbonífero: peixes, anfíbios, florestas
    Hunt,           // pérmico/mesozoico: predação e caça
    Mind,           // mente emerge
    Culture         // fogo, ferramentas, civilização
};

const char* eraName(Era e);
const char* eraStory(Era e);
const char* eraPeriod(Era e); // nome do período geológico (tipo "Devoniano")

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
    float swim = 0.f;          // habilidade de nadar (terrestres evoluem isso perto d'água)
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
    // onde este indivíduo VIVEU de verdade (EMA 0 terra .. 1 água)
    // os filhos herdam: quem vive na água tende a virar água, e vice-versa
    float habitatUse = 0.5f;
    // GRUPO SOCIAL: rebanho, matilha, cardume (-1 = sozinho)
    int groupId = -1;
    // medo aprendido de quem me caça (por tipo)
    float fearOf[FAUNA_TYPE_COUNT] = {};
    int children = 0;
    int parentA = -1, parentB = -1;
    FaunaGenome g;

    bool alive() const { return energy > 0.f; }
    bool isPredator() const {
        return type == FaunaType::SeaPredator ||
               type == FaunaType::LandPredator ||
               type == FaunaType::SkyPredator ||
               type == FaunaType::Shark ||
               type == FaunaType::Bear;
    }
    bool canFly() const {
        return type == FaunaType::SkyPredator ||
               type == FaunaType::Insect ||
               type == FaunaType::SmallBird;
    }
    bool fullyAquatic() const {
        return type == FaunaType::Medusa || type == FaunaType::Fish ||
               type == FaunaType::SeaPredator || type == FaunaType::Shark ||
               type == FaunaType::Whale || type == FaunaType::Trilobite ||
               type == FaunaType::Placoderm;
    }
    // espécies que vivem em grupo (rebanho/matilha/cardume/enxame)
    bool gregarious() const {
        return type == FaunaType::Herbivore || type == FaunaType::LandPredator ||
               type == FaunaType::Fish || type == FaunaType::SmallBird ||
               type == FaunaType::Turtle || type == FaunaType::Bear ||
               type == FaunaType::Trilobite || type == FaunaType::Insect ||
               type == FaunaType::Worm || type == FaunaType::Placoderm ||
               type == FaunaType::Lungfish;
    }
    // decompositor: acelera a transformação de morte em nutrientes
    bool decomposer() const { return type == FaunaType::Worm || type == FaunaType::Scavenger; }
};

// GRUPO SOCIAL persistente: rebanho de herbívoros, matilha de lobos,
// cardume de peixes, bando de passarinhos. Nasce da proximidade,
// cresce com nascimentos, morre quando os membros morrem.
struct FaunaGroup {
    int id = 0;
    FaunaType type = FaunaType::Herbivore;
    float x = 0.f, y = 0.f;      // centro vivo (média dos membros)
    float territoryR = 10.f;
    std::vector<int> memberIds;
    int leaderId = -1;            // o mais velho/maior (experiência)
    unsigned foundedDay = 0;
    float cohesion = 0.5f;       // quão unido está (0 solto .. 1 coeso)
    float fear = 0.f;            // medo coletivo (fuga em bando)

    int size() const { return (int)memberIds.size(); }
    bool hasMember(int id) const {
        for (int m : memberIds) if (m == id) return true;
        return false;
    }
    const char* kindName() const {
        switch (type) {
            case FaunaType::Herbivore: return "Rebanho";
            case FaunaType::LandPredator: return "Matilha";
            case FaunaType::Fish: return "Cardume";
            case FaunaType::SmallBird: return "Bando";
            case FaunaType::Turtle: return "Colonia";
            case FaunaType::Bear: return "Jura";
            case FaunaType::Trilobite: return "Banco";
            case FaunaType::Insect: return "Enxame";
            case FaunaType::Worm: return "Colonia";
            default: return "Grupo";
        }
    }
};

struct LifeLadder {
    bool microbe = false;
    bool plankton = false;
    bool plants = false;
    bool medusa = false;
    bool fish = false;
    bool amphibian = false;
    bool turtles = false;
    bool herbivores = false;
    bool seaPred = false;
    bool sharks = false;
    bool whales = false;
    bool landPred = false;
    bool bears = false;
    bool skyPred = false;
    bool scavengers = false;
    bool worms = false;
    bool trilobites = false;
    bool insects = false;
    bool placoderms = false;
    bool lungfish = false;
    bool sages = false;

    float planktonTiles = 0.f;
    float plantDensity = 0.f;
    int medusaCount = 0;
    int fishCount = 0;
    int amphibCount = 0;
    int turtleCount = 0;
    int herbCount = 0;
    int scavCount = 0;
    int seaPredCount = 0;
    int sharkCount = 0;
    int whaleCount = 0;
    int landPredCount = 0;
    int bearCount = 0;
    int skyPredCount = 0;
    int wormCount = 0;
    int trilobiteCount = 0;
    int insectCount = 0;
    int placodermCount = 0;
    int lungfishCount = 0;
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

    // GRUPOS SOCIAIS (rebanhos, matilhas, cardumes)
    const std::vector<FaunaGroup>& groups() const { return groups_; }
    std::vector<FaunaGroup>& groups() { return groups_; }
    FaunaGroup* group(int id);
    int groupCount() const { return (int)groups_.size(); }

    int emergencesTotal() const { return emergences_; }
    const std::vector<std::string>& lore() const { return lore_; }

    Era era() const { return era_; }
    void updateEra(unsigned day);
    // pressão evolutiva: cresce com o tempo e explode no Cambriano
    float evoPressure() const { return evoPressure_; }
    bool cambrianActive() const { return cambrianActive_; }

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
    std::vector<FaunaGroup> groups_;
    std::vector<std::string> lore_;
    int nextId_ = 1;
    int nextGroupId_ = 1;
    int emergences_ = 0;
    float emergeTimer_ = 5.f;
    float sageCooldown_ = 0.f;
    unsigned lastDay_ = 1;
    Era era_ = Era::Silence;
    PopStats pop_;
    // pioneiros: cada nicho só surge UMA vez (depois, só nasce/morre)
    bool pioneerDone_[FAUNA_TYPE_COUNT] = {};
    // PRESSÃO EVOLUTIVA: quanto o ambiente empurra genes a mudar
    // cresce devagar com o tempo; dispara durante a Explosão Cambriana
    float evoPressure_ = 1.f;
    float cambrianT_ = 0.f;      // dias restantes do evento cambriano
    bool cambrianActive_ = false;
    int cambrianBurst_ = 0;      // quantos nasceram durante a explosão
    unsigned lastEraDay_ = 0;    // dia em que a era atual começou
    // gerações por tipo: quanto mais gerações, mais o tipo muda
    int typeGens_[FAUNA_TYPE_COUNT] = {};

    void loreAdd(const std::string& s);
    void tryEmergence(unsigned day);
    // EXPLOSÃO CAMBRIANA: o mar enche de formas novas de uma vez
    void triggerCambrian(unsigned day);
    void updateCambrian(float dt, unsigned day);
    // viés de alimento perto de (x,y): 0..1 para plancton/planta/carna
    void foodBiasNear(float x, float y, float& plank, float& plant, float& meat) const;
    // especiação por habitat: terrestre que vive na água (ou vice-versa) pode virar outro tipo
    void tryHabitatSpeciation(Fauna& f, unsigned day);
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
    void updateTurtle(Fauna& f, float dt);
    void updateShark(Fauna& f, float dt);
    void updateBear(Fauna& f, float dt);
    void updateWhale(Fauna& f, float dt);
    void updateWorm(Fauna& f, float dt);
    void updateTrilobite(Fauna& f, float dt);
    void updateInsect(Fauna& f, float dt);
    void updatePlacoderm(Fauna& f, float dt);
    void updateLungfish(Fauna& f, float dt);
    void huntPrey(Fauna& f, float dt, const FaunaType* preyTypes, int nPrey, float speedMul);
    // conta tiles de água (e abismo) num raio — baleia precisa de oceano de verdade
    void waterBodySize(float x, float y, int& waterTiles, int& deepTiles) const;

    void wanderIn(Fauna& f, float dt, bool waterOnly);
    void moveToward(Fauna& f, float tx, float ty, float dt, float mul, bool waterOnly);
    static bool canEnterTile(const Fauna& f, Tile t); // nado / terrestre evolve
    bool nearestOfTypes(Fauna& f, const FaunaType* types, int nTypes,
                        float& px, float& py, int& pid) const;
    bool nearestPredatorThreat(const Fauna& prey, float& px, float& py, FaunaType& ptype) const;
    void learnFear(Fauna& prey, FaunaType hunter);
    void tryReproduce(Fauna& a, std::vector<Fauna>& out);
    Fauna makeChild(const Fauna& a, const Fauna& b);
    void trySageBirth(unsigned day);
    void dropCorpseFrom(const Fauna& f, float remaining = 1.f);
    // gestão de grupos sociais
    void updateGroups(float dt, unsigned day);
    void joinGroup(Fauna& f, int groupId);
    void leaveGroup(Fauna& f);
    // comportamento de grupo: seguir centro, fuga coletiva, caça coordenada
    void groupBehavior(Fauna& f, float dt);
};

}
