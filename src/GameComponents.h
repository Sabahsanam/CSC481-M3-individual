#ifndef GAME_COMPONENTS_H
#define GAME_COMPONENTS_H

#include <cstdint>

#include "Component.h"

// Components added for the individual game.
//
// None of the existing object model files (GameObject.h, Components.h)
// were changed to support these. They plug into the same GameObject
// container, which is the point of the component-based design: a new
// kind of object or behavior is a new component, not a new class.

// Solid color for objects that are drawn as a plain rectangle
// (the purple and teal static platforms).
struct ColorComponent : public Component {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;

    ColorComponent() = default;
    ColorComponent(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}
};

// Draw a texture repeated across the object's width instead of
// stretching it (used for the platforms).
struct TileComponent : public Component {
    float tileWidth = 96.0f;

    TileComponent() = default;
    explicit TileComponent(float tileWidth) : tileWidth(tileWidth) {}
};

// Moves an object along a repeating pattern driven by game time.
// Position is a function of time, so it is identical every run and
// pausing or scaling the Timeline pauses or scales the object too.
enum class MovementType {
    Horizontal,   // back and forth along x
    Vertical,     // up and down along y
    Circular      // around a circle
};

struct MovementComponent : public Component {
    MovementType type = MovementType::Horizontal;
    float originX = 0.0f;   // center of the pattern
    float originY = 0.0f;
    float amplitude = 100.0f;   // distance (or radius for Circular)
    float speed = 1.0f;         // radians per second
    float phase = 0.0f;

    // How far the object moved this frame. Used to carry a player
    // that is standing on a moving platform.
    float deltaX = 0.0f;
    float deltaY = 0.0f;

    MovementComponent() = default;
    MovementComponent(MovementType type, float originX, float originY,
                      float amplitude, float speed, float phase = 0.0f)
        : type(type), originX(originX), originY(originY),
          amplitude(amplitude), speed(speed), phase(phase) {}
};

// Hidden object marking a place a player can start or respawn.
// The object's Transform position is the point on the floor where
// the player's feet go (x is the horizontal center).
struct SpawnPointComponent : public Component {
    int index = 0;

    SpawnPointComponent() = default;
    explicit SpawnPointComponent(int index) : index(index) {}
};

// Anything with this component sends the player to a spawn point when
// the player touches it. Used for the hidden pits and also for the
// visible skull enemy.
struct DeathZoneComponent : public Component {
    int targetSpawn = 0;

    DeathZoneComponent() = default;
    explicit DeathZoneComponent(int targetSpawn) : targetSpawn(targetSpawn) {}
};

// Hidden vertical boundary tied to the edge of the visible area. When
// the player touches it the scene scrolls so the player stays inside.
enum class ScrollSide { Left, Right };

struct ScrollBoundaryComponent : public Component {
    ScrollSide side = ScrollSide::Right;
    float inset = 600.0f;   // distance from that edge of the screen

    ScrollBoundaryComponent() = default;
    ScrollBoundaryComponent(ScrollSide side, float inset)
        : side(side), inset(inset) {}
};

// Touching this ends the level (the player is sent back to the start).
struct GoalComponent : public Component {};

// Blocks the player from walking through it sideways (crates, totem).
// Pair it with a solid CollidableComponent so the top can be stood on.
struct BlockerComponent : public Component {};

// A star the player can pick up. When collected the game removes the
// object's RenderableComponent, which makes it a hidden object.
struct CollectibleComponent : public Component {
    int value = 1;
    bool collected = false;

    CollectibleComponent() = default;
    explicit CollectibleComponent(int value) : value(value) {}
};

// Scenery that scrolls slower than the level to fake depth (clouds).
struct ParallaxComponent : public Component {
    float factor = 0.3f;   // 0 = fixed to the screen, 1 = moves with the level

    ParallaxComponent() = default;
    explicit ParallaxComponent(float factor) : factor(factor) {}
};

#endif
