#ifndef LEVEL_OBJECTS_H
#define LEVEL_OBJECTS_H

#include <cmath>
#include <SDL3/SDL.h>

#include "GameObject.h"
#include "Components.h"
#include "GameComponents.h"

// Factory functions for the individual game's objects. Each one is just
// a list of components, the same way makePlayer() and makePlatform()
// are in GameObjectFactory.h.

// Level dimensions (world units are pixels at 1920x1080).
namespace level {
    constexpr float SCREEN_W = 1920.0f;
    constexpr float SCREEN_H = 1080.0f;
    constexpr float WIDTH = 5760.0f;     // three screens wide
    constexpr float GROUND_Y = 930.0f;   // top surface of the ground
}

// Textures the game loads. Any of them may be null (the object is then
// drawn as a plain rectangle), which also lets the game logic run
// without a window.
struct LevelTextures {
    SDL_Texture* player = nullptr;
    SDL_Texture* platform = nullptr;
    SDL_Texture* crate = nullptr;
    SDL_Texture* coin = nullptr;
    SDL_Texture* bush = nullptr;
    SDL_Texture* cloud = nullptr;
    SDL_Texture* background = nullptr;
    SDL_Texture* skull = nullptr;
    SDL_Texture* portal = nullptr;
    SDL_Texture* totem = nullptr;
};

// Where a moving object is at a given time. The pattern's origin is the
// CENTER of the object.
inline void evaluateMovement(const MovementComponent& m, double time,
                             float& centerX, float& centerY)
{
    float angle = m.speed * static_cast<float>(time) + m.phase;

    switch (m.type) {
        case MovementType::Horizontal:
            centerX = m.originX + m.amplitude * std::sin(angle);
            centerY = m.originY;
            break;
        case MovementType::Vertical:
            centerX = m.originX;
            centerY = m.originY + m.amplitude * std::sin(angle);
            break;
        case MovementType::Circular:
            centerX = m.originX + m.amplitude * std::cos(angle);
            centerY = m.originY + m.amplitude * std::sin(angle);
            break;
    }
}

// Platform sizes come from the princess art (my_platform.png is 192x22).
namespace art {
    constexpr float PLATFORM_W = 192.0f;
    constexpr float PLATFORM_H = 22.0f;
}

// Static platform drawn with a texture repeated across its width. If the
// texture is missing (headless tests) it falls back to a plain color.
inline GameObject makeTexturedPlatform(float x, float y, float w,
                                       SDL_Texture* tex)
{
    GameObject obj("static_platform", -1);
    obj.addComponent<TransformComponent>(x, y, w, art::PLATFORM_H);
    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    obj.addComponent<TileComponent>(art::PLATFORM_W);
    obj.addComponent<CollidableComponent>();   // solid: can be stood on
    return obj;
}

// Static platform drawn as a solid color.
inline GameObject makeColorPlatform(float x, float y, float w, float h,
                                    ColorComponent color)
{
    GameObject obj("static_platform", -1);
    obj.addComponent<TransformComponent>(x, y, w, h);
    obj.addComponent<ColorComponent>(color);
    obj.addComponent<CollidableComponent>();
    return obj;
}

// Moving platform. centerX/centerY is the middle of its path.
inline GameObject makeMovingPlatform(MovementType type,
                                     float centerX, float centerY,
                                     float amplitude, float speed, float phase,
                                     float w, SDL_Texture* tex)
{
    const float h = art::PLATFORM_H;
    GameObject obj("moving_platform", -1);
    auto& m = obj.addComponent<MovementComponent>(
        type, centerX, centerY, amplitude, speed, phase);

    float cx = 0.0f;
    float cy = 0.0f;
    evaluateMovement(m, 0.0, cx, cy);

    obj.addComponent<TransformComponent>(cx - w / 2.0f, cy - h / 2.0f, w, h);
    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    obj.addComponent<TileComponent>(art::PLATFORM_W);
    obj.addComponent<CollidableComponent>();
    return obj;
}

// Hidden spawn point. No Renderable and no Color, so it is never drawn.
// floorY is the floor height where the player's feet go.
inline GameObject makeSpawnPoint(int index, float centerX, float floorY)
{
    GameObject obj("spawn_point", index);
    obj.addComponent<TransformComponent>(centerX, floorY, 0.0f, 0.0f);
    obj.addComponent<SpawnPointComponent>(index);
    return obj;
}

// Hidden death zone. Touching it sends the player to targetSpawn.
inline GameObject makeDeathZone(float x, float y, float w, float h,
                                int targetSpawn)
{
    GameObject obj("death_zone", -1);
    obj.addComponent<TransformComponent>(x, y, w, h);
    auto& c = obj.addComponent<CollidableComponent>();
    c.solid = false;   // a trigger: it never blocks the player
    obj.addComponent<DeathZoneComponent>(targetSpawn);
    return obj;
}

