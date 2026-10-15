#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>

#include "world.hpp"
#include "creature.hpp"
#include "god.hpp"
#include "hud.hpp"
#include "rng.hpp"
#include "fauna.hpp"
#include "save.hpp"

using namespace skc;

static const float SIM_DT = 1.f / 30.f;

// conversão mouse (pontos da janela) → mundo, com zoom
static bool mouseToWorld(int mx, int my, float camX, float camY,
                         int winW, int winH, float tilePx, float& wx, float& wy) {
    wx = camX + (mx - winW * 0.5f) / tilePx;
    wy = camY + (my - winH * 0.5f) / tilePx;
    return mx >= 0 && my >= 0 && mx < winW && my < winH;
}

struct Game {
    World world{240, 180};
    CreatureSim* sim = nullptr;
    Ecosystem* eco = nullptr;
    GodSystem* god = nullptr;
    Hud hud;
    bool running = true;
    bool paused = false;
    float speed = 1.f;
    float zoom = 1.f; // scroll do mouse
    float tilePx() const { return TILE_PX * zoom; }
    float camX = 0, camY = 0;
    int selectedId = -1;
    int selectedFaunaId = -1;
    unsigned day = 1;
    float timeAcc = 0.f;
    std::string omen;
    float omenT = 0.f;
    bool editMode = false;
    Tile brush = Tile::Grass;
    int brushRadius = 3;
    bool showHelp = false;
    bool showLadder = true;
    int fps = 0;
    int fpsCount = 0;
    float fpsTimer = 0.f;
    bool painting = false;
    bool panning = false; // arrastar com botão do meio
    int panLastX = 0, panLastY = 0;
    float dayFrac = 0.1f;
    int winW = 1280, winH = 720;
    float uiWheel = 0.f; // roda acumulada para a UI neste frame

    void regenWorld() {
        unsigned seed = (unsigned)std::random_device{}();
        world.generate(seed);
        if (god) { delete god; god = nullptr; }
        if (eco) { delete eco; eco = nullptr; }
        if (sim) { delete sim; sim = nullptr; }
        sim = new CreatureSim(world);
        eco = new Ecosystem(world, *sim);
        sim->setEcosystem(eco);
        god = new GodSystem(world, *sim);
        god->setEcosystem(eco);
        selectedId = -1;
        selectedFaunaId = -1;
        day = 1;
        timeAcc = 0.f;
        dayFrac = 0.1f;
        world.setTimeOfDay(dayFrac);
        camX = world.width() * 0.5f;
        camY = world.height() * 0.5f;
    }
};

static Creature* findSelected(Game& g) {
    if (g.selectedId < 0) return nullptr;
    Creature* c = g.sim->get(g.selectedId);
    if (!c || !c->alive()) {
        g.selectedId = -1;
        return nullptr;
    }
    return c;
}

static Fauna* findSelectedFauna(Game& g) {
    if (g.selectedFaunaId < 0 || !g.eco) return nullptr;
    Fauna* f = g.eco->get(g.selectedFaunaId);
    if (!f || !f->alive()) {
        g.selectedFaunaId = -1;
        return nullptr;
    }
    return f;
}

static void worldToScreen(float wx, float wy, float camX, float camY,
                          int& sx, int& sy, int winW, int winH, float tilePx) {
    sx = (int)((wx - camX) * tilePx + winW * 0.5f);
    sy = (int)((wy - camY) * tilePx + winH * 0.5f);
}

