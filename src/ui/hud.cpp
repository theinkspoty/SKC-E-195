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
    font_ = loadAny(fontCandidates, 14);
    fontSmall_ = loadAny(fontCandidates, 12);
    fontBig_ = loadAny(fontBoldCandidates, 18);
    if (!fontBig_) fontBig_ = font_;
    if (!fontSmall_) fontSmall_ = font_;
    return font_ != nullptr;
}

void Hud::close() {
    if (font_) TTF_CloseFont(font_);
    if (fontBig_ && fontBig_ != font_) TTF_CloseFont(fontBig_);
    if (fontSmall_ && fontSmall_ != font_) TTF_CloseFont(fontSmall_);
    font_ = fontBig_ = fontSmall_ = nullptr;
}

void Hud::text(SDL_Renderer* r, TTF_Font* f, int x, int y, const std::string& s, SDL_Color c, bool shadow) {
    if (!f || s.empty()) return;
    if (shadow) {
        SDL_Color sc = {0, 0, 0, 200};
        SDL_Surface* sh = TTF_RenderUTF8_Blended(f, s.c_str(), sc);
        if (sh) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, sh);
            SDL_Rect d = {x + 1, y + 1, sh->w, sh->h};
            SDL_RenderCopy(r, t, nullptr, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(sh);
        }
    }
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void Hud::panel(SDL_Renderer* r, int x, int y, int w, int h, Uint8 a) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 10, 14, 22, a);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 70, 160, 190, 120);
    SDL_RenderDrawRect(r, &rc);
}

void Hud::bar(SDL_Renderer* r, int x, int y, int w, int h, float v, SDL_Color c) {
    v = clampf(v, 0.f, 1.f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 30, 36, 48, 200);
    SDL_Rect bg = {x, y, w, h};
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 230);
    SDL_Rect fg = {x, y, (int)(w * v), h};
    SDL_RenderFillRect(r, &fg);
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
    float m = t.moisture;
    c.r = (Uint8)clampf(c.r * (1.f - m * 0.10f) + var * 22.f, 0.f, 255.f);
    c.g = (Uint8)clampf(c.g * (1.f - m * 0.04f) + var * 24.f, 0.f, 255.f);
    c.b = (Uint8)clampf(c.b * (1.f + m * 0.06f) + var * 20.f, 0.f, 255.f);
    return c;
}

