#pragma once
#include <string>
#include "world.hpp"
#include "creature.hpp"
#include "fauna.hpp"

namespace skc {

enum class GodAction {
    Rain = 0,
    PetitionFruit,
    Asteroid,
    Frost,
    COUNT
};

struct GodResult {
    bool ok = false;
    std::string text;
};

class GodSystem {
public:
    GodSystem(World& world, CreatureSim& creatures);

    void setEcosystem(Ecosystem* eco) { eco_ = eco; }

    GodResult invoke(GodAction a, int cx, int cy);
    void update(float dt);

    float cooldownLeft(GodAction a) const;
    float cooldownMax(GodAction a) const;

    static const char* actionName(GodAction a);
    static const char* actionKey(GodAction a);

private:
    World& world_;
    CreatureSim& creatures_;
    Ecosystem* eco_ = nullptr;
    float cd_[4] = {};

    GodResult doRain(int cx, int cy);
    GodResult doPetitionFruit(int cx, int cy);
    GodResult doAsteroid(int cx, int cy);
    GodResult doFrost(int cx, int cy);
};

}