// ---- helpers de desenho ----
static void fillCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)std::sqrt((float)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void fillEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry) {
    for (int dy = -ry; dy <= ry; dy++) {
        float t = 1.f - (float)(dy * dy) / (float)(ry * ry + 1);
        int dx = (int)(rx * std::sqrt(std::max(0.f, t)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void fillTriangle(SDL_Renderer* r, int x1, int y1, int x2, int y2, int x3, int y3) {
    // scanline simples
    int miny = std::min({y1, y2, y3});
    int maxy = std::max({y1, y2, y3});
    for (int y = miny; y <= maxy; y++) {
        float xs[3] = {(float)x1, (float)x2, (float)x3};
        float ys[3] = {(float)y1, (float)y2, (float)y3};
        float xints[3];
        int n = 0;
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            if ((ys[i] <= y && ys[j] > y) || (ys[j] <= y && ys[i] > y)) {
                float t = (y - ys[i]) / (ys[j] - ys[i]);
                xints[n++] = xs[i] + t * (xs[j] - xs[i]);
            }
        }
        if (n == 2) {
            int xa = (int)std::min(xints[0], xints[1]);
            int xb = (int)std::max(xints[0], xints[1]);
            SDL_RenderDrawLine(r, xa, y, xb, y);
        }
    }
}

static void drawShadow(SDL_Renderer* r, int sx, int sy, int size) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 50);
    fillEllipse(r, sx, sy + size / 2 + 1, size, size / 3);
}

// cor por tipo de fauna (quadrado 1px)
static SDL_Color faunaColor(int type) {
    switch (type) {
        case 0: return {95, 205, 228, 255};  // medusa — ciano
        case 1: return {91, 110, 225, 255};  // peixe — azul-violeta
        case 2: return {68, 149, 100, 255};  // anfíbio — verde
        case 3: return {180, 150, 90, 255};  // herbívoro — marrom claro
        case 4: return {140, 130, 115, 255}; // carroNEIRO — cinza
        case 5: return {40, 70, 140, 255};   // piscador — azul escuro
        case 6: return {160, 55, 50, 255};   // lobo — vermelho
        case 7: return {220, 220, 240, 255}; // falcão — branco
        case 8: return {180, 140, 80, 255};  // passarinho — marrom claro
        case 9: return {110, 150, 80, 255};  // tartaruga — verde-oliva
        case 10: return {100, 110, 125, 255};// tubarão — cinza-azulado
        case 11: return {120, 80, 50, 255};  // urso — marrom escuro
        case 12: return {70, 95, 155, 255};  // baleia — azul-petróleo
        default: return {200, 200, 200, 255};
    }
}

static void drawFauna(SDL_Renderer* r, Game& g, int winW, int winH) {
    if (!g.eco) return;
    float tp = g.tilePx();
    float viewR = winW / (2.f * tp) + 2;
    float viewRy = winH / (2.f * tp) + 2;
    for (const auto& f : g.eco->all()) {
        if (!f.alive()) continue;
        if (std::fabs(f.x - g.camX) > viewR || std::fabs(f.y - g.camY) > viewRy) continue;
        int sx, sy;
        worldToScreen(f.x, f.y, g.camX, g.camY, sx, sy, winW, winH, tp);
        SDL_Color col = faunaColor((int)f.type);
        SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
        // tp já inclui zoom — não multiplicar de novo
        int sz = (int)std::max(2.f, tp * 0.35f * f.g.size);
        SDL_Rect rc = {sx - sz / 2, sy - sz / 2, sz, sz};
        SDL_RenderFillRect(r, &rc);
        if (f.id == g.selectedFaunaId) {
            SDL_SetRenderDrawColor(r, 255, 230, 120, 255);
            SDL_Rect ring = {sx - sz, sy - sz, sz * 2, sz * 2};
            SDL_RenderDrawRect(r, &ring);
        }
    }
}

static void drawBio(SDL_Renderer* r, Game& g, int winW, int winH) {
    // biofilme e plâncton na água (pontos)
    float tp = g.tilePx();
    int tx0 = std::max(0, (int)(g.camX - winW / (2.f * tp)) - 1);
    int ty0 = std::max(0, (int)(g.camY - winH / (2.f * tp)) - 1);
    int tx1 = std::min(g.world.width() - 1, (int)(g.camX + winW / (2.f * tp)) + 1);
    int ty1 = std::min(g.world.height() - 1, (int)(g.camY + winH / (2.f * tp)) + 1);
    for (int y = ty0; y <= ty1; y++) {
        for (int x = tx0; x <= tx1; x++) {
            const TileData& t = g.world.at(x, y);
            if (t.biofilm == 0 && t.plankton == 0) continue;
            int sx, sy;
            worldToScreen(x + 0.5f, y + 0.5f, g.camX, g.camY, sx, sy, winW, winH, g.tilePx());
            if (t.biofilm > 0) {
                SDL_SetRenderDrawColor(r, 90, 120, 90, 200);
                SDL_RenderDrawPoint(r, sx, sy + 1);
            }
            if (t.plankton > 0) {
                Uint8 a = (Uint8)(80 + t.plankton * 50);
                SDL_SetRenderDrawColor(r, 120, 220, 180, a);
                SDL_RenderDrawPoint(r, sx + 1, sy - 1);
                if (t.plankton >= 2) SDL_RenderDrawPoint(r, sx - 1, sy);
            }
        }
    }
}

static void drawCommunities(SDL_Renderer* r, Game& g, int winW, int winH) {
    if (!g.sim) return;
    float tp = g.tilePx();
    for (auto& comm : g.sim->communities()) {
        int sx, sy;
        worldToScreen(comm.x, comm.y, g.camX, g.camY, sx, sy, winW, winH, tp);
        // território
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 120, 200, 160, 30);
        int tr = (int)(comm.territoryR * tp);
        SDL_Rect terr = {sx - tr, sy - tr, tr * 2, tr * 2};
        SDL_RenderDrawRect(r, &terr);
        // toca
        if (comm.hasHut()) {
            drawShadow(r, sx, sy + 2, 6);
            SDL_SetRenderDrawColor(r, 130, 100, 70, 255);
            SDL_Rect hut = {sx - 5, sy - 3, 10, 7};
            SDL_RenderFillRect(r, &hut);
            // telhado em V
            fillTriangle(r, sx - 7, sy - 3, sx + 7, sy - 3, sx, sy - 9);
            SDL_SetRenderDrawColor(r, 100, 75, 50, 255);
            SDL_RenderDrawLine(r, sx - 7, sy - 3, sx, sy - 9);
            SDL_RenderDrawLine(r, sx + 7, sy - 3, sx, sy - 9);
            // porta
            SDL_SetRenderDrawColor(r, 60, 45, 30, 255);
            SDL_Rect door = {sx - 1, sy, 2, 4};
            SDL_RenderFillRect(r, &door);
        }
        // fogueira: chama animada + glow
        if (comm.hasFire()) {
            float t = (float)SDL_GetTicks() * 0.01f;
            float pulse = 0.75f + 0.25f * std::sin(t * 3.f);
            // glow de luz (noite)
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            for (int gr = 14; gr >= 3; gr -= 3) {
                Uint8 ga = (Uint8)(18 * pulse * (1.f - gr / 16.f));
                SDL_SetRenderDrawColor(r, 255, 160, 50, ga);
                fillCircle(r, sx, sy - 3, gr);
            }
            // troncos
            SDL_SetRenderDrawColor(r, 90, 60, 30, 255);
            SDL_RenderDrawLine(r, sx - 3, sy + 1, sx + 3, sy - 2);
            SDL_RenderDrawLine(r, sx - 3, sy - 2, sx + 3, sy + 1);
            // chama
            int fh = 4 + (int)(2.f * pulse);
            SDL_SetRenderDrawColor(r, 255, (Uint8)(180 * pulse), 40, 255);
            fillTriangle(r, sx - 2, sy, sx + 2, sy, sx, sy - fh);
            SDL_SetRenderDrawColor(r, 255, (Uint8)(230 * pulse), 120, 255);
            fillTriangle(r, sx - 1, sy, sx + 1, sy, sx, sy - fh + 1);
        }
    }
}

