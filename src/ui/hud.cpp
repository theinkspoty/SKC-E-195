#include "hud.hpp"
#include "rng.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace skc {

static const char* fontCandidates[] = {
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    nullptr
};

static const char* fontBoldCandidates[] = {
    "/usr/share/fonts/noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    nullptr
};

static TTF_Font* loadAny(const char** candidates, int size) {
    for (int i = 0; candidates[i]; i++) {
        TTF_Font* f = TTF_OpenFont(candidates[i], size);
        if (f) return f;
    }
    return nullptr;
}

bool Hud::loadFonts() {
    int s = (int)(uiScale_ + 0.5f);
    if (s < 1) s = 1;
    font_ = loadAny(fontCandidates, 14 * s);
    fontSmall_ = loadAny(fontCandidates, 12 * s);
    fontBig_ = loadAny(fontBoldCandidates, 18 * s);
    if (!fontBig_) fontBig_ = font_;
    if (!fontSmall_) fontSmall_ = font_;
    return font_ != nullptr;
}

void Hud::setUiScale(float scale) {
    if (scale < 1.f) scale = 1.f;
    if (scale > 3.f) scale = 3.f;
    // só recarrega se mudou de faixa inteira
    int oldS = (int)(uiScale_ + 0.5f);
    int newS = (int)(scale + 0.5f);
    if (newS == oldS) { uiScale_ = scale; return; }
    freeCache();
    if (font_ && font_ != fontBig_) TTF_CloseFont(font_);
    if (fontBig_ && fontBig_ != font_) TTF_CloseFont(fontBig_);
    if (fontSmall_ && fontSmall_ != font_) TTF_CloseFont(fontSmall_);
    font_ = fontBig_ = fontSmall_ = nullptr;
    uiScale_ = scale;
    loadFonts();
}

void Hud::close() {
    freeCache();
    if (font_ && font_ != fontBig_) TTF_CloseFont(font_);
    if (fontBig_ && fontBig_ != font_) TTF_CloseFont(fontBig_);
    if (fontSmall_ && fontSmall_ != font_) TTF_CloseFont(fontSmall_);
    font_ = fontBig_ = fontSmall_ = nullptr;
}

void Hud::freeCache() {
    for (auto& kv : textCache_) {
        if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    }
    textCache_.clear();
}

void Hud::trimCache() {
    constexpr size_t MAX = 900;
    if (textCache_.size() <= MAX) return;
    // ordena por lastUse e apaga os mais antigos
    std::vector<std::unordered_map<TextKey, TextEntry, TextKeyHash>::iterator> all;
    all.reserve(textCache_.size());
    for (auto it = textCache_.begin(); it != textCache_.end(); ++it) all.push_back(it);
    std::sort(all.begin(), all.end(),
              [](const std::unordered_map<TextKey, TextEntry, TextKeyHash>::iterator& a,
                 const std::unordered_map<TextKey, TextEntry, TextKeyHash>::iterator& b) {
                  return a->second.lastUse < b->second.lastUse;
              });
    size_t kill = textCache_.size() / 5 + 1;
    for (size_t i = 0; i < kill && i < all.size(); i++) {
        if (all[i]->second.tex) SDL_DestroyTexture(all[i]->second.tex);
        textCache_.erase(all[i]);
    }
}

int Hud::textWidth(TTF_Font* f, const std::string& s) {
    if (!f || s.empty()) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s.c_str(), &w, &h);
    return w;
}

void Hud::text(SDL_Renderer* r, TTF_Font* f, int x, int y, const std::string& s, SDL_Color c, bool shadow) {
    if (!f || s.empty()) return;
    Uint32 rgba = ((Uint32)c.r << 24) | ((Uint32)c.g << 16) | ((Uint32)c.b << 8) | c.a;
    TextKey key{s, rgba, f};
    auto it = textCache_.find(key);
    TextEntry* e = nullptr;
    if (it != textCache_.end() && it->second.tex) {
        e = &it->second;
        e->lastUse = ++cacheTick_;
    } else {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s.c_str(), c);
        if (!surf) return;
        TextEntry ne;
        ne.tex = SDL_CreateTextureFromSurface(r, surf);
        ne.w = surf->w;
        ne.h = surf->h;
        ne.lastUse = ++cacheTick_;
        SDL_FreeSurface(surf);
        if (!ne.tex) return;
        auto res = textCache_.insert({key, ne});
        e = &res.first->second;
        if (textCache_.size() > 950) trimCache();
    }
    if (shadow) {
        // sombra: multiplica textura preta por trás (barato via cor modulada)
        SDL_SetTextureColorMod(e->tex, 0, 0, 0);
        SDL_SetTextureAlphaMod(e->tex, 170);
        SDL_Rect d = {x + 1, y + 1, e->w, e->h};
        SDL_RenderCopy(r, e->tex, nullptr, &d);
        SDL_SetTextureColorMod(e->tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(e->tex, 255);
    }
    SDL_Rect dst = {x, y, e->w, e->h};
    SDL_RenderCopy(r, e->tex, nullptr, &dst);
}

void Hud::fillCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)std::sqrt((float)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void Hud::panel(SDL_Renderer* r, int x, int y, int w, int h, Uint8 a) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // preenchimento com gradiente suave (topo mais claro)
    for (int i = 0; i < h; i++) {
        float t = h > 1 ? (float)i / (h - 1) : 0.f;
        Uint8 rr = (Uint8)(9 + t * 5);
        Uint8 gg = (Uint8)(13 + t * 6);
        Uint8 bb = (Uint8)(21 + t * 10);
        SDL_SetRenderDrawColor(r, rr, gg, bb, a);
        SDL_RenderDrawLine(r, x, y + i, x + w - 1, y + i);
    }
    // borda
    SDL_SetRenderDrawColor(r, 55, 130, 160, (Uint8)(a * 0.55f));
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderDrawRect(r, &rc);
    // fio de destaque no topo
    SDL_SetRenderDrawColor(r, 95, 190, 220, (Uint8)std::min(255, (int)a + 30));
    SDL_RenderDrawLine(r, x + 1, y + 1, x + w - 2, y + 1);
}

void Hud::panelTitle(SDL_Renderer* r, int x, int y, int w, const std::string& title, SDL_Color c) {
    text(r, fontBig_, x + 10, y + 6, title, c);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 70);
    SDL_RenderDrawLine(r, x + 10, y + 28, x + w - 10, y + 28);
}

void Hud::section(SDL_Renderer* r, int x, int y, int w, const std::string& name, SDL_Color c) {
    text(r, fontSmall_, x, y, name, c);
    int tw = textWidth(fontSmall_, name);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 60);
    SDL_RenderDrawLine(r, x + tw + 8, y + 6, x + w, y + 6);
}

void Hud::bar(SDL_Renderer* r, int x, int y, int w, int h, float v, SDL_Color c) {
    v = clampf(v, 0.f, 1.f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // trilho
    SDL_SetRenderDrawColor(r, 28, 34, 46, 210);
    SDL_Rect bg = {x, y, w, h};
    SDL_RenderFillRect(r, &bg);
    // preenchimento
    int fw = (int)(w * v);
    if (fw > 0) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 235);
        SDL_Rect fg = {x, y, fw, h};
        SDL_RenderFillRect(r, &fg);
        // brilho no topo do preenchimento
        SDL_SetRenderDrawColor(r,
            (Uint8)std::min(255, c.r + 50), (Uint8)std::min(255, c.g + 50),
            (Uint8)std::min(255, c.b + 50), 180);
        SDL_RenderDrawLine(r, x, y, x + fw - 1, y);
    }
    // borda fina
    SDL_SetRenderDrawColor(r, 50, 60, 75, 160);
    SDL_RenderDrawRect(r, &bg);
}

void Hud::chip(SDL_Renderer* r, int x, int y, int w, int h, const std::string& s, SDL_Color fg, Uint8 a) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 22, 30, 42, a);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, fg.r, fg.g, fg.b, (Uint8)(a * 0.6f));
    SDL_RenderDrawRect(r, &rc);
    int tw = textWidth(fontSmall_, s);
    text(r, fontSmall_, x + (w - tw) / 2, y + (h - 12) / 2, s, fg, false);
}

void Hud::drawGodIcon(SDL_Renderer* r, GodAction a, int cx, int cy, int s, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    switch (a) {
        case GodAction::Rain: // gotas
            for (int i = -1; i <= 1; i++) {
                SDL_RenderDrawLine(r, cx + i * 5, cy - s, cx + i * 5 - 2, cy + s);
                SDL_RenderDrawLine(r, cx + i * 5, cy - s, cx + i * 5 + 2, cy + s);
            }
            break;
        case GodAction::PetitionFruit: // fruta
            fillCircle(r, cx, cy, s - 1);
            SDL_RenderDrawLine(r, cx, cy - s, cx + 3, cy - s - 3);
            break;
        case GodAction::Asteroid: // estrela / impacto
            SDL_RenderDrawLine(r, cx - s, cy - s, cx + s, cy + s);
            SDL_RenderDrawLine(r, cx + s, cy - s, cx - s, cy + s);
            SDL_RenderDrawLine(r, cx - s, cy, cx + s, cy);
            SDL_RenderDrawLine(r, cx, cy - s, cx, cy + s);
            break;
        case GodAction::Frost: // flocos
            SDL_RenderDrawLine(r, cx - s, cy, cx + s, cy);
            SDL_RenderDrawLine(r, cx - s / 2, cy - s, cx + s / 2, cy + s);
            SDL_RenderDrawLine(r, cx + s / 2, cy - s, cx - s / 2, cy + s);
            break;
        default: break;
    }
}