// Hidden scroll boundary. The game keeps it glued to the screen edge.
inline GameObject makeScrollBoundary(ScrollSide side, float inset)
{
    GameObject obj("scroll_boundary", -1);
    obj.addComponent<TransformComponent>(0.0f, -1000.0f, 16.0f, 3000.0f);
    auto& c = obj.addComponent<CollidableComponent>();
    c.solid = false;
    obj.addComponent<ScrollBoundaryComponent>(side, inset);
    return obj;
}

// Patrolling skull enemy. It is a visible death zone that moves.
inline GameObject makeSkull(float centerX, float floorY, float amplitude,
                            float speed, int targetSpawn, SDL_Texture* tex)
{
    const float size = 64.0f;

    GameObject obj("skull", -1);
    auto& m = obj.addComponent<MovementComponent>(
        MovementType::Horizontal, centerX, floorY - size / 2.0f,
        amplitude, speed, 0.0f);

    float cx = 0.0f;
    float cy = 0.0f;
    evaluateMovement(m, 0.0, cx, cy);

    obj.addComponent<TransformComponent>(cx - size / 2.0f, cy - size / 2.0f,
                                         size, size);

    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    r.frameCount = 6;
    r.frameWidth = 32;
    r.frameHeight = 32;
    r.frameDuration = 1.0f / 8.0f;
    r.useTimedAnimation = true;

    auto& c = obj.addComponent<CollidableComponent>();
    c.solid = false;
    obj.addComponent<DeathZoneComponent>(targetSpawn);
    return obj;
}

// A tall obstacle. Blocker + solid Collidable means the player can't walk
// through it but can jump on top of it.
inline GameObject makeTotem(float x, float floorY, SDL_Texture* tex)
{
    GameObject obj("totem", -1);
    obj.addComponent<TransformComponent>(x, floorY - 192.0f, 64.0f, 192.0f);
    obj.addComponent<CollidableComponent>();
    obj.addComponent<BlockerComponent>();

    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    r.frameCount = 8;
    r.frameWidth = 64;
    r.frameHeight = 192;
    r.frameDuration = 1.0f / 8.0f;
    r.useTimedAnimation = true;
    return obj;
}

// Crate from the princess art (64x64). Blocks sideways, can be stood on.
inline GameObject makeCrate(float x, float floorY, SDL_Texture* tex)
{
    GameObject obj("crate", -1);
    obj.addComponent<TransformComponent>(x, floorY - 64.0f, 64.0f, 64.0f);
    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    obj.addComponent<CollidableComponent>();
    obj.addComponent<BlockerComponent>();
    return obj;
}

// Star coin (64x64). Not solid; picked up by touching it.
inline GameObject makeCoin(float x, float y, SDL_Texture* tex)
{
    GameObject obj("coin", -1);
    obj.addComponent<TransformComponent>(x, y, 64.0f, 64.0f);
    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    auto& c = obj.addComponent<CollidableComponent>();
    c.solid = false;
    obj.addComponent<CollectibleComponent>(1);
    return obj;
}

// Decoration only: Transform + Renderable and nothing else.
inline GameObject makeBush(float x, float floorY, SDL_Texture* tex)
{
    GameObject obj("bush", -1);
    obj.addComponent<TransformComponent>(x, floorY - 90.0f, 120.0f, 90.0f);
    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    return obj;
}

// Cloud: scrolls slower than the level (Parallax) and drifts slowly
// left and right (the same MovementComponent the platforms use).
inline GameObject makeCloud(float x, float y, float phase, SDL_Texture* tex)
{
    GameObject obj("cloud", -1);
    obj.addComponent<MovementComponent>(MovementType::Horizontal,
                                        x + 110.0f, y + 55.0f,
                                        40.0f, 0.25f, phase);
    obj.addComponent<TransformComponent>(x, y, 220.0f, 110.0f);
    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    obj.addComponent<ParallaxComponent>(0.3f);
    return obj;
}

// Level exit.
inline GameObject makeGoal(float x, float floorY, SDL_Texture* tex)
{
    GameObject obj("goal", -1);
    obj.addComponent<TransformComponent>(x, floorY - 128.0f, 128.0f, 128.0f);

    auto& r = obj.addComponent<RenderableComponent>();
    r.texture = tex;
    r.frameCount = 4;
    r.frameWidth = 128;
    r.frameHeight = 128;
    r.frameDuration = 1.0f / 6.0f;
    r.useTimedAnimation = true;

    auto& c = obj.addComponent<CollidableComponent>();
    c.solid = false;
    obj.addComponent<GoalComponent>();
    return obj;
}

#endif
