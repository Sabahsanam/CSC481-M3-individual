#include "GameLogic.h"
#include <cstdio>
#include <random>

// Is there solid ground whose top is near `y` under horizontal point `x`?
static bool solidUnder(GameWorld& w, float x, float y) {
    for (auto& o : w.objects) {
        auto* c = o.getComponent<CollidableComponent>();
        auto* t = o.getComponent<TransformComponent>();
        if (!c || !c->solid || !t) continue;
        if (x >= t->x && x <= t->x + t->width && t->y >= y - 12 && t->y <= y + 60) return true;
    }
    return false;
}
static float skullAhead(GameWorld& w, float pxRight, float pxLeft) {
    for (auto& o : w.objects) {
        if (o.getType() != "skull") continue;
        auto* t = o.getComponent<TransformComponent>();
        if (t->x > pxRight - 20 && t->x - pxRight < 380) return t->x - pxRight;
        (void)pxLeft;
    }
    return -1.0f;
}

int main() {
    std::mt19937 rng(12345);
    LevelTextures tex;
    int totalWins = 0, attempts = 0; double bestTime = 1e9; float furthest = 0;
    for (int attempt = 0; attempt < 400 && totalWins < 5; attempt++) {
        GameWorld w; game::buildLevel(w, tex);
        std::uniform_real_distribution<float> U(0.f, 1.f);
        float probe = 10 + U(rng) * 120;          // how close to an edge to jump
        bool run = U(rng) < 0.5f;
        float skullLead = 120 + U(rng) * 200;     // how far ahead of skull to jump
        float jumpEvery = 0.2f + U(rng) * 1.2f;   // extra random jumping (for moving platforms)
        float sinceJump = 0;
        auto* pt = w.player.getComponent<TransformComponent>();
        auto* pg = w.player.getComponent<GravityComponent>();
        for (int i = 0; i < 60 * 150 && w.wins == 0; i++) {
            GameInput in; in.right = true; in.run = run;
            float pxR = pt->x + pt->width, bottom = pt->y + pt->height;
            sinceJump += 1.0f / 60.0f;
            if (pg->grounded) {
                bool edge = !solidUnder(w, pxR - 30 + probe, bottom);
                float sk = skullAhead(w, pxR, pt->x);
                bool skull = sk >= 0 && sk < skullLead;
                bool blocked = false;
                for (auto& ob : w.objects) if (ob.hasComponent<BlockerComponent>()) { auto* bt = ob.getComponent<TransformComponent>(); float gap = bt->x - pxR; if (gap > -5 && gap < 90 && bottom > bt->y + 4) blocked = true; }
                bool rnd = sinceJump > jumpEvery && pt->x > 1300;
                if (edge || skull || rnd || blocked) { in.jump = true; sinceJump = 0; }
            }
            game::step(w, in, 1.0f / 60.0f);
            if (pt->x > furthest) furthest = pt->x;
        }
        attempts++;
        if (w.wins > 0) { totalWins++; if (w.time < bestTime) bestTime = w.time; }
    }
    std::printf("attempts=%d wins=%d furthestX=%.0f fastestWin=%.1fs\n", attempts, totalWins, furthest, totalWins ? bestTime : -1.0);
    return totalWins ? 0 : 1;
}
