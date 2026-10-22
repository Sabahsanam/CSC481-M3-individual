#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <algorithm>
#include <vector>

#include "GameObject.h"
#include "Components.h"
#include "GameComponents.h"
#include "GameObjectFactory.h"
#include "LevelObjects.h"
#include "Physics.h"
#include "Collision.h"

// The game's rules, written as small "systems" that loop over GameObjects
// and act on whichever ones have the components they care about. This file
// has no window or renderer code, so the same logic can run headless.

struct GameInput {
    bool left = false;
    bool right = false;
    bool jump = false;
    bool run = false;
};

struct GameWorld {
    GameObject player;                 // the client-controlled character
    std::vector<GameObject> objects;   // everything else in the level
    Physics physics;

    float cameraX = 0.0f;              // left edge of the visible area
    double time = 0.0;                 // game time, drives moving objects
    int standingOn = -1;               // index in objects, or -1 if airborne

    int deaths = 0;
    int wins = 0;
    int coins = 0;                     // stars collected
    int totalCoins = 0;                // stars in the level
    bool facingLeft = false;           // which way the princess looks
    float grace = 0.0f;                // seconds after a respawn when
                                       // visible enemies can't hurt

    // The hitbox is the same size the princess had in Milestone 1.
    GameWorld() : player(makePlayer(1, 0.0f, 0.0f, 80.0f, 130.0f))
    {
        physics.setGravity(900.0f);
    }
};