SDL_Color tileColor(const TileData& t, int x, int y) {
    // variação suave por posição (sem grade dura)
    float n1 = (float)(((x * 73856093) ^ (y * 19349663)) & 255) / 255.f - 0.5f;
    float n2 = (float)(((x * 83492791) ^ (y * 1299709)) & 255) / 255.f - 0.5f;
    float var = n1 * 0.6f + n2 * 0.4f;
    SDL_Color c;
    switch (t.type) {
        case Tile::DeepWater: c = {22, 48, 96, 255}; break;
        case Tile::Water: c = {38, 92, 160, 255}; break;
        case Tile::Sand: c = {196, 178, 128, 255}; break;
        case Tile::Grass: c = {78, 130, 62, 255}; break;
        case Tile::Forest: c = {48, 100, 48, 255}; break;
        case Tile::Rock: c = {110, 110, 118, 255}; break;
        case Tile::Ice: c = {190, 220, 235, 255}; break;
        case Tile::Soil: c = {120, 88, 58, 255}; break;
        default: c = {255, 0, 255, 255}; break;
    }
    // tom do bioma: pradaria mais amarelada, pântano mais esverdeado,
    // deserto mais claro, montanha mais cinza, tundra mais pálida
    switch (t.biome) {
        case Biome::Grassland:
            c.r = (Uint8)std::min(255, c.r + 14); c.g = (Uint8)std::min(255, c.g + 8);
            break;
        case Biome::Wetland:
            c.g = (Uint8)std::min(255, c.g + 18); c.b = (Uint8)std::min(255, c.b + 10);
            break;
        case Biome::Desert:
            c.r = (Uint8)std::min(255, c.r + 12); c.g = (Uint8)std::min(255, c.g + 10);
            c.b = (Uint8)std::max(0, c.b - 8);
            break;
        case Biome::Mountain:
            c.r = (Uint8)std::min(255, c.r + 6); c.g = (Uint8)std::min(255, c.g + 6);
            c.b = (Uint8)std::min(255, c.b + 8);
            break;
        case Biome::Tundra:
            c.r = (Uint8)std::min(255, c.r + 18); c.g = (Uint8)std::min(255, c.g + 16);
            c.b = (Uint8)std::min(255, c.b + 14);
            break;
        case Biome::Coast:
            // areia mais clara na costa
            if (t.type == Tile::Sand) { c.r = (Uint8)std::min(255, c.r + 10); c.g = (Uint8)std::min(255, c.g + 8); }
            break;
        default: break;
    }
    float m = t.moisture;
    c.r = (Uint8)clampf(c.r * (1.f - m * 0.10f) + var * 22.f, 0.f, 255.f);
    c.g = (Uint8)clampf(c.g * (1.f - m * 0.04f) + var * 24.f, 0.f, 255.f);
    c.b = (Uint8)clampf(c.b * (1.f + m * 0.06f) + var * 20.f, 0.f, 255.f);
    return c;
}

// cor da fauna por tipo (espelha main.cpp)
static SDL_Color faunaCol(int type) {
    switch (type) {
        case 0: return {95, 205, 228, 255};
        case 1: return {91, 110, 225, 255};
        case 2: return {68, 149, 100, 255};
        case 3: return {180, 150, 90, 255};
        case 4: return {140, 130, 115, 255};
        case 5: return {40, 70, 140, 255};
        case 6: return {160, 55, 50, 255};
        case 7: return {220, 220, 240, 255};
        case 8: return {180, 140, 80, 255};
        case 9: return {110, 150, 80, 255};
        case 10: return {100, 110, 125, 255};
        case 11: return {120, 80, 50, 255};
        case 12: return {70, 95, 155, 255};
        case 13: return {180, 140, 110, 255};
        case 14: return {160, 130, 90, 255};
        case 15: return {200, 220, 80, 255};
        case 16: return {70, 110, 110, 255};
        case 17: return {130, 120, 80, 255};
        default: return {200, 200, 200, 255};
    }
}

