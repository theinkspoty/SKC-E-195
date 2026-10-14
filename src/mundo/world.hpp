#pragma once
#include <vector>
#include <cstdint>

namespace skc {

constexpr int TILE_PX = 8;
// 1 dia = 3 minutos reais em velocidade 1x — tempo de a vida se desenvolver
constexpr float SECONDS_PER_DAY = 180.f;

enum class Tile : uint8_t {
    DeepWater = 0,
    Water,
    Sand,
    Grass,
    Forest,
    Rock,
    Ice,
    Soil,
    COUNT
};

enum class PlantKind : uint8_t {
    None = 0,
    Fruit,
    Toxic
};

// genoma da planta — cada uma tem estratégia de conquista
struct PlantGene {
    float growth = 1.f;
    float yield = 1.f;
    float toxicity = 0.3f;
    float hardiness = 1.f;
    float cure = 0.f;
    // estratégia de espalhar: veneno via morte vs fruta via animais vs invasão
    float animal_spread = 0.3f; // quão bem o animal leva a semente
    float expansion = 0.3f;     // invade território do vizinho
    float spore_range = 0.3f;   // sementes leves (fica perto mas muitas)
};

struct Plant {
    PlantKind kind = PlantKind::None;
    uint8_t stage = 0; // 0 broto, 1 crescendo, 2 maduro
    PlantGene gene;
};

struct TileData {
    Tile type = Tile::Grass;
    float moisture = 0.5f;
    float fertility = 0.5f;
    float fert_bonus = 0.f;
    float nutrients = 0.35f;
    uint8_t biofilm = 0;
    uint8_t plankton = 0;
    uint8_t corpse = 0; // carne fresca 0-15
    uint8_t bones = 0;  // ossos 0-15 (decai bem mais devagar)
    Plant plant;
};

class World {
public:
    World(int w, int h);

    void generate(unsigned seed);
    void update();

    bool inBounds(int x, int y) const;
    TileData& at(int x, int y);
    const TileData& at(int x, int y) const;

    int width() const { return w_; }
    int height() const { return h_; }

    float globalHumidity() const;
    float averageMoisture(int cx, int cy, int radius) const;

    // ciclo dia/noite: timeOfDay 0..1 (0 = amanhecer, 0.25 = meio-dia, 0.5 = entardecer, 0.75 = noite)
    void setTimeOfDay(float t) { timeOfDay_ = t; }
    float timeOfDay() const { return timeOfDay_; }
    float lightLevel() const;
    bool isNight() const { return lightLevel() < 0.35f; }
    const char* phaseName() const;
    int currentDay() const { return day_; }
    void setDay(int d) { day_ = d; advanceSeason(d); }

    // média do genoma das plantas (evolução visível)
    PlantGene averagePlantGene() const;
    int plantCount() const;
    int curePlantCount() const;

    void paint(int cx, int cy, int radius, Tile type);
    bool spawnPlant(int x, int y, PlantKind kind, int stage = 0);
    bool walkable(float x, float y) const;
    bool nearWater(float x, float y, float radius) const;

    // métricas de ecossistema
    float avgBiofilm() const;
    float avgPlankton() const;
    float plantDensity(int cx, int cy, int radius) const;
    int countPlanktonRich(int minStage) const;

    // ESTAÇÕES (cada uma dura ~15 dias)
    int season() const { return season_; } // 0 primavera 1 verão 2 outono 3 inverno
    const char* seasonName() const;
    float seasonTemp() const;  // -1 inverno .. 0 neutro .. +1 verão
    float seasonRain() const;  // 0 seco .. 1 chuvoso
    void advanceSeason(int day);

    // INCÊNDIOS naturais
    void updateFires(float dt);
    bool onFire(int x, int y) const;
    void startFire(int x, int y);
    int countFireTiles() const;

    // POLINIZAÇÃO: insetos ajudam plantas a se reproduzirem
    float pollination() const { return pollination_; }
    void setPollination(float p) { pollination_ = p; }

private:
    int w_, h_;
    std::vector<TileData> tiles_;
    float timeOfDay_ = 0.1f;
    int season_ = 0;
    int day_ = 1;
    std::vector<uint8_t> fire_; // fogo por tile (0-15)
    float pollination_ = 0.f;

    float equilibriumMoisture(Tile t) const;
    float equilibriumFertility(Tile t) const;
    bool canHostPlant(Tile t) const;
};

const char* tileName(Tile t);

}