namespace game {

constexpr float MAX_CAMERA_X = level::WIDTH - level::SCREEN_W;

inline float clampCamera(float x)
{
    return std::max(0.0f, std::min(x, MAX_CAMERA_X));
}

// Keeps the two hidden scroll boundaries glued to the screen edges.
inline void updateScrollBoundaries(GameWorld& w)
{
    for (auto& obj : w.objects) {
        auto* b = obj.getComponent<ScrollBoundaryComponent>();
        auto* t = obj.getComponent<TransformComponent>();
        if (!b || !t) {
            continue;
        }

        if (b->side == ScrollSide::Left) {
            t->x = w.cameraX + b->inset;
        } else {
            t->x = w.cameraX + level::SCREEN_W - b->inset;
        }
    }
}

// Puts the player on the spawn point with the given index.
inline void respawn(GameWorld& w, int spawnIndex)
{
    auto* pt = w.player.getComponent<TransformComponent>();
    auto* pv = w.player.getComponent<VelocityComponent>();
    auto* pg = w.player.getComponent<GravityComponent>();

    const TransformComponent* chosen = nullptr;

    for (auto& obj : w.objects) {
        auto* sp = obj.getComponent<SpawnPointComponent>();
        auto* t = obj.getComponent<TransformComponent>();
        if (sp && t && sp->index == spawnIndex) {
            chosen = t;
            break;
        }
    }

    // Fall back to the first spawn point if the index doesn't exist
    if (!chosen) {
        for (auto& obj : w.objects) {
            if (obj.hasComponent<SpawnPointComponent>()) {
                chosen = obj.getComponent<TransformComponent>();
                break;
            }
        }
    }

    if (chosen) {
        pt->x = chosen->x - pt->width / 2.0f;
        pt->y = chosen->y - pt->height;
    }

    pv->vx = 0.0f;
    pv->vy = 0.0f;
    pg->grounded = false;
    w.standingOn = -1;
    w.grace = 1.0f;   // a moment to get moving before the skull counts

    w.cameraX = clampCamera(pt->x - 600.0f);
    updateScrollBoundaries(w);
}

inline void updateMovers(GameWorld& w)
{
    for (auto& obj : w.objects) {
        auto* m = obj.getComponent<MovementComponent>();
        auto* t = obj.getComponent<TransformComponent>();
        if (!m || !t) {
            continue;
        }

        float cx = 0.0f;
        float cy = 0.0f;
        evaluateMovement(*m, w.time, cx, cy);

        float newX = cx - t->width / 2.0f;
        float newY = cy - t->height / 2.0f;

        m->deltaX = newX - t->x;
        m->deltaY = newY - t->y;

        t->x = newX;
        t->y = newY;
    }
}

inline void updateAnimations(GameWorld& w, const GameInput& in, float dt)
{
    auto advance = [dt](RenderableComponent& r) {
        if (r.frameCount <= 1 || !r.useTimedAnimation) {
            return;
        }
        r.animationTimer += dt;
        while (r.animationTimer >= r.frameDuration) {
            r.currentFrame = (r.currentFrame + 1) % r.frameCount;
            r.animationTimer -= r.frameDuration;
        }
    };

    // The princess only walks when a direction is held
    if (auto* r = w.player.getComponent<RenderableComponent>()) {
        if (in.left || in.right) {
            advance(*r);
        } else {
            r->currentFrame = 0;
            r->animationTimer = 0.0f;
        }
    }
    for (auto& obj : w.objects) {
        if (auto* r = obj.getComponent<RenderableComponent>()) {
            advance(*r);
        }
    }
}

// Advance the whole game by dt seconds.
inline void step(GameWorld& w, const GameInput& in, float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    if (dt > 0.05f) {
        dt = 0.05f;   // avoid huge jumps after a stall
    }

    w.time += dt;

    auto* pt = w.player.getComponent<TransformComponent>();
    auto* pv = w.player.getComponent<VelocityComponent>();
    auto* pg = w.player.getComponent<GravityComponent>();

    float prevBottom = pt->y + pt->height;
    float prevX = pt->x;

    if (w.grace > 0.0f) {
        w.grace -= dt;
    }

    // 1. Input
    float speed = in.run ? 550.0f : 300.0f;
    if (in.left) {
        pt->x -= speed * dt;
        w.facingLeft = true;
    }
    if (in.right) {
        pt->x += speed * dt;
        w.facingLeft = false;
    }

    // 1b. Blockers (crates, totem): can't walk through them. If the
    // player was above the top last frame they are landing on it, not
    // walking into it, so it is left to the landing step below.
    for (auto& obj : w.objects) {
        if (!obj.hasComponent<BlockerComponent>()) {
            continue;
        }
        auto* t = obj.getComponent<TransformComponent>();
        if (!t) {
            continue;
        }
        bool overlapX = pt->x < t->x + t->width && pt->x + pt->width > t->x;
        bool overlapY = pt->y < t->y + t->height && pt->y + pt->height > t->y;
        bool wasAbove = prevBottom <= t->y + 4.0f;
        if (!overlapX || !overlapY || wasAbove) {
            continue;
        }
        if (prevX + pt->width <= t->x + 1.0f) {
            pt->x = t->x - pt->width;            // hit its left side
        } else if (prevX >= t->x + t->width - 1.0f) {
            pt->x = t->x + t->width;             // hit its right side
        }
    }
    if (in.jump) {
        w.physics.jump(w.player, 650.0f);
    }

    // 2. Gravity and velocity
    w.physics.update(w.player, dt);

    // 3. Moving platforms and the skull
    updateMovers(w);

    // 4. Carry the player if they were standing on a moving platform
    if (w.standingOn >= 0) {
        auto* m = w.objects[w.standingOn].getComponent<MovementComponent>();
        if (m) {
            pt->x += m->deltaX;
            pt->y += m->deltaY;
        }
    }

    // 5. Landing on solid objects (platforms and ground)
    pg->grounded = false;
    w.standingOn = -1;

    float left = pt->x;
    float right = pt->x + pt->width;
    float bottom = pt->y + pt->height;
    float bestTop = 0.0f;

    for (size_t i = 0; i < w.objects.size(); i++) {
        auto& obj = w.objects[i];
        auto* col = obj.getComponent<CollidableComponent>();
        auto* t = obj.getComponent<TransformComponent>();
        if (!col || !col->solid || !t) {
            continue;
        }

        if (right <= t->x || left >= t->x + t->width) {
            continue;
        }

        float platformDeltaY = 0.0f;
        if (auto* m = obj.getComponent<MovementComponent>()) {
            platformDeltaY = m->deltaY;
        }

        float top = t->y;
        float previousTop = top - platformDeltaY;

        bool falling = pv->vy >= 0.0f;
        bool wasAbove = prevBottom <= previousTop + 4.0f;
        bool reached = bottom >= top;

        if (falling && wasAbove && reached) {
            if (w.standingOn < 0 || top < bestTop) {
                w.standingOn = static_cast<int>(i);
                bestTop = top;
            }
        }
    }

    if (w.standingOn >= 0) {
        pt->y = bestTop - pt->height;
        pv->vy = 0.0f;
        pg->grounded = true;
    }

    // Keep the player inside the level
    pt->x = std::max(0.0f, std::min(pt->x, level::WIDTH - pt->width));

    // 6. Death zones (pits and the skull)
    for (auto& obj : w.objects) {
        auto* dz = obj.getComponent<DeathZoneComponent>();
        if (!dz) {
            continue;
        }
        // Visible enemies (the skull) are harmless just after a respawn
        if (w.grace > 0.0f && obj.hasComponent<RenderableComponent>()) {
            continue;
        }
        if (Collision::checkCollision(w.player, obj)) {
            w.deaths++;
            respawn(w, dz->targetSpawn);
            return;
        }
    }

    // 7. Side-scrolling boundaries
    updateScrollBoundaries(w);

    for (auto& obj : w.objects) {
        auto* b = obj.getComponent<ScrollBoundaryComponent>();
        auto* t = obj.getComponent<TransformComponent>();
        if (!b || !t) {
            continue;
        }
        if (!Collision::checkCollision(w.player, obj)) {
            continue;
        }

        if (b->side == ScrollSide::Right) {
            w.cameraX += (pt->x + pt->width) - t->x;
        } else {
            w.cameraX -= (t->x + t->width) - pt->x;
        }
        w.cameraX = clampCamera(w.cameraX);
        updateScrollBoundaries(w);
    }

    // 8. Stars
    for (auto& obj : w.objects) {
        auto* c = obj.getComponent<CollectibleComponent>();
        if (!c || c->collected) {
            continue;
        }
        if (Collision::checkCollision(w.player, obj)) {
            c->collected = true;
            w.coins += c->value;
            // Remove the look: no Renderable means it is hidden now
            obj.removeComponent<RenderableComponent>();
        }
    }

    // 9. Goal
    for (auto& obj : w.objects) {
        if (!obj.hasComponent<GoalComponent>()) {
            continue;
        }
        if (Collision::checkCollision(w.player, obj)) {
            w.wins++;
            respawn(w, 0);
            return;
        }
    }

    // 10. Animation
    updateAnimations(w, in, dt);
}

// Builds the level. Scenery first (so it is drawn behind), then platforms
// and pickups, then the hidden objects (spawn points, death zones, scroll
// boundaries).
inline void buildLevel(GameWorld& w, const LevelTextures& tex)
{
    const float G = level::GROUND_Y;
    const float PH = art::PLATFORM_H;
    auto& o = w.objects;

    // --- Scenery (drawn behind everything else) ---
    for (int i = 0; i < 10; i++) {
        float x = 150.0f + i * 330.0f;
        float y = 80.0f + (i % 3) * 110.0f;
        o.push_back(makeCloud(x, y, i * 1.3f, tex.cloud));
    }
    for (float x : { 380.0f, 1180.0f, 2650.0f, 3050.0f, 4700.0f, 5200.0f }) {
        o.push_back(makeBush(x, G, tex.bush));
    }

    // --- Static platforms (each has a different position/size/look) ---
    o.push_back(makeTexturedPlatform(0.0f,    G, 1536.0f, tex.platform));   // ground A
    o.push_back(makeTexturedPlatform(2304.0f, G, 1152.0f, tex.platform));   // ground B
    o.push_back(makeTexturedPlatform(4416.0f, G, 1344.0f, tex.platform));   // ground C
    o.push_back(makeColorPlatform(2500.0f, 760.0f, 256.0f, PH,
                                  ColorComponent(130, 80, 170)));           // purple
    o.push_back(makeColorPlatform(2900.0f, 600.0f, 256.0f, PH,
                                  ColorComponent(60, 160, 160)));           // teal
    o.push_back(makeTexturedPlatform(3240.0f, 720.0f, 192.0f, tex.platform)); // small

    // --- Moving platforms (three different movement patterns) ---
    o.push_back(makeMovingPlatform(MovementType::Horizontal, 1920.0f, G + 16.0f,
                                   200.0f, 1.0f, 0.0f, 192.0f, tex.platform));
    o.push_back(makeMovingPlatform(MovementType::Circular, 3770.0f, 760.0f,
                                   180.0f, 0.9f, 3.14159f, 192.0f, tex.platform));
    o.push_back(makeMovingPlatform(MovementType::Vertical, 4250.0f, 800.0f,
                                   150.0f, 1.1f, 0.0f, 192.0f, tex.platform));

    // --- Obstacles the princess has to jump over (they block sideways) ---
    o.push_back(makeTotem(700.0f, G, tex.totem));
    o.push_back(makeCrate(1100.0f, G, tex.crate));
    o.push_back(makeCrate(1300.0f, G, tex.crate));
    o.push_back(makeCrate(3150.0f, G, tex.crate));
    o.push_back(makeCrate(4780.0f, G, tex.crate));

    // --- Stars ---
    for (auto pos : { std::pair<float, float>{ 560.0f,  G - 120.0f },
                      { 1200.0f, G - 230.0f },
                      { 1920.0f, G - 170.0f },
                      { 2580.0f, 640.0f },
                      { 2990.0f, 480.0f },
                      { 3290.0f, 600.0f },
                      { 3770.0f, 560.0f },
                      { 4250.0f, 520.0f },
                      { 4700.0f, G - 150.0f },
                      { 5350.0f, G - 150.0f } }) {
        o.push_back(makeCoin(pos.first, pos.second, tex.coin));
        w.totalCoins++;
    }

    // --- Enemy and goal ---
    o.push_back(makeSkull(5100.0f, G, 220.0f, 1.0f, 2, tex.skull));
    o.push_back(makeGoal(5560.0f, G, tex.portal));

    // --- Hidden objects ---
    o.push_back(makeSpawnPoint(0, 200.0f,  G));
    o.push_back(makeSpawnPoint(1, 2450.0f, G));
    o.push_back(makeSpawnPoint(2, 4540.0f, G));

    o.push_back(makeDeathZone(1536.0f, 1000.0f, 768.0f, 600.0f, 0));   // gap 1
    o.push_back(makeDeathZone(3456.0f, 1000.0f, 960.0f, 600.0f, 1));   // gap 2
    o.push_back(makeDeathZone(-500.0f, 1500.0f, 7000.0f, 500.0f, 0));  // safety net

    o.push_back(makeScrollBoundary(ScrollSide::Left, 500.0f));
    o.push_back(makeScrollBoundary(ScrollSide::Right, 800.0f));

    // --- The princess: 8 walking frames, 96x156 each (Milestone 1 art) ---
    if (auto* r = w.player.getComponent<RenderableComponent>()) {
        r->texture = tex.player;
        r->frameCount = 8;
        r->frameWidth = 96;
        r->frameHeight = 156;
        r->frameDuration = 1.0f / 8.0f;
        r->useTimedAnimation = true;
    }

    respawn(w, 0);
}

} // namespace game

#endif