static void drawCreatures(SDL_Renderer* r, Game& g, int winW, int winH) {
    if (!g.sim) return;
    float tp = g.tilePx();
    float viewR = winW / (2.f * tp) + 2;
    float viewRy = winH / (2.f * tp) + 2;
    for (const auto& c : g.sim->all()) {
        if (!c.alive()) continue;
        if (std::fabs(c.x - g.camX) > viewR || std::fabs(c.y - g.camY) > viewRy) continue;
        int sx, sy;
        worldToScreen(c.x, c.y, g.camX, g.camY, sx, sy, winW, winH, tp);
        // pensante: quadrado, azul macho / rosa fêmea, menor = filhote
        SDL_Color col;
        if (c.state == CState::Sick) col = {170, 80, 200, 255};
        else if (c.male) col = {80, 140, 220, 255};
        else col = {230, 130, 170, 255};
        SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
        int sz = c.juvenile()
            ? (int)std::max(2.f, tp * 0.25f)
            : (int)std::max(3.f, tp * 0.38f);
        SDL_Rect rc = {sx - sz / 2, sy - sz / 2, sz, sz};
        SDL_RenderFillRect(r, &rc);
        if (c.id == g.selectedId) {
            SDL_SetRenderDrawColor(r, 255, 230, 120, 255);
            SDL_Rect ring = {sx - sz, sy - sz, sz * 2, sz * 2};
            SDL_RenderDrawRect(r, &ring);
        }
    }
}

