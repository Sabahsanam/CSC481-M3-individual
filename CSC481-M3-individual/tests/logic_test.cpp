#include "GameLogic.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); failures++; } else { std::printf("ok:   %s\n", msg); } } while (0)

static void run(GameWorld& w, GameInput in, float seconds) {
    int n = (int)(seconds * 60.0f);
    for (int i = 0; i < n; i++) game::step(w, in, 1.0f / 60.0f);
}
static TransformComponent* P(GameWorld& w) { return w.player.getComponent<TransformComponent>(); }
static GravityComponent* G(GameWorld& w) { return w.player.getComponent<GravityComponent>(); }

int main() {
    LevelTextures tex;

    { // spawn + landing
        GameWorld w; game::buildLevel(w, tex);
        run(w, {}, 1.0f);
        CHECK(G(w)->grounded, "player lands and is grounded at start");
        CHECK(std::abs(P(w)->y + P(w)->height - level::GROUND_Y) < 1.0f, "player stands on ground height");
        CHECK(std::abs(P(w)->x - 160.0f) < 1.0f, "player starts at spawn 0 (x center 200)");
        CHECK(w.deaths == 0, "no deaths at start");
    }
    { // scrolling right then left
        GameWorld w; game::buildLevel(w, tex);
        GameInput r; r.right = true; r.run = true; r.jump = true;
        P(w)->x = 800;
        run(w, r, 1.2f);
        CHECK(w.cameraX > 0.0f, "camera scrolls right when player hits right boundary");
        float cam = w.cameraX;
        GameInput l; l.left = true; l.run = true; l.jump = true;
        run(w, l, 1.4f);
        CHECK(w.cameraX < cam, "camera scrolls back left when player hits left boundary");
        CHECK(w.cameraX >= 0.0f, "camera never goes below 0");
    }
    { // pit death -> spawn 0
        GameWorld w; game::buildLevel(w, tex);
        GameInput r; r.right = true; r.run = true;
        P(w)->x = 1380;
        for (int i = 0; i < 60 * 12 && w.deaths == 0; i++) game::step(w, r, 1.0f / 60.0f);
        CHECK(w.deaths == 1, "falling into the gap triggers a death zone");
        CHECK(std::abs(P(w)->x - 160.0f) < 1.0f, "gap 1 death zone sends player to spawn 0");
        CHECK(w.cameraX == 0.0f, "camera is reset to show the spawn point");
    }
    { // moving platform carries player
        GameWorld w; game::buildLevel(w, tex);
        // find the horizontal moving platform
        int idx = -1;
        for (size_t i = 0; i < w.objects.size(); i++) {
            auto* m = w.objects[i].getComponent<MovementComponent>();
            if (m && m->type == MovementType::Horizontal && w.objects[i].getType() == "moving_platform") idx = (int)i;
        }
        CHECK(idx >= 0, "level has a horizontal moving platform");
        // put player above the platform and let them land
        auto* pl = w.objects[idx].getComponent<TransformComponent>();
        P(w)->x = pl->x + 56; P(w)->y = pl->y - 150;
        run(w, {}, 1.0f);
        CHECK(w.standingOn == idx, "player lands on the moving platform");
        float px0 = P(w)->x, plx0 = pl->x;
        run(w, {}, 0.5f);
        float dPlayer = P(w)->x - px0, dPlat = pl->x - plx0;
        CHECK(std::abs(dPlayer - dPlat) < 0.5f && std::abs(dPlat) > 5.0f, "player is carried along by the moving platform");
    }
    { // skull kills
        GameWorld w; game::buildLevel(w, tex);
        P(w)->x = 5100; P(w)->y = 700; w.grace = 0;
        w.cameraX = game::clampCamera(P(w)->x - 600);
        int d0 = w.deaths;
        run(w, {}, 3.0f);
        CHECK(w.deaths > d0, "touching the skull causes a respawn");
    }
    { // goal
        GameWorld w; game::buildLevel(w, tex);
        P(w)->x = 5500; P(w)->y = 700;
        run(w, {}, 1.0f);
        CHECK(w.wins == 1, "touching the goal counts a win");
        CHECK(P(w)->x < 600.0f, "after the goal the player is back at the start");
    }
    { // hidden objects are not drawn (no Renderable/Color)
        GameWorld w; game::buildLevel(w, tex);
        int hidden = 0, spawns = 0, zones = 0, bounds = 0;
        for (auto& o : w.objects) {
            if (!o.hasComponent<RenderableComponent>() && !o.hasComponent<ColorComponent>()) hidden++;
            if (o.hasComponent<SpawnPointComponent>()) spawns++;
            if (o.hasComponent<DeathZoneComponent>() && !o.hasComponent<RenderableComponent>()) zones++;
            if (o.hasComponent<ScrollBoundaryComponent>()) bounds++;
        }
        std::printf("      hidden=%d spawns=%d hiddenDeathZones=%d boundaries=%d\n", hidden, spawns, zones, bounds);
        CHECK(spawns >= 2, "multiple spawn points");
        CHECK(zones >= 1, "at least one hidden death zone");
        CHECK(bounds == 2, "left and right scroll boundaries");
    }
    { // no respawn loop: idle at every spawn point for 30 s
        for (int sp = 0; sp < 3; sp++) {
            GameWorld w; game::buildLevel(w, tex);
            game::respawn(w, sp);
            run(w, {}, 30.0f);
            char m[64]; std::snprintf(m, sizeof m, "idle 30s at spawn %d causes no deaths", sp);
            CHECK(w.deaths == 0, m);
        }
    }
    { // blockers stop the player
        GameWorld w; game::buildLevel(w, tex);
        GameInput r; r.right = true;
        run(w, r, 3.0f);   // totem at 700..764
        CHECK(P(w)->x + P(w)->width <= 700.5f && P(w)->x + P(w)->width > 690.0f, "totem blocks the player walking right");
        GameInput l; l.left = true;
        P(w)->x = 770; run(w, l, 1.0f);
        CHECK(P(w)->x >= 763.5f, "totem blocks the player walking left");
        // jump over it
        GameWorld w2; game::buildLevel(w2, tex);
        P(w2)->x = 560; run(w2, {}, 0.5f);
        float best = 0; bool crossed = false;
        for (int i = 0; i < 60*3 && !crossed; i++) { GameInput in; in.right = true; in.run = true; in.jump = true; game::step(w2, in, 1.0f/60); if (P(w2)->x > 780) crossed = true; }
        CHECK(crossed, "player can jump over the totem");
        (void)best;
        // stand on a crate
        GameWorld w3; game::buildLevel(w3, tex);
        P(w3)->x = 1100 - 8; P(w3)->y = 700; run(w3, {}, 1.0f);
        CHECK(std::abs(P(w3)->y + P(w3)->height - (level::GROUND_Y - 64.0f)) < 1.0f, "player can stand on top of a crate");
    }
    { // coins
        GameWorld w; game::buildLevel(w, tex);
        CHECK(w.totalCoins == 10, "level has 10 stars");
        int idx = -1;
        for (size_t i = 0; i < w.objects.size(); i++) if (w.objects[i].hasComponent<CollectibleComponent>()) { idx = (int)i; break; }
        auto* ct = w.objects[idx].getComponent<TransformComponent>();
        P(w)->x = ct->x - 10; P(w)->y = ct->y - 40; w.cameraX = 0;
        game::step(w, {}, 1.0f/60);
        CHECK(w.coins == 1, "touching a star collects it");
        CHECK(!w.objects[idx].hasComponent<RenderableComponent>(), "collected star loses its Renderable (hidden)");
        P(w)->x = ct->x - 10; P(w)->y = ct->y - 40;
        game::step(w, {}, 1.0f/60);
        CHECK(w.coins == 1, "a star is only counted once");
    }
    { // grace period
        GameWorld w; game::buildLevel(w, tex);
        CHECK(w.grace > 0.0f, "grace timer set on respawn");
    }
    std::printf("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
