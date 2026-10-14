#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <vector>
#include "creature.hpp"
#include "god.hpp"
#include "world.hpp"
#include "fauna.hpp"

namespace skc {

struct HudState {
    int fps = 0;
    unsigned day = 0;
    float dayFrac = 0.f;
    const char* phaseName = "Dia";
    const char* seasonName = "Primavera";
    float light = 1.f;
    bool paused = false;
    float speed = 1.f;
    float humidity = 0.f;
    int creatureCount = 0;
    Creature* selected = nullptr;
    Fauna* selectedFauna = nullptr;
    Ecosystem* eco = nullptr;
    GodSystem* god = nullptr;
    std::vector<SimEvent>* events = nullptr;
    bool editMode = false;
    Tile brush = Tile::Grass;
    bool showHelp = false;
    std::string omen;
    float omenT = 0.f;
    int brushRadius = 3;
    World* world = nullptr;
    float mouseX = 0, mouseY = 0;
    bool mouseInWorld = false;
    bool showLadder = true;
    CreatureSim::Stats sageStats;
    PlantGene plantGene;
    int plantCount = 0;
    int winW = 1280;
    int winH = 720;
    CreatureSim* sims = nullptr;
    float camX = 0, camY = 0;
};

class Hud {
public:
    bool loadFonts();
    void close();
    void draw(SDL_Renderer* r, const HudState& st);

private:
    TTF_Font* font_ = nullptr;
    TTF_Font* fontBig_ = nullptr;
    TTF_Font* fontSmall_ = nullptr;

    void text(SDL_Renderer* r, TTF_Font* f, int x, int y, const std::string& s, SDL_Color c, bool shadow = true);
    void panel(SDL_Renderer* r, int x, int y, int w, int h, Uint8 a = 170);
    void bar(SDL_Renderer* r, int x, int y, int w, int h, float v, SDL_Color c);
};

SDL_Color tileColor(const TileData& t, int x, int y);

}