void Hud::draw(SDL_Renderer* r, const HudState& st) {
    const int winW = st.winW > 0 ? st.winW : 1280;
    const int winH = st.winH > 0 ? st.winH : 720;
    char buf[256];

    // ================= BARRA SUPERIOR (largura total) =================
    const int topH = 52;
    panel(r, 0, 0, winW, topH, 200);
    text(r, fontBig_, 14, 6, "SKC E-195", {120, 220, 240, 255});
    if (st.eco) {
        text(r, font_, 150, 8, eraName(st.eco->era()), {255, 210, 130, 255});
    }
    snprintf(buf, sizeof(buf), "Dia %u  %s  %d%%   Vivos %d   FPS %d   %s x%.0f",
             st.day, st.phaseName, (int)(st.dayFrac * 100.f),
             st.creatureCount, st.fps, st.paused ? "PAUSADO" : "ok", st.speed);
    text(r, font_, 14, 28, buf, {200, 210, 220, 255});

    // dia + umidade no canto superior direito
    int right = winW - 14;
    bar(r, winW - 220, 12, 120, 8, st.dayFrac,
        st.light > 0.4f ? SDL_Color{240, 200, 80, 255} : SDL_Color{80, 100, 200, 255});
    text(r, fontSmall_, winW - 220, 26, "dia", {160, 170, 180, 255});
    bar(r, winW - 90, 12, 70, 8, st.humidity, {60, 140, 220, 255});
    text(r, fontSmall_, winW - 90, 26, "agua", {160, 170, 180, 255});
    (void)right;

    int y = topH + 8;
    const int leftX = 10;
    const int leftW = 300;

    // banner de edição
    if (st.editMode) {
        panel(r, 0, y, winW, 26, 200);
        snprintf(buf, sizeof(buf), "MUNDO  escova: %s  raio %d  —  botao esq pinta, roda = raio", tileName(st.brush), st.brushRadius);
        text(r, font_, 10, y + 4, buf, {240, 200, 120, 255});
        y += 34;
    }

    // ============ PAINEL ESQUERDO: seleção (não sobrepõe) ============
    if (st.selected) {
        const Creature& c = *st.selected;
        const int ph = 760;
        panel(r, leftX, y, leftW, ph, 200);
        int ty = y + 8;
        int px = leftX + 12;
        char line[256];
        snprintf(line, sizeof(line), "%s  (%s)", c.fullName().c_str(), c.male ? "macho ♂" : "femea ♀");
        text(r, fontBig_, px, ty, line, c.male ? SDL_Color{180, 210, 255, 255} : SDL_Color{255, 180, 210, 255});
        ty += 22;
        snprintf(line, sizeof(line), "%s   %.1f dias   filhos %d   intel %.2f", stateName(c.state), c.age, c.children, c.g.intelligence);
        text(r, font_, px, ty, line, {200, 210, 220, 255});
        ty += 20;

        text(r, fontSmall_, px, ty, "Energia", {180, 180, 180, 255});
        bar(r, px + 70, ty + 1, leftW - 94, 8, c.energy / 100.f, {80, 200, 100, 255});
        ty += 14;
        text(r, fontSmall_, px, ty, "Agua", {180, 180, 180, 255});
        bar(r, px + 70, ty + 1, leftW - 94, 8, c.hydration / 100.f, {70, 150, 230, 255});
        ty += 18;

        text(r, fontSmall_, px, ty, "GENOMA", {120, 220, 240, 255});
        ty += 15;
        snprintf(line, sizeof(line), "vel %.2f  metab %.2f  vis %.1f  porte %.2f  aggro %.2f",
                 c.g.speed, c.g.metabolism, c.g.vision, c.g.size, c.g.aggression);
        text(r, fontSmall_, px, ty, line, {190, 200, 210, 255});
        ty += 13;
        snprintf(line, sizeof(line), "fert %.2f  vigor %.2f  vida %.0f  soc %.2f",
                 c.g.fertility, c.g.robustness, c.g.lifespan, c.g.sociability);
        text(r, fontSmall_, px, ty, line, {190, 200, 210, 255});
        ty += 17;

        text(r, fontSmall_, px, ty, "APRENDIZADO", {120, 220, 240, 255});
        ty += 15;
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

        text(r, fontSmall_, px, ty, "SENTIMENTOS", {255, 160, 180, 255});
        ty += 15;
        auto emo = [&](const char* n, float v, SDL_Color col) {
            text(r, fontSmall_, px, ty, n, {170, 170, 180, 255});
            bar(r, px + 78, ty + 1, leftW - 102, 7, v, col);
            ty += 12;
        };
        emo("medo", c.em.fear, {140, 160, 230, 255});
        emo("raiva", c.em.anger, {230, 90, 70, 255});
        emo("amor", c.em.love, {240, 120, 160, 255});
        emo("luto", c.em.grief, {160, 140, 200, 255});
        emo("prazer", c.em.satisfaction, {240, 200, 100, 255});
        emo("culpa", c.em.guilt, {180, 120, 160, 255});
        emo("vergonha", c.em.shame, {160, 100, 140, 255});
        emo("orgulho", c.em.pride, {140, 200, 220, 255});
        emo("conforto", c.em.comfort, {120, 210, 140, 255});
        emo("solidao", c.em.loneliness, {150, 150, 170, 255});

        // moral e reputação
        ty += 4;
        text(r, fontSmall_, px, ty, "MORAL", {255, 180, 180, 255});
        bar(r, px + 50, ty + 1, leftW - 74, 7, c.morality,
            c.morality > 0.6f ? SDL_Color{100, 220, 120, 255} :
            (c.morality < 0.35f ? SDL_Color{220, 80, 70, 255} : SDL_Color{200, 180, 100, 255}));
        ty += 12;
        snprintf(line, sizeof(line), "reputacao %s%.2f",
                 c.reputation >= 0 ? "+" : "", c.reputation);
        text(r, fontSmall_, px, ty, line,
             c.reputation < -0.3f ? SDL_Color{220, 100, 80, 255} :
             (c.reputation > 0.3f ? SDL_Color{120, 220, 140, 255} : SDL_Color{170, 170, 180, 255}));
        ty += 14;

        if (!c.bonds.empty()) {
            const Bond& b = c.bonds.back();
            snprintf(line, sizeof(line), "Laco #%d  conf %.2f  aff %.2f  rancor %.2f",
                     b.id, b.trust, b.affection, b.grudge);
            text(r, fontSmall_, px, ty, line, {200, 190, 160, 255});
            ty += 14;
        }
        // personalidade: gráfico de barras (cada um é um)
        ty += 2;
        text(r, fontSmall_, px, ty, "PERSONALIDADE", {255, 160, 220, 255});
        ty += 14;
        auto persBar = [&](const char* n, float v, SDL_Color col) {
            text(r, fontSmall_, px, ty, n, {170, 170, 180, 255});
            bar(r, px + 90, ty + 1, leftW - 114, 6, v, col);
            ty += 11;
        };
        persBar("desejo sexual", c.pers.sexuality, {240, 120, 180, 255});
        persBar("ousadia", c.pers.boldness, {240, 160, 80, 255});
        persBar("pensa vs age", c.pers.thinker, {120, 200, 240, 255});
        persBar("ganancia", c.pers.greed, {200, 180, 80, 255});
        persBar("preguica", c.pers.laziness, {150, 150, 160, 255});
        persBar("empatia", c.pers.empathy, {140, 220, 160, 255});
        persBar("vagabundo", c.pers.wanderlust, {180, 160, 220, 255});

        // O QUE ELE ACREDITA
        ty += 4;
        text(r, fontSmall_, px, ty, "IDEIAS", {255, 220, 140, 255});
        ty += 14;
        if (c.ideas.empty()) {
            text(r, fontSmall_, px, ty, "ainda nao tem certezas", {130, 140, 150, 255}, false);
            ty += 12;
        } else {
            int shown = (int)std::min<size_t>(c.ideas.size(), 6);
            // mostra as mais fortes
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
                if (!id->fromName.empty()) {
                    snprintf(line, sizeof(line), "de %s", id->fromName.c_str());
                    text(r, fontSmall_, px + leftW - 100, ty - 2, line, {140, 150, 160, 255}, false);
                }
                ty += 10;
            }
            if ((int)c.ideas.size() > shown) {
                snprintf(line, sizeof(line), "... +%d outras", (int)c.ideas.size() - shown);
                text(r, fontSmall_, px, ty, line, {120, 130, 140, 255}, false);
                ty += 11;
            }
        }

        // preferências de par (o que ela/e acha atraente)
        snprintf(line, sizeof(line), "gosta de: forca %.2f  mentes %.2f  habs %.2f",
                 c.prefs.likeStrength, c.prefs.likeSmarts, c.prefs.likeSkill);
        text(r, fontSmall_, px, ty, line, {200, 170, 200, 255});
        ty += 13;
        snprintf(line, sizeof(line), "         porte %.2f  calma %.2f  juventude %.2f",
                 c.prefs.likeSize, c.prefs.likeCalm, c.prefs.likeYoung);
        text(r, fontSmall_, px, ty, line, {200, 170, 200, 255});
        ty += 15;
        // ensinamentos recebidos
        if (!c.teachings.empty()) {
            text(r, fontSmall_, px, ty, "ENSINAMENTOS", {180, 220, 180, 255});
            ty += 13;
            int shown = (int)std::min<size_t>(c.teachings.size(), 4);
            for (int i = (int)c.teachings.size() - shown; i < (int)c.teachings.size(); i++) {
                const TeachingRecord& t = c.teachings[i];
                snprintf(line, sizeof(line), "de %s: %s (%.2f)", t.teacherName.c_str(), t.lesson.c_str(), t.value);
                text(r, fontSmall_, px, ty, line, {160, 200, 160, 255}, false);
                ty += 12;
            }
        }
        // simbiose
        if (!c.speciesBonds.empty()) {
            const SpeciesBond& sb = c.speciesBonds.back();
            text(r, fontSmall_, px, ty, "SIMBIOSE", {180, 200, 255, 255});
            ty += 13;
            snprintf(line, sizeof(line), "com %s: conf %.2f  afeto %.2f",
                     faunaName((FaunaType)sb.faunaType), sb.trust, sb.affection);
            text(r, fontSmall_, px, ty, line, {160, 180, 220, 255});
            ty += 14;
        }
        // FAMÍLIA
        ty += 4;
        text(r, fontSmall_, px, ty, "FAMILIA", {255, 200, 160, 255});
        ty += 13;
        if (st.sims) {
            // pais
            const Creature* pa = c.parentA >= 0 ? st.sims->get(c.parentA) : nullptr;
            const Creature* pb = c.parentB >= 0 ? st.sims->get(c.parentB) : nullptr;
            snprintf(line, sizeof(line), "pais: %s | %s",
                     pa ? pa->name.c_str() : "?",
                     pb ? pb->name.c_str() : "?");
            text(r, fontSmall_, px, ty, line, {190, 180, 160, 255}, false);
            ty += 12;
            // par
            if (c.em.mateId >= 0) {
                const Creature* mate = st.sims->get(c.em.mateId);
                if (mate && mate->alive()) {
                    snprintf(line, sizeof(line), "par: %s (amor %.2f)", mate->name.c_str(), c.em.love);
                    text(r, fontSmall_, px, ty, line, {240, 140, 170, 255}, false);
                    ty += 12;
                }
            }
            // filhos
            if (!c.childrenIds.empty()) {
                int show = (int)std::min<size_t>(c.childrenIds.size(), 3);
                std::string kids = "filhos:";
                for (int i = 0; i < show; i++) {
                    const Creature* k = st.sims->get(c.childrenIds[c.childrenIds.size() - show + i]);
                    if (k) kids += " " + k->name;
                }
                if ((int)c.childrenIds.size() > show) kids += " ...";
                text(r, fontSmall_, px, ty, kids, {160, 210, 160, 255}, false);
                ty += 12;
            }
            // sobrenome
            if (!c.surname.empty()) {
                snprintf(line, sizeof(line), "linhagem: %s", c.surname.c_str());
                text(r, fontSmall_, px, ty, line, {180, 170, 140, 255}, false);
                ty += 12;
            }
        }

        // comunidade
        if (c.communityId >= 0 && st.sims) {
            if (Community* comm = st.sims->community(c.communityId)) {
                text(r, fontSmall_, px, ty, "COMUNIDADE", {120, 230, 160, 255});
                ty += 13;
                snprintf(line, sizeof(line), "%s (%d membros)", comm->name.c_str(), comm->size());
                text(r, fontSmall_, px, ty, line, {160, 220, 170, 255});
                ty += 12;
                snprintf(line, sizeof(line), "%s%s%s  ideias do grupo: %d",
                         comm->hasFire ? "fogueira " : "",
                         comm->hasHut ? "toca " : "",
                         (!comm->hasFire && !comm->hasHut) ? "acampamento" : "",
                         (int)comm->groupIdeas.size());
                text(r, fontSmall_, px, ty, line, {140, 200, 150, 255});
                ty += 14;
            }
        }
        text(r, fontSmall_, px, ty, "OBJETIVO", {255, 210, 120, 255});
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
        y += ph + 8;
    } else if (st.selectedFauna && st.selectedFauna->alive()) {
        const Fauna& f = *st.selectedFauna;
        const int ph = 155;
        panel(r, leftX, y, leftW, ph, 200);
        int px = leftX + 12;
        char line[256];
        snprintf(line, sizeof(line), "%s #%d (%s)", faunaName(f.type), f.id, f.male ? "macho ♂" : "femea ♀");
        text(r, fontBig_, px, y + 8, line, f.male ? SDL_Color{180, 210, 255, 255} : SDL_Color{255, 180, 210, 255});
        snprintf(line, sizeof(line), "idade %.1f / vida %.0f   filhos %d", f.age, f.g.lifespan, f.children);
        text(r, fontSmall_, px, y + 32, line, {180, 190, 200, 255});
        text(r, fontSmall_, px, y + 50, "Energia", {180, 180, 180, 255});
        bar(r, px + 70, y + 51, leftW - 94, 8, f.energy / 100.f, {80, 200, 100, 255});
        text(r, fontSmall_, px, y + 64, "Saciedade", {200, 180, 140, 255});
        bar(r, px + 70, y + 65, leftW - 94, 8, f.satisfaction, {240, 200, 100, 255});
        snprintf(line, sizeof(line), "vel %.2f  vis %.1f  porte %.2f  aggro %.2f",
                 f.g.speed, f.g.vision, f.g.size, f.g.aggression);
        text(r, fontSmall_, px, y + 82, line, {170, 180, 195, 255});
        // medo aprendido
        if (f.fearOf[(int)FaunaType::SeaPredator] > 0.1f || f.fearOf[(int)FaunaType::LandPredator] > 0.1f ||
            f.fearOf[(int)FaunaType::SkyPredator] > 0.1f) {
            snprintf(line, sizeof(line), "medo: Pisc %.2f  Lobo %.2f  Falcon %.2f",
                     f.fearOf[(int)FaunaType::SeaPredator], f.fearOf[(int)FaunaType::LandPredator],
                     f.fearOf[(int)FaunaType::SkyPredator]);
            text(r, fontSmall_, px, y + 98, line, {160, 150, 180, 255});
        }
        text(r, fontSmall_, px, y + 114, "OBJETIVO", {255, 210, 120, 255});
        text(r, fontSmall_, px, y + 130, Ecosystem::goalOf(f), {255, 230, 160, 255});
        y += ph + 8;
    }

    // ============ ESCADA DA VIDA ============
    if (st.showLadder && st.eco) {
        LifeLadder L = st.eco->ladder();
        const int lh = 230;
        // se não couber embaixo do painel, fica no topo esquerdo
        int ly = y;
        if (ly + lh > winH - 90) ly = topH + 8;
        panel(r, leftX, ly, leftW, lh, 180);
        text(r, fontBig_, leftX + 8, ly + 4, eraName(st.eco->era()), {255, 210, 130, 255});
        // história em 1 linha (pode ser longa — corta)
        std::string story = eraStory(st.eco->era());
        if (story.size() > 42) story = story.substr(0, 40) + "..";
        text(r, fontSmall_, leftX + 8, ly + 24, story, {160, 180, 195, 255}, false);
        auto row = [&](int i, const char* n, bool has, const char* need) {
            int yy = ly + 40 + i * 14;
            SDL_Color c = has ? SDL_Color{120, 230, 140, 255} : SDL_Color{105, 115, 125, 255};
            snprintf(buf, sizeof(buf), "%s %s", has ? "*" : " ", n);
            text(r, fontSmall_, leftX + 8, yy, buf, c, false);
            if (!has) text(r, fontSmall_, leftX + 120, yy, need, {90, 100, 110, 255}, false);
        };
        row(0, "Microbioma", L.microbe, "agua+minerais");
        row(1, "Plancton", L.plankton, "luz+nutrientes");
        row(2, "Plantas", L.plants, "terra umida");
        row(3, "Medusa", L.medusa, "plancton (mar)");
        row(4, "Peixe", L.fish, "medusas");
        row(5, "Anfibio", L.amphibian, "peixe + margem");
        row(6, "Herbivoro", L.herbivores, "anfibio + plantas");
        row(7, "Piscador", L.seaPred, "peixes (mar)");
        row(8, "Lobo", L.landPred, "herbivoros (terra)");
        row(9, "Falcon", L.skyPred, "carroneiros (ar)");
        row(10, "Carroneiro", L.scavengers, "mortes");
        row(11, "Pensantes", L.sages, "pressao+tempo");
        text(r, fontSmall_, leftX + 8, ly + lh - 18, L.nextHint(), {230, 200, 140, 255}, false);
    }

    // ============ LOG DE EVENTOS (em cima da barra divina) ============
    if (st.events && !st.events->empty()) {
        int show = (int)std::min<size_t>(st.events->size(), 5);
        int logW = 420;
        int logH = show * 15 + 8;
        int lx = winW - logW - 12;
        int ly = winH - 78 - logH;
        if (ly < topH + 4) ly = topH + 4;
        panel(r, lx, ly, logW, logH, 130);
        for (int i = 0; i < show; i++) {
            const SimEvent& e = (*st.events)[st.events->size() - show + i];
            float fade = clampf(1.3f - e.t * 0.015f, 0.3f, 1.f);
            SDL_Color col = {170, 195, 175, (Uint8)(230 * fade)};
            std::string msg = e.text;
            if (msg.size() > 52) msg = msg.substr(0, 50) + "..";
            text(r, fontSmall_, lx + 6, ly + 4 + i * 15, msg, col, false);
        }
    }

    // ============ BARRA DIVINA (embaixo, centro) ============
    if (st.god) {
        int bw = 170, bh = 52, gap = 10;
        int total = 4 * bw + 3 * gap;
        int bx0 = (winW - total) / 2;
        int by = winH - bh - 10;
        for (int i = 0; i < 4; i++) {
            GodAction a = (GodAction)i;
            int x = bx0 + i * (bw + gap);
            panel(r, x, by, bw, bh, 210);
            snprintf(buf, sizeof(buf), "[%s] %s", GodSystem::actionKey(a), GodSystem::actionName(a));
            text(r, font_, x + 10, by + 5, buf, {230, 220, 180, 255});
            float cl = st.god->cooldownLeft(a);
            float cm = st.god->cooldownMax(a);
            if (cl > 0.f) {
                float frac = cl / cm;
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
                SDL_Rect cover = {x, by, (int)(bw * frac), bh};
                SDL_RenderFillRect(r, &cover);
                snprintf(buf, sizeof(buf), "recarga %.0fs", cl);
                text(r, fontSmall_, x + 10, by + 30, buf, {180, 140, 140, 255});
            } else {
                text(r, fontSmall_, x + 10, by + 30, "pronto (mouse)", {120, 200, 140, 255});
            }
        }
    }

    // tile sob o mouse (canto superior direito, abaixo da barra)
    if (st.mouseInWorld && st.world && !st.editMode) {
        int tx = (int)st.mouseX, ty = (int)st.mouseY;
        if (st.world->inBounds(tx, ty)) {
            const TileData& t = st.world->at(tx, ty);
            char line[256];
            snprintf(line, sizeof(line), "%s  nutr %.2f  biof %u  planct %u  carca %u",
                     tileName(t.type), t.nutrients, t.biofilm, t.plankton, t.corpse);
            int tw = 0, th = 0;
            TTF_SizeUTF8(fontSmall_, line, &tw, &th);
            int px = winW - tw - 20;
            if (px < leftX + leftW + 8) px = leftX + leftW + 8;
            panel(r, px - 4, topH + 4, tw + 12, th + 8, 175);
            text(r, fontSmall_, px, topH + 8, line, {200, 215, 225, 255}, false);
        }
    }

    // omen central
    if (st.omenT > 0.f && !st.omen.empty()) {
        Uint8 a = (Uint8)clampf(st.omenT * 70.f, 0.f, 235.f);
        SDL_Color oc = {255, 240, 200, a};
        int tw = 0, th = 0;
        TTF_SizeUTF8(fontBig_, st.omen.c_str(), &tw, &th);
        text(r, fontBig_, (winW - tw) / 2, topH + 16, st.omen, oc);
    }

    // ajuda
    if (st.showHelp) {
        int hw = 560, hh = 320;
        int hx = (winW - hw) / 2, hy = (winH - hh) / 2;
        panel(r, hx, hy, hw, hh, 235);
        int ty = hy + 12;
        text(r, fontBig_, hx + 16, ty, "SKC E-195 — controles", {120, 220, 240, 255});
        ty += 30;
        const char* lines[] = {
            "WASD / setas   mover a camera",
            "ESPACO         pausar / continuar",
            "Z / X          velocidade - / +",
            "clique         inspecionar ser vivo",
            "TAB            proximo ser",
            "1 2 3 4        influencia divina no mouse",
            "E              modo edicao de mundo",
            "  1-8 terreno, roda = raio",
            "R              mundo novo",
            "L              Escada da Vida",
            "H              fechar ajuda",
            "ESC            sair",
            "scroll         zoom in/out",
            "shift+scroll   raio da escova (no modo E)",
            "",
            "1 dia = 3 min. Ha noite. A vida emerge sozinha.",
            "Femeas escolhem o par. Predadores tem 3 nichos.",
            "Voce pede. O mundo decide.",
        };
        for (const char* l : lines) {
            text(r, font_, hx + 16, ty, l, {210, 215, 220, 255}, false);
            ty += 19;
        }
    }
}

}