static void drawPlants(SDL_Renderer* r, Game& g, int winW, int winH) {
    float tp = g.tilePx();
    int tx0 = std::max(0, (int)(g.camX - winW / (2.f * tp)) - 1);
    int ty0 = std::max(0, (int)(g.camY - winH / (2.f * tp)) - 1);
    int tx1 = std::min(g.world.width() - 1, (int)(g.camX + winW / (2.f * tp)) + 1);
    int ty1 = std::min(g.world.height() - 1, (int)(g.camY + winH / (2.f * tp)) + 1);

    for (int y = ty0; y <= ty1; y++) {
        for (int x = tx0; x <= tx1; x++) {
            const TileData& td = g.world.at(x, y);
            int sx, sy;
            worldToScreen(x + 0.5f, y + 0.5f, g.camX, g.camY, sx, sy, winW, winH, g.tilePx());

            // fogo (incêndio)
            if (g.world.onFire(x, y)) {
                int fsz = (int)std::max(3.f, tp * 0.5f);
                float pulse = 0.7f + 0.3f * std::sin((float)SDL_GetTicks() * 0.01f + x * 3.f + y * 7.f);
                SDL_SetRenderDrawColor(r, (Uint8)(255 * pulse), (Uint8)(140 * pulse), 30, 255);
                SDL_Rect fire = {sx - fsz / 2, sy - fsz / 2, fsz, fsz};
                SDL_RenderFillRect(r, &fire);
            }

            // ossos — quadrado branco
            if (td.bones > 0) {
                int bsz = (int)std::max(2.f, tp * 0.3f);
                SDL_SetRenderDrawColor(r, 220, 220, 210, 255);
                SDL_Rect bone = {sx - bsz / 2, sy - bsz / 2, bsz, bsz};
                SDL_RenderFillRect(r, &bone);
                // detalhe escuro (espalha)
                if (td.bones > 5) {
                    SDL_SetRenderDrawColor(r, 180, 180, 170, 255);
                    SDL_Rect d = {sx - bsz / 2 + 1, sy - bsz / 2 + 1, bsz - 2, bsz - 2};
                    SDL_RenderDrawRect(r, &d);
                }
            }
            // carne fresca (vermelho) por cima dos ossos
            if (td.corpse > 0) {
                int csz = (int)std::max(2.f, tp * 0.28f);
                SDL_SetRenderDrawColor(r, 180, 50, 45, 255);
                SDL_Rect meat = {sx - csz / 2, sy - csz / 2, csz, csz};
                SDL_RenderFillRect(r, &meat);
            }

            // árvores na floresta — retângulos, estilo pixel world
            if (td.type == Tile::Forest && tp >= 5) {
                int h = (x * 73856093) ^ (y * 19349663);
                if ((h & 3) == 0) {
                    int trunkH = (int)(tp * 0.35f);
                    int leafW = (int)(tp * 0.55f);
                    int leafH = (int)(tp * 0.45f);
                    int base = sy + (int)(tp / 3);
                    // tronco
                    SDL_SetRenderDrawColor(r, 100, 75, 45, 255);
                    SDL_Rect trunk = {sx - 1, base - trunkH, 2, trunkH};
                    SDL_RenderFillRect(r, &trunk);
                    // copa: retângulo verde
                    SDL_SetRenderDrawColor(r, 45, 110, 40, 255);
                    SDL_Rect canopy = {sx - leafW / 2, base - trunkH - leafH, leafW, leafH};
                    SDL_RenderFillRect(r, &canopy);
                    // brilho
                    SDL_SetRenderDrawColor(r, 65, 140, 55, 255);
                    SDL_Rect shine = {sx - leafW / 4, base - trunkH - leafH, leafW / 2, leafH / 3};
                    SDL_RenderFillRect(r, &shine);
                }
            }
            // arbusto em grama — quadrado
            if (td.type == Tile::Grass && tp >= 5) {
                int h = ((x * 83492791) ^ (y * 1299709)) & 7;
                if (h == 0) {
                    int bsz = (int)(tp * 0.3f);
                    SDL_SetRenderDrawColor(r, 55, 115, 48, 255);
                    SDL_Rect bush = {sx - bsz / 2, sy - bsz / 2, bsz, bsz};
                    SDL_RenderFillRect(r, &bush);
                }
            }

            const Plant& p = td.plant;
            if (p.kind == PlantKind::None) continue;
            // quadrado pequeno por estágio
            int sz;
            SDL_Color col;
            if (p.stage == 0) {
                sz = (int)std::max(1.f, tp * 0.15f);
                col = {60, 120, 50, 255};
            } else if (p.stage == 1) {
                sz = (int)std::max(2.f, tp * 0.25f);
                col = {80, 160, 70, 255};
            } else {
                sz = (int)std::max(2.f, tp * 0.3f);
                col = (p.kind == PlantKind::Fruit) ? SDL_Color{250, 210, 50, 255}
                     : (p.gene.cure > 0.25f) ? SDL_Color{240, 200, 80, 255}
                     : SDL_Color{180, 70, 210, 255};
            }
            SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
            SDL_Rect rc = {sx - sz / 2, sy - sz / 2, sz, sz};
            SDL_RenderFillRect(r, &rc);
        }
    }
}

static void paintAt(Game& g, float wx, float wy) {
    g.world.paint((int)wx, (int)wy, g.brushRadius, g.brush);
}

static Creature* pickCreature(Game& g, float wx, float wy) {
    Creature* best = nullptr;
    float bd = 1.6f * 1.6f;
    for (auto& c : g.sim->all()) {
        if (!c.alive()) continue;
        float dx = c.x - wx, dy = c.y - wy;
        float d = dx * dx + dy * dy;
        if (d < bd) {
            bd = d;
            best = &c;
        }
    }
    return best;
}

