#pragma once
#include <string>

namespace skc {

class World;
class CreatureSim;
class Ecosystem;
class GodSystem;

// salva tudo em arquivo texto
bool saveGame(World& w, CreatureSim& sim, Ecosystem& eco,
              unsigned day, float timeAcc, float camX, float camY, float zoom,
              const std::string& path);

// carrega no mundo existente (limpa e recria sim/eco/god)
bool loadGame(World& w, CreatureSim*& sim, Ecosystem*& eco, GodSystem*& god,
              unsigned& day, float& timeAcc, float& camX, float& camY, float& zoom,
              const std::string& path);

}

