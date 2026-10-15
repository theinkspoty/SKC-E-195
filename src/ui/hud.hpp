#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>
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
    // roda do mouse sobre painéis de UI (1 = sobe, -1 = desce)
    float uiWheel = 0.f;
    // posição do mouse em pontos da janela (para hit-test de UI)
    float mouseWinX = 0, mouseWinY = 0;
};

class Hud {
public:
    bool loadFonts();
    void close();
    void draw(SDL_Renderer* r, const HudState& st);
    // recarrega fontes se a escala HiDPI mudou muito
    void setUiScale(float scale);

private:
    TTF_Font* font_ = nullptr;
    TTF_Font* fontBig_ = nullptr;
    TTF_Font* fontSmall_ = nullptr;
    float uiScale_ = 1.f;
    float scrollInspect_ = 0.f; // rolagem do painel de inspeção
    int lastInspectId_ = -1;    // reseta rolagem ao trocar de ser

    // cache de texturas de texto (evita criar/destroir textura por frame)
    struct TextKey {
        std::string s;
        Uint32 rgba;
        TTF_Font* font;
        bool operator==(const TextKey& o) const {
            return rgba == o.rgba && font == o.font && s == o.s;
        }
    };
    struct TextKeyHash {
        size_t operator()(const TextKey& k) const {
            size_t h = std::hash<std::string>{}(k.s);
            h ^= (size_t)k.font + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= (size_t)k.rgba + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    struct TextEntry {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
        Uint32 lastUse = 0;
    };
    std::unordered_map<TextKey, TextEntry, TextKeyHash> textCache_;
    Uint32 cacheTick_ = 0;

    void text(SDL_Renderer* r, TTF_Font* f, int x, int y, const std::string& s, SDL_Color c, bool shadow = true);
    int textWidth(TTF_Font* f, const std::string& s);
    void panel(SDL_Renderer* r, int x, int y, int w, int h, Uint8 a = 170);
    void panelTitle(SDL_Renderer* r, int x, int y, int w, const std::string& title, SDL_Color c);
    void bar(SDL_Renderer* r, int x, int y, int w, int h, float v, SDL_Color c);
    void chip(SDL_Renderer* r, int x, int y, int w, int h, const std::string& s, SDL_Color fg, Uint8 a = 160);
    void section(SDL_Renderer* r, int x, int y, int w, const std::string& name, SDL_Color c);
    void fillCircle(SDL_Renderer* r, int cx, int cy, int radius);
    void drawGodIcon(SDL_Renderer* r, GodAction a, int cx, int cy, int s, SDL_Color c);
    void trimCache();
    void freeCache();
};

SDL_Color tileColor(const TileData& t, int x, int y);

}