void Hud::draw(SDL_Renderer* r, const HudState& st) {
    const int winW = st.winW > 0 ? st.winW : 1280;
    const int winH = st.winH > 0 ? st.winH : 720;
    char buf[320];
    int S = (int)(uiScale_ + 0.5f);
    if (S < 1) S = 1;
    const int fs = 14 * S, fsm = 12 * S, fsb = 18 * S;
    (void)fs; (void)fsm; (void)fsb;

    // ================= BARRA SUPERIOR =================
    const int topH = 52;
    panel(r, 0, 0, winW, topH, 210);

    // logo + versão
    text(r, fontBig_, 14, 6, "SKC E-195", {120, 220, 240, 255});
    // era em "badge" (nome + período geológico)
    if (st.eco) {
        std::string era = std::string(eraName(st.eco->era()));
        // mostra período geológico em parênteses quando relevante
        const char* per = eraPeriod(st.eco->era());
        if (per && per[0] && st.eco->era() != Era::Silence) {
            era += " · ";
            era += per;
        }
        int ew = textWidth(font_, era) + 18;
        SDL_Color ec = st.eco->era() == Era::Cambrian ? SDL_Color{255, 160, 80, 255}
                     : SDL_Color{255, 210, 130, 255};
        chip(r, 140, 8, ew, 20, era, ec, 120);
    }
    if (st.sims && st.sims->totalGenerations() > 0) {
        std::string ce = st.sims->culturalEraName();
        int cw = textWidth(font_, ce) + 18;
        int cx0 = 140;
        if (st.eco) cx0 = 140 + textWidth(font_, eraName(st.eco->era())) + 26;
        chip(r, cx0, 8, cw, 20, ce, {180, 220, 180, 255}, 120);
    }

    // relógio do dia + texto (linha de baixo da barra)
    {
        int ccx = 22, ccy = 38;
        int rad = 9;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 40, 50, 65, 220);
        fillCircle(r, ccx, ccy, rad);
        SDL_SetRenderDrawColor(r, 12, 16, 24, 255);
        fillCircle(r, ccx, ccy, rad - 2);
        float ang = st.dayFrac * 2.f * 3.14159265f - 3.14159265f / 2.f;
        int sxp = ccx + (int)(std::cos(ang) * (rad - 3));
        int syp = ccy + (int)(std::sin(ang) * (rad - 3));
        bool day = st.light > 0.4f;
        SDL_SetRenderDrawColor(r,
            day ? 250 : 180, day ? 210 : 200, day ? 90 : 255, 255);
        fillCircle(r, sxp, syp, 3);
        // arco de progresso
        SDL_SetRenderDrawColor(r, day ? 240 : 100, day ? 200 : 120, day ? 80 : 200, 110);
        int segs = 20;
        int lit = (int)(st.dayFrac * segs);
        for (int i = 0; i < lit; i++) {
            float a0 = (float)i / segs * 2.f * 3.14159265f - 3.14159265f / 2.f;
            float a1 = (float)(i + 0.7f) / segs * 2.f * 3.14159265f - 3.14159265f / 2.f;
            SDL_RenderDrawLine(r,
                ccx + (int)(std::cos(a0) * (rad + 2)), ccy + (int)(std::sin(a0) * (rad + 2)),
                ccx + (int)(std::cos(a1) * (rad + 2)), ccy + (int)(std::sin(a1) * (rad + 2)));
        }
        snprintf(buf, sizeof(buf), "Dia %u", st.day);
        text(r, font_, 38, 28, buf, {220, 228, 238, 255});
        // estação
        SDL_Color sc = {180, 220, 180, 255};
        std::string sn = st.seasonName;
        if (sn == "Verao" || sn == "Verão") sc = {255, 200, 90, 255};
        else if (sn == "Outono") sc = {230, 150, 80, 255};
        else if (sn == "Inverno") sc = {160, 200, 255, 255};
        int sw = textWidth(fontSmall_, sn) + 12;
        chip(r, 96, 30, sw, 18, sn, sc, 110);
        // fase do dia
        text(r, fontSmall_, 96 + sw + 8, 32, st.phaseName, {160, 175, 190, 255});
    }

    // centro: velocidade / pausa / cambriano
    {
        int bw = 90;
        int bx = (winW - bw) / 2;
        if (st.eco && st.eco->cambrianActive()) {
            // Cambriano pulsando — o mar ferve
            Uint8 pulse = (Uint8)(140 + 80.f * std::sin((float)SDL_GetTicks() * 0.008f));
            chip(r, bx - 40, 14, bw + 80, 24, "CAMBRIANO!", {255, 200, 120, 255}, pulse);
        } else if (st.paused) {
            chip(r, bx, 14, bw, 24, "PAUSADO", {255, 180, 120, 255}, 180);
        } else {
            snprintf(buf, sizeof(buf), "x%.1f", st.speed);
            chip(r, bx, 14, bw, 24, buf, {180, 220, 240, 255}, 140);
        }
    }

    // direita: vivos / fps / umidade
    int rx = winW - 14;
    // umidade
    {
        int uw = 70;
        bar(r, winW - 220, 14, uw, 8, st.humidity, {60, 140, 220, 255});
        text(r, fontSmall_, winW - 220, 26, "agua", {150, 165, 180, 255});
        rx = winW - 220 - 10;
    }
    // vivos
    snprintf(buf, sizeof(buf), "%d vivos", st.creatureCount);
    {
        int tw = textWidth(font_, buf);
        rx -= tw + 14;
        text(r, font_, rx, 16, buf, {200, 214, 226, 255});
    }
    // fps discreto
    snprintf(buf, sizeof(buf), "%d fps", st.fps);
    {
        int tw = textWidth(fontSmall_, buf);
        rx -= tw + 12;
        SDL_Color fc = st.fps >= 50 ? SDL_Color{110, 160, 130, 255}
                     : st.fps >= 25 ? SDL_Color{200, 180, 110, 255}
                                    : SDL_Color{220, 110, 100, 255};
        text(r, fontSmall_, rx, 18, buf, fc);
    }

    int y = topH + 8;
    const int leftX = 10;
    const int leftW = 300;

    // banner de edição
    if (st.editMode) {
        panel(r, 0, y, winW, 26, 200);
        snprintf(buf, sizeof(buf), "MUNDO  escova: %s  raio %d  -  botao esq pinta, shift+roda = raio", tileName(st.brush), st.brushRadius);
        text(r, font_, 10, y + 4, buf, {240, 200, 120, 255});
        y += 34;
    }

    // ============ PAINEL DE INSPERÇÃO (rolável) ============
    // troca de ser zera a rolagem
    {
        int curId = st.selected ? st.selected->id
                  : st.selectedGrave ? 1000000 + st.selectedGrave->id
                  : (st.selectedFauna ? st.selectedFauna->id : -1);
        if (curId != lastInspectId_) {
            scrollInspect_ = 0.f;
            lastInspectId_ = curId;
        }
    }
    int inspectH = 0;
    if (st.selected) inspectH = std::min(520, winH - topH - 100);
    else if (st.selectedGrave) inspectH = std::min(440, winH - topH - 100);
    else if (st.selectedFauna) inspectH = 210;
    int inspectY = y;

    // rolagem: roda do mouse sobre o painel
    bool mouseOverInspect = st.selected || st.selectedFauna || st.selectedGrave;
    if (mouseOverInspect && st.uiWheel != 0.f) {
        float mxw = st.mouseWinX, myw = st.mouseWinY;
        if (mxw >= leftX && mxw <= leftX + leftW && myw >= inspectY && myw <= inspectY + inspectH) {
            scrollInspect_ -= st.uiWheel * 24.f;
        }
    }

    // ---- TÚMULO: linha do tempo da vida ----
    if (st.selectedGrave) {
        const Grave& g = *st.selectedGrave;
        panel(r, leftX, inspectY, leftW, inspectH, 205);
        std::string title = g.name + (g.surname.empty() ? "" : " " + g.surname);
        panelTitle(r, leftX, inspectY, leftW, title, {190, 180, 160, 255});
        SDL_Rect clip = {leftX + 2, inspectY + 32, leftW - 4, inspectH - 36};
        SDL_RenderSetClipRect(r, &clip);
        int px = leftX + 12;
        int ty = inspectY + 38 - (int)scrollInspect_;
        char line[256];
        snprintf(line, sizeof(line), "%s   %d dias   filhos %d",
                 g.male ? "macho" : "femea", (int)g.age, g.children);
        text(r, fontSmall_, px, ty, line, {180, 175, 165, 255});
        ty += 16;
        section(r, px, ty, leftW - 24, "CRONICA DA VIDA", {220, 200, 150, 255});
        ty += 14;
        if (g.chronicle.empty()) {
            text(r, fontSmall_, px, ty, "sem registros", {130, 130, 140, 255}, false);
            ty += 12;
        } else {
            for (const auto& ev : g.chronicle) {
                SDL_Color lc = {190, 195, 200, 255};
                // destaca morte e nascimento
                if (ev.text.find("partiu") != std::string::npos ||
                    ev.text.find("nasceu") != std::string::npos)
                    lc = {230, 200, 150, 255};
                else if (ev.text.find("odeio") != std::string::npos ||
                         ev.text.find("vinganca") != std::string::npos)
                    lc = {220, 130, 120, 255};
                else if (ev.text.find("acasalou") != std::string::npos ||
                         ev.text.find("filho") != std::string::npos ||
                         ev.text.find("adot") != std::string::npos)
                    lc = {240, 160, 180, 255};
                snprintf(line, sizeof(line), "dia %u: %s", ev.day, ev.text.c_str());
                // quebra linha longa
                std::string msg = line;
                if (msg.size() > 48) msg = msg.substr(0, 46) + "..";
                text(r, fontSmall_, px, ty, msg, lc, false);
                ty += 12;
            }
        }
        // scrollbar
        int contentH = ty + (int)scrollInspect_ - inspectY - 38;
        int viewH = inspectH - 36;
        if (contentH > viewH) {
            int sbX = leftX + leftW - 6;
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 40, 50, 65, 180);
            SDL_Rect track = {sbX, inspectY + 34, 3, inspectH - 40};
            SDL_RenderFillRect(r, &track);
            float frac = (float)viewH / contentH;
            int thumbH = std::max(20, (int)(track.h * frac));
            int maxScroll = contentH - viewH;
            float sp = maxScroll > 0 ? scrollInspect_ / maxScroll : 0.f;
            int thumbY = track.y + (int)((track.h - thumbH) * sp);
            SDL_SetRenderDrawColor(r, 180, 160, 120, 220);
            SDL_Rect thumb = {sbX, thumbY, 3, thumbH};
            SDL_RenderFillRect(r, &thumb);
        }
        SDL_RenderSetClipRect(r, nullptr);
        {
            int maxScroll = std::max(0, contentH - viewH);
            scrollInspect_ = clampf(scrollInspect_, 0.f, (float)maxScroll);
        }
        y = inspectY + inspectH + 8;
    }
    else if (st.selected) {
        const Creature& c = *st.selected;
        panel(r, leftX, inspectY, leftW, inspectH, 205);
        // título
        snprintf(buf, sizeof(buf), "%s  (%s)", c.fullName().c_str(), c.male ? "M" : "F");
        panelTitle(r, leftX, inspectY, leftW, buf,
                   c.male ? SDL_Color{180, 210, 255, 255} : SDL_Color{255, 180, 210, 255});

        // conteúdo rolável
        SDL_Rect clip = {leftX + 2, inspectY + 32, leftW - 4, inspectH - 36};
        SDL_RenderSetClipRect(r, &clip);
        int px = leftX + 12;
        int ty = inspectY + 38 - (int)scrollInspect_;
        char line[256];

        snprintf(line, sizeof(line), "%s   %.1f dias   filhos %d   intel %.2f   era %d",
                 stateName(c.state), c.age, c.children, c.g.intelligence, c.culturalLevel);
        text(r, fontSmall_, px, ty, line, {200, 210, 220, 255});
        ty += 18;

        // ---- COMPANHEIRO (monogamia) ----
        {
            const Creature* mate = c.em.mateId >= 0 ? st.sims->get(c.em.mateId) : nullptr;
            if (mate && mate->alive()) {
                snprintf(line, sizeof(line), "COMPANHEIRO: %s %s  amor %.2f",
                         mate->name.c_str(),
                         mate->surname == c.surname && !c.surname.empty() ? "(mesma linhagem)" : "",
                         c.em.love);
                text(r, fontSmall_, px, ty, line, {255, 150, 180, 255});
                ty += 14;
            } else if (c.em.mateId >= 0) {
                // tinha par, perdeu
                const Creature* old = st.sims->get(c.em.mateId);
                snprintf(line, sizeof(line), "viuvo de %s   tristeza %.2f",
                         old ? old->name.c_str() : "?", c.em.grief);
                text(r, fontSmall_, px, ty, line, {170, 150, 190, 255});
                ty += 14;
            } else if (!c.juvenile()) {
                snprintf(line, sizeof(line), "solteiro   desejo familia %.2f", c.em.desireFamily);
                text(r, fontSmall_, px, ty, line, {160, 170, 180, 255});
                ty += 14;
            }
        }

        // melhor amigo
        {
            int bfId = -1;
            float bfAff = 0.35f;
            for (const auto& b : c.bonds) {
                if (b.affection < bfAff || b.id == c.em.mateId) continue;
                bfId = b.id;
                bfAff = b.affection;
            }
            if (bfId >= 0) {
                const Creature* fr = st.sims->get(bfId);
                if (fr && fr->alive()) {
                    snprintf(line, sizeof(line), "AMIGO: %s  afeto %.2f", fr->name.c_str(), bfAff);
                    text(r, fontSmall_, px, ty, line, {140, 210, 180, 255});
                    ty += 14;
                }
            }
        }
        // inimigo de sangue / túmulo
        if (!c.bloodEnemy.empty()) {
            snprintf(line, sizeof(line), "INIMIGO DE SANGUE: %s", c.bloodEnemy.c_str());
            text(r, fontSmall_, px, ty, line, {230, 120, 100, 255});
            ty += 14;
        }
        if (c.graveX >= 0.f && !c.graveName.empty()) {
            snprintf(line, sizeof(line), "tumulo de %s", c.graveName.c_str());
            text(r, fontSmall_, px, ty, line, {160, 150, 170, 255});
            ty += 14;
        }

        // vitais lado a lado
        text(r, fontSmall_, px, ty, "Energia", {180, 180, 180, 255});
        bar(r, px + 62, ty + 1, 90, 8, c.energy / 100.f, {80, 200, 100, 255});
        text(r, fontSmall_, px + 162, ty, "Agua", {180, 180, 180, 255});
        bar(r, px + 200, ty + 1, 70, 8, c.hydration / 100.f, {70, 150, 230, 255});
        ty += 16;

        section(r, px, ty, leftW - 24, "GENOMA", {120, 220, 240, 255});
        ty += 14;
        snprintf(line, sizeof(line), "vel %.2f  metab %.2f  vis %.1f  porte %.2f  aggro %.2f",
                 c.g.speed, c.g.metabolism, c.g.vision, c.g.size, c.g.aggression);
        text(r, fontSmall_, px, ty, line, {190, 200, 210, 255});
        ty += 13;
        snprintf(line, sizeof(line), "fert %.2f  vigor %.2f  vida %.0f  soc %.2f",
                 c.g.fertility, c.g.robustness, c.g.lifespan, c.g.sociability);
        text(r, fontSmall_, px, ty, line, {190, 200, 210, 255});
        ty += 17;

        section(r, px, ty, leftW - 24, "APRENDIZADO", {120, 220, 240, 255});
        ty += 14;
        snprintf(line, sizeof(line), "fruta %.2f  veneno %.2f  discrim %.2f",
                 c.k.plant_value, c.k.toxic_aversion, c.k.discrimination);
        text(r, fontSmall_, px, ty, line, {180, 210, 170, 255});
        ty += 13;
        snprintf(line, sizeof(line), "antidoto %.2f", c.k.antidote);
        text(r, fontSmall_, px, ty, line, {140, 220, 200, 255});
        ty += 13;
        if (c.k.tool_skill > 0.01f || c.k.fire_knowledge > 0.01f) {
            snprintf(line, sizeof(line), "ferramenta %.2f  fogo %.2f", c.k.tool_skill, c.k.fire_knowledge);
            text(r, fontSmall_, px, ty, line, {255, 180, 100, 255});
            ty += 15;
        }

        // emoções em 2 colunas
        section(r, px, ty, leftW - 24, "SENTIMENTOS", {255, 160, 180, 255});
        ty += 14;
        auto emo2 = [&](int col, const char* n, float v, SDL_Color bc) {
            int bx = px + col * 148;
            text(r, fontSmall_, bx, ty, n, {160, 165, 175, 255});
            bar(r, bx + 72, ty + 1, 64, 6, v, bc);
        };
        struct { const char* n; float v; SDL_Color c; } emos[] = {
            {"amor", c.em.love, {240, 100, 150, 255}},
            {"conforto", c.em.comfort, {120, 210, 140, 255}},
            {"felicidade", c.em.happiness, {255, 210, 90, 255}},
            {"prazer", c.em.satisfaction, {250, 190, 80, 255}},
            {"seguranca", c.em.security, {100, 180, 220, 255}},
            {"solidao", c.em.loneliness, {150, 150, 170, 255}},
            {"medo", c.em.fear, {140, 160, 230, 255}},
            {"raiva", c.em.anger, {230, 90, 70, 255}},
            {"LUTO", c.em.grief, {180, 140, 210, 255}},
            {"respeito", c.em.respect, {200, 190, 160, 255}},
            {"familia", c.em.desireFamily, {240, 160, 190, 255}},
            {"orgulho", c.em.pride, {140, 200, 220, 255}},
            {"culpa", c.em.guilt, {180, 120, 160, 255}},
            {"vergonha", c.em.shame, {160, 100, 140, 255}},
        };
        for (int i = 0; i < 14; i++) {
            if (i % 2 == 0 && i > 0) ty += 11;
            emo2(i % 2, emos[i].n, emos[i].v, emos[i].c);
        }
        ty += 16;

        section(r, px, ty, leftW - 24, "CORPO E MENTE", {200, 180, 160, 255});
        ty += 14;
        snprintf(line, sizeof(line), "forca %.2f  resis %.2f  agil %.2f  saude %.2f",
                 c.body.strength, c.body.endurance, c.body.agility, c.body.health);
        text(r, fontSmall_, px, ty, line, {190, 175, 160, 255});
        ty += 13;
        snprintf(line, sizeof(line), "sabedoria %.2f  memoria %.2f  criativ %.2f  vontade %.2f",
                 c.mind.wisdom, c.mind.memory, c.mind.creativity, c.mind.willpower);
        text(r, fontSmall_, px, ty, line, {170, 190, 210, 255});
        ty += 17;

        section(r, px, ty, leftW - 24, "MORAL", {255, 180, 180, 255});
        ty += 14;
        text(r, fontSmall_, px, ty, "moral", {170, 170, 180, 255});
        bar(r, px + 48, ty + 1, leftW - 72, 7, c.morality,
            c.morality > 0.6f ? SDL_Color{100, 220, 120, 255} :
            (c.morality < 0.35f ? SDL_Color{220, 80, 70, 255} : SDL_Color{200, 180, 100, 255}));
        ty += 12;
        snprintf(line, sizeof(line), "reputacao %s%.2f", c.reputation >= 0 ? "+" : "", c.reputation);
        text(r, fontSmall_, px, ty, line,
             c.reputation < -0.3f ? SDL_Color{220, 100, 80, 255} :
             (c.reputation > 0.3f ? SDL_Color{120, 220, 140, 255} : SDL_Color{170, 170, 180, 255}));
        ty += 16;

        if (!c.bonds.empty()) {
            const Bond& b = c.bonds.back();
            snprintf(line, sizeof(line), "Laco #%d  conf %.2f  aff %.2f  rancor %.2f",
                     b.id, b.trust, b.affection, b.grudge);
            text(r, fontSmall_, px, ty, line, {200, 190, 160, 255});
            ty += 16;
        }

        // personalidade 2 colunas
        section(r, px, ty, leftW - 24, "PERSONALIDADE", {255, 160, 220, 255});
        ty += 14;
        auto pers2 = [&](int col, const char* n, float v, SDL_Color bc) {
            int bx = px + col * 148;
            text(r, fontSmall_, bx, ty, n, {160, 165, 175, 255});
            bar(r, bx + 88, ty + 1, 48, 6, v, bc);
        };
        struct { const char* n; float v; SDL_Color c; } pers[] = {
            {"sexo", c.pers.sexuality, {240, 120, 180, 255}},
            {"ousadia", c.pers.boldness, {240, 160, 80, 255}},
            {"pensa", c.pers.thinker, {120, 200, 240, 255}},
            {"ganancia", c.pers.greed, {200, 180, 80, 255}},
            {"preguica", c.pers.laziness, {150, 150, 160, 255}},
            {"empatia", c.pers.empathy, {140, 220, 160, 255}},
            {"vagar", c.pers.wanderlust, {180, 160, 220, 255}},
            {"apego", c.pers.attachment, {240, 140, 170, 255}},
            {"otimismo", c.pers.optimism, {255, 220, 100, 255}},
        };
        for (int i = 0; i < 9; i++) {
            if (i % 2 == 0 && i > 0) ty += 11;
            pers2(i % 2, pers[i].n, pers[i].v, pers[i].c);
        }
        ty += 16;

        // o que aprendeu SOZINHO (descobertas próprias)
        section(r, px, ty, leftW - 24, "DESCOBERTAS PROPRIAS", {255, 230, 140, 255});
        ty += 14;
        {
            std::vector<const Idea*> own;
            for (auto& i : c.ideas)
                if (i.isOwnDiscovery() && !i.isLie) own.push_back(&i);
            std::sort(own.begin(), own.end(),
                      [](const Idea* a, const Idea* b) { return a->strength > b->strength; });
            if (own.empty()) {
                text(r, fontSmall_, px, ty, "ainda nao descobriu nada sozinho", {120, 130, 140, 255}, false);
                ty += 12;
            } else {
                int shown = (int)std::min<size_t>(own.size(), 5);
                for (int i = 0; i < shown; i++) {
                    const Idea* id = own[i];
                    SDL_Color ic = id->strength > 0.6f ? SDL_Color{255, 230, 130, 255}
                                                       : SDL_Color{210, 200, 160, 255};
                    text(r, fontSmall_, px, ty, "* " + id->text, ic, false);
                    bar(r, px + 20, ty + 11, leftW - 140, 4, id->strength, {240, 200, 100, 255});
                    ty += 16;
                }
                if ((int)own.size() > shown) {
                    snprintf(line, sizeof(line), "... +%d outras descobertas", (int)own.size() - shown);
                    text(r, fontSmall_, px, ty, line, {140, 140, 130, 255}, false);
                    ty += 12;
                }
            }
        }
        ty += 4;

        section(r, px, ty, leftW - 24, "IDEIAS", {255, 220, 140, 255});
        ty += 14;
        if (c.ideas.empty()) {
            text(r, fontSmall_, px, ty, "ainda nao tem certezas", {130, 140, 150, 255}, false);
            ty += 12;
        } else {
            int shown = (int)std::min<size_t>(c.ideas.size(), 6);
            std::vector<const Idea*> sorted;
            for (auto& i : c.ideas) sorted.push_back(&i);
            std::sort(sorted.begin(), sorted.end(),
                      [](const Idea* a, const Idea* b) { return a->strength > b->strength; });
            for (int i = 0; i < shown; i++) {
                const Idea* id = sorted[i];
                SDL_Color ic = id->isLie ? SDL_Color{220, 100, 100, 255} :
                    (id->morality > 0.65f ? SDL_Color{140, 220, 150, 255} :
                     (id->morality < 0.35f ? SDL_Color{220, 120, 100, 255} : SDL_Color{200, 200, 180, 255}));
                std::string prefix = id->isLie ? "[MENTIRA] " : "";
                text(r, fontSmall_, px, ty, "\"" + prefix + id->text + "\"", ic, false);
                ty += 11;
                bar(r, px + 20, ty, leftW - 140, 5, id->strength, {240, 200, 100, 255});
                const char* origin = id->isOwnDiscovery() ? "propria"
                    : (id->fromName.empty() ? "propria" : id->fromName.c_str());
                snprintf(line, sizeof(line), "%s", origin);
                text(r, fontSmall_, px + leftW - 100, ty - 2, line,
                     id->isOwnDiscovery() ? SDL_Color{220, 200, 130, 255} : SDL_Color{140, 150, 160, 255}, false);
                ty += 10;
            }
            if ((int)c.ideas.size() > shown) {
                snprintf(line, sizeof(line), "... +%d outras", (int)c.ideas.size() - shown);
                text(r, fontSmall_, px, ty, line, {120, 130, 140, 255}, false);
                ty += 11;
            }
        }

        section(r, px, ty, leftW - 24, "O QUE QUER NO PAR", {220, 160, 200, 255});
        ty += 14;
        snprintf(line, sizeof(line), "forca %.2f  mentes %.2f  habs %.2f  porte %.2f",
                 c.prefs.likeStrength, c.prefs.likeSmarts, c.prefs.likeSkill, c.prefs.likeSize);
        text(r, fontSmall_, px, ty, line, {200, 170, 200, 255});
        ty += 13;
        snprintf(line, sizeof(line), "calma %.2f  juventude %.2f  cabelo %.2f",
                 c.prefs.likeCalm, c.prefs.likeYoung, c.prefs.likeHair);
        text(r, fontSmall_, px, ty, line, {200, 170, 200, 255});
        ty += 13;
        snprintf(line, sizeof(line), "voz %.2f  genio %.2f  coragem %.2f",
                 c.prefs.likeVoice, c.prefs.likeGenerosity, c.prefs.likeCourage);
        text(r, fontSmall_, px, ty, line, {200, 170, 200, 255});
        ty += 13;
        snprintf(line, sizeof(line), "lealdade %.2f  humor %.2f  riqueza %.2f",
                 c.prefs.likeLoyalty, c.prefs.likeHumor, c.prefs.likeWealth);
        text(r, fontSmall_, px, ty, line, {200, 170, 200, 255});
        ty += 16;

        if (!c.teachings.empty()) {
            section(r, px, ty, leftW - 24, "ENSINAMENTOS", {180, 220, 180, 255});
            ty += 14;
            int shown = (int)std::min<size_t>(c.teachings.size(), 4);
            for (int i = (int)c.teachings.size() - shown; i < (int)c.teachings.size(); i++) {
                const TeachingRecord& t = c.teachings[i];
                snprintf(line, sizeof(line), "de %s: %s (%.2f)", t.teacherName.c_str(), t.lesson.c_str(), t.value);
                text(r, fontSmall_, px, ty, line, {160, 200, 160, 255}, false);
                ty += 12;
            }
            ty += 4;
        }
        if (!c.speciesBonds.empty()) {
            const SpeciesBond& sb = c.speciesBonds.back();
            section(r, px, ty, leftW - 24, "SIMBIOSE", {180, 200, 255, 255});
            ty += 14;
            snprintf(line, sizeof(line), "com %s: conf %.2f  afeto %.2f",
                     faunaName((FaunaType)sb.faunaType), sb.trust, sb.affection);
            text(r, fontSmall_, px, ty, line, {160, 180, 220, 255});
            ty += 16;
        }

        section(r, px, ty, leftW - 24, "FAMILIA", {255, 200, 160, 255});
        ty += 14;
        if (st.sims) {
            const Creature* pa = c.parentA >= 0 ? st.sims->get(c.parentA) : nullptr;
            const Creature* pb = c.parentB >= 0 ? st.sims->get(c.parentB) : nullptr;
            snprintf(line, sizeof(line), "pais: %s | %s", pa ? pa->name.c_str() : "?", pb ? pb->name.c_str() : "?");
            text(r, fontSmall_, px, ty, line, {190, 180, 160, 255}, false);
            ty += 13;
            if (c.em.mateId >= 0) {
                const Creature* mate = st.sims->get(c.em.mateId);
                if (mate && mate->alive()) {
                    snprintf(line, sizeof(line), "companheiro: %s (amor %.2f)", mate->name.c_str(), c.em.love);
                    text(r, fontSmall_, px, ty, line, {240, 140, 170, 255}, false);
                    ty += 12;
                }
            }
            if (!c.childrenIds.empty()) {
                int show = (int)std::min<size_t>(c.childrenIds.size(), 3);
                std::string kids = "filhos:";
                for (int i = 0; i < show; i++) {
                    const Creature* k = st.sims->get(c.childrenIds[c.childrenIds.size() - show + i]);
                    if (k) kids += " " + k->name;
                }
                if ((int)c.childrenIds.size() > show) kids += " ...";
                text(r, fontSmall_, px, ty, kids, {160, 210, 160, 255}, false);
                ty += 13;
            }
            if (!c.surname.empty()) {
                snprintf(line, sizeof(line), "linhagem: %s", c.surname.c_str());
                text(r, fontSmall_, px, ty, line, {180, 170, 140, 255}, false);
                ty += 13;
            }
        }

        if (c.communityId >= 0 && st.sims) {
            if (Community* comm = st.sims->community(c.communityId)) {
                section(r, px, ty, leftW - 24, "COMUNIDADE", {120, 230, 160, 255});
                ty += 14;
                snprintf(line, sizeof(line), "%s (%d membros, %d construções)",
                         comm->name.c_str(), comm->size(), comm->totalBuildings());
                text(r, fontSmall_, px, ty, line, {160, 220, 170, 255});
                ty += 13;
                snprintf(line, sizeof(line), "%s%s%s  ideias: %d",
                         comm->hasFire() ? "fogueira " : "",
                         comm->hasHut() ? "toca " : "",
                         (!comm->hasFire() && !comm->hasHut()) ? "acampamento" : "",
                         (int)comm->groupIdeas.size());
                text(r, fontSmall_, px, ty, line, {140, 200, 150, 255});
                ty += 16;
            }
        }

        section(r, px, ty, leftW - 24, "OBJETIVO", {255, 210, 120, 255});
        ty += 14;
        const char* goal = "viver e sentir o mundo";
        if (c.state == CState::SeekFood) goal = "comer";
        else if (c.state == CState::SeekWater) goal = "beber";
        else if (c.state == CState::SeekMate) goal = "amor / companhia";
        else if (c.state == CState::Rest) goal = "descansar";
        else if (c.state == CState::Sick) goal = "sobreviver ao veneno";
        else if (c.state == CState::Eat) goal = "comer e aprender";
        else if (c.state == CState::SeekShelter) goal = "achar abrigo";
        else if (c.state == CState::Fight) goal = "defender-se / vingar";
        else if (c.state == CState::Mourn) goal = "luto";
        text(r, fontSmall_, px, ty, goal, {255, 220, 150, 255});
        ty += 20;

        // indicação de rolagem
        int contentH = ty + (int)scrollInspect_ - inspectY - 38;
        int viewH = inspectH - 36;
        if (contentH > viewH) {
            // scrollbar simples
            int sbX = leftX + leftW - 6;
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 40, 50, 65, 180);
            SDL_Rect track = {sbX, inspectY + 34, 3, inspectH - 40};
            SDL_RenderFillRect(r, &track);
            float frac = (float)viewH / contentH;
            int thumbH = std::max(20, (int)(track.h * frac));
            int maxScroll = contentH - viewH;
            float sp = maxScroll > 0 ? scrollInspect_ / maxScroll : 0.f;
            int thumbY = track.y + (int)((track.h - thumbH) * sp);
            SDL_SetRenderDrawColor(r, 90, 170, 200, 220);
            SDL_Rect thumb = {sbX, thumbY, 3, thumbH};
            SDL_RenderFillRect(r, &thumb);
        }
        SDL_RenderSetClipRect(r, nullptr);
        y = inspectY + inspectH + 8;
        // clamp scroll
        {
            int maxScroll = std::max(0, contentH - viewH);
            scrollInspect_ = clampf(scrollInspect_, 0.f, (float)maxScroll);
        }
    } else if (st.selectedFauna && st.selectedFauna->alive()) {
        const Fauna& f = *st.selectedFauna;
        panel(r, leftX, inspectY, leftW, inspectH, 205);
        snprintf(buf, sizeof(buf), "%s #%d", faunaName(f.type), f.id);
        panelTitle(r, leftX, inspectY, leftW, buf, faunaCol((int)f.type));
        int px = leftX + 12;
        int ty = inspectY + 38;
        char line[256];
        snprintf(line, sizeof(line), "%s   %.1f / vida %.0f   filhos %d",
                 f.male ? "macho" : "femea", f.age, f.g.lifespan, f.children);
        text(r, fontSmall_, px, ty, line, {190, 200, 215, 255});
        ty += 18;
        // grupo social
        if (f.groupId >= 0 && st.eco) {
            FaunaGroup* g = nullptr;
            for (auto& gg : st.eco->groups()) if (gg.id == f.groupId) { g = &gg; break; }
            if (g) {
                snprintf(line, sizeof(line), "%s #%d — %d membros%s",
                         g->kindName(), g->id, g->size(),
                         g->leaderId == f.id ? " (lider)" : "");
                text(r, fontSmall_, px, ty, line, {180, 210, 170, 255});
                ty += 14;
            }
        }
        text(r, fontSmall_, px, ty, "Energia", {180, 180, 180, 255});
        bar(r, px + 62, ty + 1, leftW - 86, 8, f.energy / 100.f, {80, 200, 100, 255});
        ty += 13;
        text(r, fontSmall_, px, ty, "Saciedade", {200, 180, 140, 255});
        bar(r, px + 62, ty + 1, leftW - 86, 8, f.satisfaction, {240, 200, 100, 255});
        ty += 15;
        snprintf(line, sizeof(line), "vel %.2f  vis %.1f  porte %.2f  aggro %.2f  nado %.2f",
                 f.g.speed, f.g.vision, f.g.size, f.g.aggression, f.g.swim);
        text(r, fontSmall_, px, ty, line, {170, 180, 195, 255});
        ty += 14;
        // onde viveu de verdade (evolução por habitat)
        {
            const char* hab = f.habitatUse > 0.65f ? "vive no mar"
                            : f.habitatUse < 0.35f ? "vive na terra"
                            : "vive na costa";
            snprintf(line, sizeof(line), "habitat: %s (%.0f%% agua)", hab, f.habitatUse * 100.f);
            text(r, fontSmall_, px, ty, line,
                 f.habitatUse > 0.65f ? SDL_Color{120, 190, 240, 255} :
                 (f.habitatUse < 0.35f ? SDL_Color{180, 200, 140, 255} : SDL_Color{200, 190, 140, 255}));
            ty += 14;
        }
        text(r, fontSmall_, px, ty, Ecosystem::nicheOf(f).c_str(), {140, 200, 160, 255});
        ty += 16;
        if (f.fearOf[(int)FaunaType::SeaPredator] > 0.1f || f.fearOf[(int)FaunaType::LandPredator] > 0.1f ||
            f.fearOf[(int)FaunaType::SkyPredator] > 0.1f || f.fearOf[(int)FaunaType::Shark] > 0.1f) {
            section(r, px, ty, leftW - 24, "MEDO", {160, 150, 190, 255});
            ty += 14;
            snprintf(line, sizeof(line), "Pisc %.2f  Tub %.2f  Lobo %.2f  Falc %.2f",
                     f.fearOf[(int)FaunaType::SeaPredator], f.fearOf[(int)FaunaType::Shark],
                     f.fearOf[(int)FaunaType::LandPredator], f.fearOf[(int)FaunaType::SkyPredator]);
            text(r, fontSmall_, px, ty, line, {170, 160, 190, 255});
            ty += 16;
        }
        section(r, px, ty, leftW - 24, "OBJETIVO", {255, 210, 120, 255});
        ty += 14;
        text(r, fontSmall_, px, ty, Ecosystem::goalOf(f), {255, 230, 160, 255});
        y = inspectY + inspectH + 8;
        scrollInspect_ = 0.f;
    } else {
        scrollInspect_ = 0.f;
    }

    // ============ ESCADA DA VIDA — compacta (21 linhas) ============
    if (st.showLadder && st.eco) {
        LifeLadder L = st.eco->ladder();
        const int rowH = 12;
        const int nRows = 21;
        const int lh = 48 + nRows * rowH + 20;
        int ly = y;
        if (ly + lh > winH - 80) ly = topH + 8;
        panel(r, leftX, ly, leftW, lh, 185);
        text(r, fontBig_, leftX + 10, ly + 4, eraName(st.eco->era()), {255, 210, 130, 255});
        std::string story = eraStory(st.eco->era());
        if (story.size() > 42) story = story.substr(0, 40) + "..";
        text(r, fontSmall_, leftX + 10, ly + 24, story, {160, 180, 195, 255}, false);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 80, 150, 180, 60);
        SDL_RenderDrawLine(r, leftX + 10, ly + 40, leftX + leftW - 10, ly + 40);
        auto row = [&](int i, const char* n, bool has, int count, const char* need) {
            int yy = ly + 44 + i * rowH;
            SDL_Color c = has ? SDL_Color{120, 230, 140, 255} : SDL_Color{100, 110, 120, 255};
            snprintf(buf, sizeof(buf), "%s %s", has ? "*" : " ", n);
            text(r, fontSmall_, leftX + 10, yy, buf, c, false);
            if (has && count > 0) {
                snprintf(buf, sizeof(buf), "%d", count);
                text(r, fontSmall_, leftX + 158, yy, buf, {150, 170, 160, 255}, false);
            } else if (!has) {
                text(r, fontSmall_, leftX + 132, yy, need, {85, 95, 105, 255}, false);
            }
        };
        row(0, "Microbioma", L.microbe, 0, "agua+minerais");
        row(1, "Plancton", L.plankton, 0, "luz+nutrientes");
        row(2, "Plantas", L.plants, 0, "terra umida");
        row(3, "Medusa", L.medusa, L.medusaCount, "plancton (mar)");
        row(4, "Trilobita", L.trilobites, L.trilobiteCount, "biofilme");
        row(5, "Peixe", L.fish, L.fishCount, "medusas");
        row(6, "Anfibio", L.amphibian, L.amphibCount, "peixe + margem");
        row(7, "Peixulmao", L.lungfish, L.lungfishCount, "peixe costa");
        row(8, "Tartaruga", L.turtles, L.turtleCount, "anfibio + costa");
        row(9, "Herbivoro", L.herbivores, L.herbCount, "anfibio + plantas");
        row(10, "Inseto", L.insects, L.insectCount, "plantas");
        row(11, "Piscador", L.seaPred, L.seaPredCount, "peixes (mar)");
        row(12, "Placodermio", L.placoderms, L.placodermCount, "peixe grande");
        row(13, "Tubarao", L.sharks, L.sharkCount, "piscadores");
        row(14, "Baleia", L.whales, L.whaleCount, "herbivoro + oceano");
        row(15, "Lobo", L.landPred, L.landPredCount, "herbivoros");
        row(16, "Urso", L.bears, L.bearCount, "lobo + agua");
        row(17, "Falcon", L.skyPred, L.skyPredCount, "carroneiros");
        row(18, "Carroneiro", L.scavengers, L.scavCount, "mortes");
        row(19, "Verme", L.worms, L.wormCount, "carcoas");
        row(20, "Pensantes", L.sages, L.sageCount, "pressao+tempo");
        text(r, fontSmall_, leftX + 10, ly + lh - 16, L.nextHint(), {230, 200, 140, 255}, false);
    }

    // ============ COLUNA DE POPULAÇÃO (direita) — compacta, espécies vivas primeiro ============
    int popBottomY = topH + 8;
    if (st.eco && winW > 700) {
        const int rw = 122;
        // conta vivas primeiro pra altura dinâmica
        int counts[FAUNA_TYPE_COUNT] = {};
        int living = 0;
        for (const auto& f : st.eco->all()) if (f.alive()) counts[(int)f.type]++;
        for (int i = 0; i < FAUNA_TYPE_COUNT; i++) if (counts[i] > 0) living++;
        int sages = st.sims ? st.sims->aliveCount() : 0;
        if (sages > 0) living++;
        int groupsN = st.eco->groupCount();
        // resumo do mundo
        int treesN = st.world ? st.world->totalTrees() : 0;
        int rows = living + 2; // pensantes + árvores/resumo
        const int rowH = 13;
        const int rh = 22 + rows * rowH + 8;
        int rxp = winW - rw - 10;
        int ryp = topH + 8;
        popBottomY = ryp + rh;
        panel(r, rxp, ryp, rw, rh, 155);
        text(r, fontSmall_, rxp + 8, ryp + 3, "POPULACAO", {140, 210, 230, 255});
        int ry = ryp + 18;
        auto rowPop = [&](int typeIdx, const char* name, int count) {
            if (count <= 0) return;
            SDL_SetRenderDrawColor(r, faunaCol(typeIdx).r, faunaCol(typeIdx).g, faunaCol(typeIdx).b, 255);
            SDL_Rect dot = {rxp + 6, ry + 2, 6, 6};
            SDL_RenderFillRect(r, &dot);
            std::string nm = name;
            if (nm.size() > 10) nm = nm.substr(0, 9) + ".";
            text(r, fontSmall_, rxp + 16, ry, nm, {200, 210, 220, 255}, false);
            snprintf(buf, sizeof(buf), "%d", count);
            text(r, fontSmall_, rxp + rw - 28, ry, buf, {170, 190, 200, 255}, false);
            ry += rowH;
        };
        // vivos primeiro (ordem do enum, mas só quem existe)
        for (int i = 0; i < FAUNA_TYPE_COUNT; i++)
            rowPop(i, faunaName((FaunaType)i), counts[i]);
        // pensantes
        if (sages > 0) {
            SDL_SetRenderDrawColor(r, 140, 180, 240, 255);
            SDL_Rect pdot = {rxp + 6, ry + 2, 6, 6};
            SDL_RenderFillRect(r, &pdot);
            text(r, fontSmall_, rxp + 16, ry, "Pensante", {200, 210, 220, 255}, false);
            snprintf(buf, sizeof(buf), "%d", sages);
            text(r, fontSmall_, rxp + rw - 28, ry, buf, {170, 190, 200, 255}, false);
            ry += rowH;
        }
        // resumo: grupos + árvores
        if (groupsN > 0) {
            SDL_SetRenderDrawColor(r, 200, 200, 140, 255);
            SDL_Rect gdot = {rxp + 6, ry + 2, 6, 6};
            SDL_RenderFillRect(r, &gdot);
            snprintf(buf, sizeof(buf), "Grupos %d", groupsN);
            text(r, fontSmall_, rxp + 16, ry, buf, {200, 200, 160, 255}, false);
            ry += rowH;
        }
        if (treesN > 0) {
            // mini árvore: 2 pixels
            SDL_SetRenderDrawColor(r, 100, 75, 45, 255);
            SDL_RenderDrawPoint(r, rxp + 8, ry + 5);
            SDL_SetRenderDrawColor(r, 55, 130, 45, 255);
            SDL_RenderDrawPoint(r, rxp + 8, ry + 3);
            SDL_RenderDrawPoint(r, rxp + 8, ry + 2);
            snprintf(buf, sizeof(buf), "Arvores %d", treesN);
            text(r, fontSmall_, rxp + 16, ry, buf, {160, 190, 150, 255}, false);
        }
    }

    // ============ LOG DE EVENTOS — cor por categoria ============
    if (st.events && !st.events->empty()) {
        int show = (int)std::min<size_t>(st.events->size(), 8);
        int logW = 440;
        int logH = show * 15 + 18;
        int lx = winW - logW - 12;
        int ly = winH - 78 - logH;
        // não colide com coluna de espécies
        if (ly < popBottomY + 6) ly = popBottomY + 6;
        if (ly < topH + 4) ly = topH + 4;
        panel(r, lx, ly, logW, logH, 140);
        text(r, fontSmall_, lx + 6, ly + 2, "CRONICA", {140, 190, 210, 200});
        // cor por tipo de evento
        auto evColor = [](EvKind k) -> SDL_Color {
            switch (k) {
                case EvKind::Birth:     return {160, 220, 255, 255};
                case EvKind::Death:     return {180, 160, 200, 255};
                case EvKind::Discovery: return {255, 220, 120, 255};
                case EvKind::Build:     return {140, 220, 150, 255};
                case EvKind::War:       return {255, 130, 110, 255};
                case EvKind::Love:      return {255, 160, 190, 255};
                case EvKind::Idea:      return {140, 220, 240, 255};
                case EvKind::Health:    return {230, 200, 140, 255};
                default:                return {170, 195, 175, 255};
            }
        };
        for (int i = 0; i < show; i++) {
            const SimEvent& e = (*st.events)[st.events->size() - show + i];
            // eventos importantes degradam mais devagar
            float fade = clampf(1.4f - e.t * (1.f / e.life), 0.2f, 1.f);
            SDL_Color col = evColor(e.kind);
            col.a = (Uint8)(240 * fade);
            std::string msg = e.text;
            if (msg.size() > 54) msg = msg.substr(0, 52) + "..";
            text(r, fontSmall_, lx + 8, ly + 16 + i * 15, msg, col, false);
        }
    }

    // ============ BARRA DIVINA ============
    if (st.god) {
        int bw = 168, bh = 56, gap = 8;
        int total = (int)GodAction::COUNT * bw + ((int)GodAction::COUNT - 1) * gap;
        int bx0 = (winW - total) / 2;
        int by = winH - bh - 10;
        for (int i = 0; i < (int)GodAction::COUNT; i++) {
            GodAction a = (GodAction)i;
            int x = bx0 + i * (bw + gap);
            panel(r, x, by, bw, bh, 215);
            // ícone
            float cl = st.god->cooldownLeft(a);
            SDL_Color ic = cl > 0.f ? SDL_Color{110, 120, 130, 255} : SDL_Color{255, 230, 160, 255};
            drawGodIcon(r, a, x + 18, by + 20, 8, ic);
            snprintf(buf, sizeof(buf), "[%s] %s", GodSystem::actionKey(a), GodSystem::actionName(a));
            // corta se não couber
            while (textWidth(font_, buf) > bw - 40 && buf[0]) {
                std::string s(buf);
                if (s.size() > 3) s = s.substr(0, s.size() - 2) + ".";
                else break;
                snprintf(buf, sizeof(buf), "%s", s.c_str());
            }
            text(r, font_, x + 34, by + 8, buf,
                 cl > 0.f ? SDL_Color{150, 155, 165, 255} : SDL_Color{230, 220, 180, 255});
            if (cl > 0.f) {
                float cm = st.god->cooldownMax(a);
                float frac = cm > 0.f ? cl / cm : 0.f;
                bar(r, x + 8, by + bh - 12, bw - 16, 5, 1.f - frac, {100, 140, 200, 255});
                snprintf(buf, sizeof(buf), "recarga %.0fs", cl);
                text(r, fontSmall_, x + 34, by + 28, buf, {170, 140, 140, 255});
            } else {
                bar(r, x + 8, by + bh - 12, bw - 16, 5, 1.f, {100, 200, 140, 255});
                text(r, fontSmall_, x + 34, by + 28, "pronto (mouse)", {120, 200, 140, 255});
            }
        }
    }

    // tile sob o mouse — canto inferior esquerdo (acima da barra divina)
    if (st.mouseInWorld && st.world && !st.editMode) {
        int tx = (int)st.mouseX, ty2 = (int)st.mouseY;
        if (st.world->inBounds(tx, ty2)) {
            const TileData& t = st.world->at(tx, ty2);
            snprintf(buf, sizeof(buf), "%s (%s)  arv %u  lenha %u  planct %u",
                     tileName(t.type), biomeName(t.biome), t.trees, t.wood, t.plankton);
            if (t.plant.kind != PlantKind::None) {
                snprintf(buf, sizeof(buf), "%s  %s%s  fruta-c %u",
                         buf,
                         t.plant.kind == PlantKind::Fruit ? "fruta" : "veneno",
                         t.plant.stage >= 2 ? "*" : "",
                         t.fallenFruit);
            } else if (t.fallenFruit > 0) {
                snprintf(buf, sizeof(buf), "%s  fruta-chao %u", buf, t.fallenFruit);
            }
            int tw = textWidth(fontSmall_, buf);
            int px2 = 12;
            int py2 = winH - 78 - 24;
            // não colide com a barra divina (centro embaixo)
            int godTotal = (int)GodAction::COUNT * 176;
            int godX0 = (winW - godTotal) / 2;
            if (px2 + tw + 14 > godX0) py2 = winH - 118;
            panel(r, px2, py2, tw + 14, 22, 175);
            text(r, fontSmall_, px2 + 7, py2 + 4, buf, {200, 215, 225, 255}, false);
        }
    }

    // ============ MINIMAPA ============
    if (st.world && st.winW > 400) {
        int mmSize = 120;
        int mmX = st.winW - mmSize - 10;
        int mmY = st.winH - mmSize - 78;
        panel(r, mmX - 3, mmY - 3, mmSize + 6, mmSize + 6, 185);
        float scaleX = (float)st.world->width() / mmSize;
        float scaleY = (float)st.world->height() / mmSize;
        for (int py = 0; py < mmSize; py++) {
            for (int px = 0; px < mmSize; px++) {
                int wx = (int)(px * scaleX);
                int wy = (int)(py * scaleY);
                if (!st.world->inBounds(wx, wy)) continue;
                const TileData& t = st.world->at(wx, wy);
                SDL_Color c = {30, 30, 40, 255};
                switch (t.type) {
                    case Tile::DeepWater: c = {20, 40, 80, 255}; break;
                    case Tile::Water: c = {40, 80, 140, 255}; break;
                    case Tile::Sand: c = {180, 160, 110, 255}; break;
                    case Tile::Grass: c = {60, 110, 50, 255}; break;
                    case Tile::Forest: c = {35, 80, 35, 255}; break;
                    case Tile::Rock: c = {100, 100, 110, 255}; break;
                    case Tile::Ice: c = {170, 200, 220, 255}; break;
                    case Tile::Soil: c = {100, 75, 50, 255}; break;
                    default: break;
                }
                if (st.world->onFire(wx, wy)) c = {255, 120, 20, 255};
                SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
                SDL_RenderDrawPoint(r, mmX + px, mmY + py);
            }
        }
        // fauna no minimapa
        if (st.eco) {
            for (const auto& f : st.eco->all()) {
                if (!f.alive()) continue;
                int mx2 = mmX + (int)(f.x / scaleX);
                int my2 = mmY + (int)(f.y / scaleY);
                if (mx2 < mmX || mx2 >= mmX + mmSize || my2 < mmY || my2 >= mmY + mmSize) continue;
                SDL_Color fc = faunaCol((int)f.type);
                SDL_SetRenderDrawColor(r, fc.r, fc.g, fc.b, 230);
                SDL_RenderDrawPoint(r, mx2, my2);
            }
        }
        // pensantes
        if (st.sims) {
            for (const auto& c : st.sims->all()) {
                if (!c.alive()) continue;
                int mx2 = mmX + (int)(c.x / scaleX);
                int my2 = mmY + (int)(c.y / scaleY);
                if (mx2 < mmX || mx2 >= mmX + mmSize || my2 < mmY || my2 >= mmY + mmSize) continue;
                SDL_SetRenderDrawColor(r, c.male ? 100 : 240, c.male ? 160 : 150, c.male ? 255 : 190, 255);
                SDL_RenderDrawPoint(r, mx2, my2);
            }
        }
        // comunidades (fogueira brilha)
        if (st.sims) {
            for (const auto& comm : st.sims->communities()) {
                int mx2 = mmX + (int)(comm.x / scaleX);
                int my2 = mmY + (int)(comm.y / scaleY);
                if (mx2 < mmX || mx2 >= mmX + mmSize || my2 < mmY || my2 >= mmY + mmSize) continue;
                if (comm.hasFire()) {
                    SDL_SetRenderDrawColor(r, 255, 180, 60, 255);
                    SDL_RenderDrawPoint(r, mx2, my2);
                    SDL_RenderDrawPoint(r, mx2 + 1, my2);
                } else {
                    SDL_SetRenderDrawColor(r, 200, 200, 160, 200);
                    SDL_RenderDrawPoint(r, mx2, my2);
                }
            }
        }
        // centros de grupos (rebanhos, matilhas...)
        if (st.eco) {
            for (const auto& g : st.eco->groups()) {
                int mx2 = mmX + (int)(g.x / scaleX);
                int my2 = mmY + (int)(g.y / scaleY);
                if (mx2 < mmX || mx2 >= mmX + mmSize || my2 < mmY || my2 >= mmY + mmSize) continue;
                SDL_Color gc = faunaCol((int)g.type);
                // marca em cruz para diferenciar de ponto de animal
                SDL_SetRenderDrawColor(r, gc.r, gc.g, gc.b, 255);
                SDL_RenderDrawPoint(r, mx2, my2);
                SDL_RenderDrawPoint(r, mx2 - 1, my2);
                SDL_RenderDrawPoint(r, mx2 + 1, my2);
                SDL_RenderDrawPoint(r, mx2, my2 - 1);
                SDL_RenderDrawPoint(r, mx2, my2 + 1);
            }
        }
        // marcador da câmera
        SDL_SetRenderDrawColor(r, 255, 255, 255, 220);
        int cx = mmX + (int)(st.camX / scaleX);
        int cy = mmY + (int)(st.camY / scaleY);
        if (cx >= mmX && cx < mmX + mmSize && cy >= mmY && cy < mmY + mmSize) {
            SDL_Rect camMark = {cx - 3, cy - 3, 6, 6};
            SDL_RenderDrawRect(r, &camMark);
        }
        // legenda
        text(r, fontSmall_, mmX, mmY + mmSize + 4, "mapa", {130, 145, 160, 200}, false);
    }

    // omen central
    if (st.omenT > 0.f && !st.omen.empty()) {
        Uint8 a = (Uint8)clampf(st.omenT * 70.f, 0.f, 235.f);
        // faixa de fundo
        int tw = textWidth(fontBig_, st.omen);
        int oh = 30;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 8, 12, 20, (Uint8)(a * 0.65f));
        SDL_Rect ob = {(winW - tw) / 2 - 16, topH + 12, tw + 32, oh};
        SDL_RenderFillRect(r, &ob);
        SDL_SetRenderDrawColor(r, 200, 180, 120, (Uint8)(a * 0.4f));
        SDL_RenderDrawRect(r, &ob);
        text(r, fontBig_, (winW - tw) / 2, topH + 18, st.omen, {255, 240, 200, a});
    }

    // ============ LEITOR DE CRÔNICA (F1) — histórico livre ============
    if (st.showChronicle && st.events && !st.events->empty()) {
        int hw = std::min(560, winW - 40);
        int hh = std::min(winH - 120, 480);
        int hx = (winW - hw) / 2;
        int hy = (winH - hh) / 2;
        panel(r, hx, hy, hw, hh, 235);
        text(r, fontBig_, hx + 14, hy + 8, "CRONICA DO MUNDO", {255, 220, 150, 255});
        text(r, fontSmall_, hx + hw - 80, hy + 12, "F1 fecha", {140, 150, 160, 255}, false);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 80, 150, 180, 70);
        SDL_RenderDrawLine(r, hx + 14, hy + 32, hx + hw - 14, hy + 32);
        // rolagem
        if (st.uiWheel != 0.f) {
            float mxw = st.mouseWinX, myw = st.mouseWinY;
            if (mxw >= hx && mxw <= hx + hw && myw >= hy && myw <= hy + hh) {
                scrollChronicle_ -= st.uiWheel * 22.f;
            }
        }
        SDL_Rect clip = {hx + 4, hy + 36, hw - 8, hh - 44};
        SDL_RenderSetClipRect(r, &clip);
        int n = (int)st.events->size();
        int rowH = 14;
        int contentH = n * rowH;
        int viewH = hh - 44;
        scrollChronicle_ = clampf(scrollChronicle_, 0.f, (float)std::max(0, contentH - viewH));
        int startI = (int)(scrollChronicle_ / rowH);
        int offY = (int)scrollChronicle_ % rowH;
        int ty = hy + 40 - offY;
        auto evCol = [](EvKind k) -> SDL_Color {
            switch (k) {
                case EvKind::Birth:     return {160, 220, 255, 255};
                case EvKind::Death:     return {180, 160, 200, 255};
                case EvKind::Discovery: return {255, 220, 120, 255};
                case EvKind::Build:     return {140, 220, 150, 255};
                case EvKind::War:       return {255, 130, 110, 255};
                case EvKind::Love:      return {255, 160, 190, 255};
                case EvKind::Idea:      return {140, 220, 240, 255};
                case EvKind::Health:    return {230, 200, 140, 255};
                default:                return {200, 210, 215, 255};
            }
        };
        for (int i = startI; i < n && ty < hy + hh - 8; i++) {
            const SimEvent& e = (*st.events)[i];
            char line[256];
            snprintf(line, sizeof(line), "%s", e.text.c_str());
            std::string msg = line;
            if (msg.size() > 62) msg = msg.substr(0, 60) + "..";
            text(r, fontSmall_, hx + 14, ty, msg, evCol(e.kind), false);
            ty += rowH;
        }
        // scrollbar
        if (contentH > viewH) {
            int sbX = hx + hw - 8;
            SDL_SetRenderDrawColor(r, 40, 50, 65, 180);
            SDL_Rect track = {sbX, hy + 38, 3, hh - 48};
            SDL_RenderFillRect(r, &track);
            float frac = (float)viewH / contentH;
            int thumbH = std::max(24, (int)(track.h * frac));
            float sp = (contentH - viewH) > 0 ? scrollChronicle_ / (contentH - viewH) : 0.f;
            int thumbY = track.y + (int)((track.h - thumbH) * sp);
            SDL_SetRenderDrawColor(r, 200, 180, 120, 220);
            SDL_Rect thumb = {sbX, thumbY, 3, thumbH};
            SDL_RenderFillRect(r, &thumb);
        }
        SDL_RenderSetClipRect(r, nullptr);
    }

    // ============ AJUDA ============
    if (st.showHelp) {
        int hw = 580, hh = 360;
        int hx = (winW - hw) / 2, hy = (winH - hh) / 2;
        panel(r, hx, hy, hw, hh, 240);
        text(r, fontBig_, hx + 16, hy + 10, "SKC E-195 — controles", {120, 220, 240, 255});
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 80, 160, 190, 80);
        SDL_RenderDrawLine(r, hx + 16, hy + 34, hx + hw - 16, hy + 34);
        int ty = hy + 44;
        struct HelpRow { const char* k; const char* d; };
        HelpRow rows[] = {
            {"WASD / setas", "mover a camera"},
            {"scroll", "zoom in/out"},
            {"botao medio", "arrastar mapa"},
            {"ESPACO", "pausar / continuar"},
            {"Z / X", "velocidade - / +"},
            {"clique", "inspecionar ser vivo"},
            {"TAB", "proximo ser"},
            {"1 2 3 4", "influencia divina no mouse"},
            {"E", "modo edicao de mundo"},
            {"  1-8 / roda", "terreno / raio da escova"},
            {"shift + scroll", "raio da escova"},
            {"R", "mundo novo"},
            {"F5 / F9", "salvar / carregar"},
            {"L", "Escada da Vida"},
            {"F1", "ler a Cronica do Mundo"},
            {"H", "fechar ajuda"},
            {"ESC", "sair"},
        };
        for (const auto& hr : rows) {
            text(r, font_, hx + 20, ty, hr.k, {255, 220, 140, 255}, false);
            text(r, font_, hx + 170, ty, hr.d, {210, 215, 220, 255}, false);
            ty += 18;
        }
        ty += 6;
        const char* flavor[] = {
            "Estacoes mudam o mundo. Fogo se espalha.",
            "Doenca em comunidade grande. Nado evolui.",
            "Baleias nascem de herbivoros que voltam ao mar.",
            "",
            "Voce observa. O mundo se desenvolve sozinho.",
        };
        for (const char* l : flavor) {
            text(r, font_, hx + 20, ty, l, {160, 180, 200, 255}, false);
            ty += 18;
        }
    }
}

}