static Fauna* pickFauna(Game& g, float wx, float wy) {
    if (!g.eco) return nullptr;
    Fauna* best = nullptr;
    float bd = 1.6f * 1.6f;
    for (auto& f : g.eco->all()) {
        if (!f.alive()) continue;
        float dx = f.x - wx, dy = f.y - wy;
        float d = dx * dx + dy * dy;
        if (d < bd) {
            bd = d;
            best = &f;
        }
    }
    return best;
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    // janela grande desde o começo
    SDL_DisplayMode dm;
    int initW = 1600, initH = 900;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        initW = dm.w * 9 / 10;
        initH = dm.h * 9 / 10;
        if (initW < 1280) initW = 1280;
        if (initH < 720) initH = 720;
    }

    SDL_Window* win = SDL_CreateWindow(
        "SKC E-195",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        initW, initH,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (win) SDL_MaximizeWindow(win);

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    if (!win || !ren) {
        std::fprintf(stderr, "janela/renderer: %s\n", SDL_GetError());
        return 1;
    }

    Game g;
    if (!g.hud.loadFonts()) {
        std::fprintf(stderr, "aviso: fontes nao encontradas, HUD pode falhar\n");
    }

    g.world.generate((unsigned)std::random_device{}());
    g.sim = new CreatureSim(g.world);
    g.eco = new Ecosystem(g.world, *g.sim);
    g.sim->setEcosystem(g.eco);
    g.god = new GodSystem(g.world, *g.sim);
    g.god->setEcosystem(g.eco);
    g.camX = g.world.width() * 0.5f;
    g.camY = g.world.height() * 0.5f;

    Uint64 prev = SDL_GetPerformanceCounter();
    double perf = (double)SDL_GetPerformanceFrequency();

    int smokeFrames = 0;
    if (const char* env = std::getenv("SKC_SMOKE_FRAMES")) {
        smokeFrames = std::atoi(env);
        g.speed = 6.f;
    }
    if (std::getenv("SKC_SHOT")) {
        g.zoom = 2.5f; // close-up p/ ver criaturas no shot
    }
    int frame = 0;

    while (g.running) {
        frame++;
        if (smokeFrames > 0 && frame >= smokeFrames) g.running = false;
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((now - prev) / perf);
        prev = now;
        if (dt > 0.1f) dt = 0.1f;
        g.uiWheel = 0.f;

        g.fpsCount++;
        g.fpsTimer += dt;
        if (g.fpsTimer >= 0.5f) {
            g.fps = (int)(g.fpsCount / g.fpsTimer);
            g.fpsCount = 0;
            g.fpsTimer = 0.f;
        }

        // ---- tamanho REAL do renderer em pixels (ocupa a tela toda) ----
        int renW = 0, renH = 0;
        SDL_GetRendererOutputSize(ren, &renW, &renH);
        if (renW <= 0 || renH <= 0) {
            SDL_GetWindowSize(win, &renW, &renH);
        }
        g.winW = renW;
        g.winH = renH;

        // escala janela→pixels (HiDPI); desenhamos em pixels
        int winWpt = 0, winHpt = 0;
        SDL_GetWindowSize(win, &winWpt, &winHpt);
        float sx = (winWpt > 0) ? renW / (float)winWpt : 1.f;
        float sy = (winHpt > 0) ? renH / (float)winHpt : 1.f;
        // escala de UI HiDPI (fontes maiores em telas densas)
        g.hud.setUiScale(sx > 1.2f ? sx : 1.f);

        int rawMx, rawMy;
        SDL_GetMouseState(&rawMx, &rawMy);
        float mx = rawMx * sx;
        float my = rawMy * sy;
        float mwx, mwy;
        bool mouseInWorld = mouseToWorld((int)mx, (int)my, g.camX, g.camY, renW, renH, g.tilePx(), mwx, mwy);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g.running = false;
            else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) g.running = false;
                else if (k == SDLK_SPACE) g.paused = !g.paused;
                else if (k == SDLK_h) g.showHelp = !g.showHelp;
                else if (k == SDLK_l) g.showLadder = !g.showLadder;
                else if (k == SDLK_e) g.editMode = !g.editMode;
                else if (k == SDLK_F5) {
                    std::string path = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/.skc_e195_save.txt";
                    if (saveGame(g.world, *g.sim, *g.eco, g.day, g.timeAcc, g.camX, g.camY, g.zoom, path)) {
                        g.omen = "Mundo salvo!"; g.omenT = 2.f;
                    } else { g.omen = "Erro ao salvar."; g.omenT = 2.f; }
                }
                else if (k == SDLK_F9) {
                    std::string path = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/.skc_e195_save.txt";
                    CreatureSim* ns = nullptr;
                    Ecosystem* ne = nullptr;
                    GodSystem* ng = nullptr;
                    unsigned nd; float nta, ncx, ncy, nz;
                    if (loadGame(g.world, ns, ne, ng, nd, nta, ncx, ncy, nz, path)) {
                        delete g.god; g.god = ng;
                        delete g.eco; g.eco = ne;
                        delete g.sim; g.sim = ns;
                        g.day = nd; g.timeAcc = nta;
                        g.camX = ncx; g.camY = ncy; g.zoom = nz;
                        g.selectedId = -1; g.selectedFaunaId = -1;
                        g.omen = "Mundo carregado!"; g.omenT = 2.f;
                    } else { g.omen = "Nenhum save encontrado."; g.omenT = 2.f; }
                }
                else if (k == SDLK_z) g.speed = std::max(0.5f, g.speed - 0.5f);
                else if (k == SDLK_x) g.speed = std::min(6.f, g.speed + 0.5f);
                else if (k == SDLK_r) {
                    if (!g.editMode) g.regenWorld();
                }
                else if (g.editMode) {
                    if (k >= SDLK_1 && k <= SDLK_8) {
                        int idx = (int)(k - SDLK_1);
                        g.brush = (Tile)idx;
                    }
                } else {
                    if (k >= SDLK_1 && k <= SDLK_4 && g.god && mouseInWorld) {
                        GodAction a = (GodAction)(k - SDLK_1);
                        GodResult res = g.god->invoke(a, (int)mwx, (int)mwy);
                        g.omen = res.text;
                        g.omenT = 3.5f;
                    }
                    if (k == SDLK_TAB) {
                        // alterna entre pensantes e fauna
                        bool hasSel = g.selectedId >= 0 || g.selectedFaunaId >= 0;
                        bool wasCreature = g.selectedId >= 0;
                        if (!hasSel || wasCreature) {
                            if (g.eco && !g.eco->all().empty()) {
                                auto& all = g.eco->all();
                                int idx = 0;
                                for (int i = 0; i < (int)all.size(); i++) {
                                    if (all[i].id == g.selectedFaunaId) { idx = (i + 1) % (int)all.size(); break; }
                                }
                                g.selectedFaunaId = all[idx].id;
                                g.selectedId = -1;
                            } else if (!g.sim->all().empty()) {
                                auto& all = g.sim->all();
                                int idx = 0;
                                for (int i = 0; i < (int)all.size(); i++) {
                                    if (all[i].id == g.selectedId) { idx = (i + 1) % (int)all.size(); break; }
                                }
                                g.selectedId = all[idx].id;
                                g.selectedFaunaId = -1;
                            }
                        } else {
                            if (!g.sim->all().empty()) {
                                auto& all = g.sim->all();
                                int idx = 0;
                                for (int i = 0; i < (int)all.size(); i++) {
                                    if (all[i].id == g.selectedId) { idx = (i + 1) % (int)all.size(); break; }
                                }
                                g.selectedId = all[idx].id;
                                g.selectedFaunaId = -1;
                            }
                        }
                    }
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_MIDDLE) {
                g.panning = true;
                g.panLastX = e.button.x;
                g.panLastY = e.button.y;
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_MIDDLE) {
                g.panning = false;
            }
            else if (e.type == SDL_MOUSEMOTION && g.panning) {
                int dx = e.motion.x - g.panLastX;
                int dy = e.motion.y - g.panLastY;
                g.panLastX = e.motion.x;
                g.panLastY = e.motion.y;
                float tp = g.tilePx();
                g.camX -= dx / tp;
                g.camY -= dy / tp;
                g.camX = clampf(g.camX, 0.f, (float)g.world.width());
                g.camY = clampf(g.camY, 0.f, (float)g.world.height());
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                if (mouseInWorld) {
                    if (g.editMode) {
                        g.painting = true;
                        paintAt(g, mwx, mwy);
                    } else {
                        Creature* c = pickCreature(g, mwx, mwy);
                        Fauna* f = c ? nullptr : pickFauna(g, mwx, mwy);
                        if (c) {
                            g.selectedId = c->id;
                            g.selectedFaunaId = -1;
                        } else if (f) {
                            g.selectedFaunaId = f->id;
                            g.selectedId = -1;
                        } else {
                            g.selectedId = -1;
                            g.selectedFaunaId = -1;
                        }
                    }
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                g.painting = false;
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                // painéis da esquerda consomem a rola (zoom só no mundo)
                bool overLeftUI = false;
                {
                    float mwxp = rawMx * sx, mwyp = rawMy * sy;
                    (void)mwyp;
                    if (mwxp < 320) overLeftUI = true; // coluna esquerda
                }
                bool hasInspect = g.selectedId >= 0 || g.selectedFaunaId >= 0;
                if (g.editMode && (SDL_GetModState() & KMOD_SHIFT)) {
                    g.brushRadius = std::clamp(g.brushRadius + e.wheel.y, 1, 10);
                } else if (overLeftUI && hasInspect) {
                    g.uiWheel += (float)e.wheel.y;
                } else {
                    // zoom com scroll
                    float z = g.zoom * (e.wheel.y > 0 ? 1.12f : 0.89f);
                    g.zoom = clampf(z, 0.35f, 4.f);
                }
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        float camSpd = 22.f * dt;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) g.camY -= camSpd;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) g.camY += camSpd;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) g.camX -= camSpd;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) g.camX += camSpd;
        g.camX = clampf(g.camX, 0.f, (float)g.world.width());
        g.camY = clampf(g.camY, 0.f, (float)g.world.height());

        if (g.editMode && g.painting && mouseInWorld) {
            paintAt(g, mwx, mwy);
        }

        if (!g.paused) {
            float simDt = SIM_DT * g.speed;
            int steps = (int)std::max(1.f, std::min(g.speed, 6.f));
            for (int i = 0; i < steps; i++) {
                g.world.update();
                if (g.eco) g.eco->update(simDt, g.day);
                g.sim->update(simDt, g.day);
                if (g.god) g.god->update(simDt);
                g.timeAcc += simDt;
                while (g.timeAcc >= SECONDS_PER_DAY) {
                    g.timeAcc -= SECONDS_PER_DAY;
                    g.day++;
                }
                g.dayFrac = g.timeAcc / SECONDS_PER_DAY;
                g.world.setTimeOfDay(g.dayFrac);
                g.world.setDay(g.day); // atualiza estação
            }
            // lore do ecossistema vira omen
            if (g.eco && !g.eco->lore().empty() && g.omenT <= 0.f) {
                static std::string lastLore;
                const std::string& last = g.eco->lore().back();
                if (last != lastLore) {
                    lastLore = last;
                    g.omen = last;
                    g.omenT = 5.f;
                }
            }
        }

        // render — ocupa o renderer TODO (pixels reais)
        SDL_SetRenderDrawColor(ren, 8, 10, 14, 255);
        SDL_RenderClear(ren);

        int winW = renW, winH = renH;
        int viewW = renW, viewH = renH;
        float tp = g.tilePx();
        int tx0 = std::max(0, (int)(g.camX - viewW / (2.f * tp)) - 1);
        int ty0 = std::max(0, (int)(g.camY - viewH / (2.f * tp)) - 1);
        int tx1 = std::min(g.world.width() - 1, (int)(g.camX + viewW / (2.f * tp)) + 1);
        int ty1 = std::min(g.world.height() - 1, (int)(g.camY + viewH / (2.f * tp)) + 1);

        float light = g.world.lightLevel();

        for (int y = ty0; y <= ty1; y++) {
            for (int x = tx0; x <= tx1; x++) {
                const TileData& t = g.world.at(x, y);
                SDL_Color col = tileColor(t, x, y);
                col.r = (Uint8)(col.r * (0.25f + light * 0.75f));
                col.g = (Uint8)(col.g * (0.28f + light * 0.72f));
                col.b = (Uint8)(col.b * (0.40f + light * 0.60f));
                int sx, sy;
                worldToScreen((float)x, (float)y, g.camX, g.camY, sx, sy, winW, winH, g.tilePx());
                SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, 255);
                int ts = (int)std::max(3.f, tp + 1); // +1 sobrepõe: sem costura de grade
                SDL_Rect rc = {sx, sy, ts, ts};
                SDL_RenderFillRect(ren, &rc);
            }
        }

        drawBio(ren, g, viewW, viewH);
        drawPlants(ren, g, viewW, viewH);
        drawCommunities(ren, g, viewW, viewH);
        drawFauna(ren, g, viewW, viewH);
        drawCreatures(ren, g, viewW, viewH);

        // ---- NOITE: gradiente azul + fogueiras iluminam ----
        if (light < 0.9f) {
            float night = clampf(1.f - light, 0.f, 1.f);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            // gradiente: topo mais escuro, base levemente mais clara
            int bands = 8;
            for (int i = 0; i < bands; i++) {
                float t = (float)i / bands;
                Uint8 a = (Uint8)(night * (90.f + t * 40.f));
                SDL_SetRenderDrawColor(ren, 6, 10, 35, a);
                SDL_Rect band = {0, i * winH / bands, winW, winH / bands + 1};
                SDL_RenderFillRect(ren, &band);
            }
            // fogueiras furam a noite (círculo de luz)
            if (g.sim) {
                for (auto& comm : g.sim->communities()) {
                    if (!comm.hasFire()) continue;
                    int sx, sy;
                    worldToScreen(comm.x, comm.y, g.camX, g.camY, sx, sy, winW, winH, g.tilePx());
                    float pulse = 0.8f + 0.2f * std::sin((float)SDL_GetTicks() * 0.006f);
                    for (int gr = 28; gr >= 6; gr -= 5) {
                        float fall = 1.f - gr / 32.f;
                        Uint8 ga = (Uint8)(night * 70.f * pulse * fall);
                        SDL_SetRenderDrawColor(ren, 255, 170, 60, ga);
                        fillCircle(ren, sx, sy - 2, gr);
                    }
                }
            }
        }

        // mira divina
        if (!g.editMode && mouseInWorld) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 255, 240, 180, 160);
            int sx, sy;
            worldToScreen(mwx, mwy, g.camX, g.camY, sx, sy, winW, winH, g.tilePx());
            SDL_Rect m = {sx - 6, sy - 6, 12, 12};
            SDL_RenderDrawRect(ren, &m);
            SDL_RenderDrawLine(ren, sx - 10, sy, sx + 10, sy);
            SDL_RenderDrawLine(ren, sx, sy - 10, sx, sy + 10);
        }

        // escova
        if (g.editMode && mouseInWorld) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 240, 200, 120, 140);
            int sx, sy;
            worldToScreen(mwx, mwy, g.camX, g.camY, sx, sy, winW, winH, g.tilePx());
            int rr = (int)(g.brushRadius * g.tilePx());
            SDL_Rect b = {sx - rr, sy - rr, rr * 2, rr * 2};
            SDL_RenderDrawRect(ren, &b);
        }

        if (g.omenT > 0.f) g.omenT -= dt;

        HudState hs;
        hs.fps = g.fps;
        hs.day = g.day;
        hs.dayFrac = g.dayFrac;
        hs.phaseName = g.world.phaseName();
        hs.seasonName = g.world.seasonName();
        hs.light = light;
        hs.paused = g.paused;
        hs.speed = g.speed;
        hs.humidity = g.world.globalHumidity();
        hs.creatureCount = g.sim->aliveCount() + (g.eco ? (int)g.eco->all().size() : 0);
        hs.selected = findSelected(g);
        hs.selectedFauna = findSelectedFauna(g);
        hs.eco = g.eco;
        hs.god = g.god;
        hs.events = &g.sim->events();
        hs.editMode = g.editMode;
        hs.brush = g.brush;
        hs.brushRadius = g.brushRadius;
        hs.showHelp = g.showHelp;
        hs.showLadder = g.showLadder;
        hs.omen = g.omen;
        hs.omenT = g.omenT;
        hs.world = &g.world;
        hs.mouseX = mwx;
        hs.mouseY = mwy;
        hs.mouseInWorld = mouseInWorld;
        hs.sageStats = g.sim->stats();
        hs.plantGene = g.world.averagePlantGene();
        hs.plantCount = g.world.plantCount();
        hs.winW = winW;
        hs.winH = winH;
        hs.sims = g.sim;
        hs.camX = g.camX;
        hs.camY = g.camY;
        hs.uiWheel = g.uiWheel;
        hs.mouseWinX = mx;
        hs.mouseWinY = my;
        g.hud.draw(ren, hs);

        if (hs.creatureCount == 0 && !g.paused) {
            // mundo vazio: mostre aviso via omen ocasional
            if (g.omenT <= 0.f) {
                g.omen = "Silencio. A vida espera as condicoes certas. (chuva alimenta a agua)";
                g.omenT = 2.f;
            }
        }

        // captura opcional: SKC_SHOT=/caminho/saida.bmp (útil para debug de UI)
        // precisa ser ANTES do present (backbuffer ainda válido)
        if (const char* shot = std::getenv("SKC_SHOT")) {
            static int shotAt = -1;
            if (shotAt < 0) shotAt = smokeFrames > 0 ? smokeFrames - 1 : 90;
            // pouco antes do shot, seleciona alguém p/ mostrar o painel
            if (frame == shotAt - 5 && g.selectedId < 0 && g.selectedFaunaId < 0) {
                // prefere pensante (mostra a ficha de emoções)
                for (auto& c : g.sim->all()) if (c.alive()) {
                    g.selectedId = c.id; g.camX = c.x; g.camY = c.y; break;
                }
                if (g.selectedId < 0)
                    for (auto& f : g.eco->all()) if (f.alive()) {
                        g.selectedFaunaId = f.id; g.camX = f.x; g.camY = f.y; break;
                    }
            }
            if (frame == shotAt) {
                SDL_Surface* ss = SDL_CreateRGBSurfaceWithFormat(0, renW, renH, 32, SDL_PIXELFORMAT_ARGB8888);
                if (ss) {
                    SDL_RenderReadPixels(ren, nullptr, ss->format->format, ss->pixels, ss->pitch);
                    SDL_SaveBMP(ss, shot);
                    SDL_FreeSurface(ss);
                }
            }
        }

        SDL_RenderPresent(ren);
    }

    delete g.sim;
    delete g.eco;
    delete g.god;
    g.hud.close();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
