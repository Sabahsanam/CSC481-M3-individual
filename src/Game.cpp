#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <cstdio>

#include "GameLogic.h"
#include "Input.h"
#include "Timeline.h"

// Individual game built on the team engine: a side-scrolling platformer.
//
// Controls
//   A / D       move left / right
//   Shift       run
//   W           jump
//   P           pause / unpause
//   - / +       slow down / speed up the game (Timeline scale)
//   F1          show the hidden objects (spawn points, death zones,
//               scroll boundaries)
//   Esc         quit

// Draws one GameObject. Objects with neither a Renderable nor a Color are
// hidden and are skipped.
static void drawObject(SDL_Renderer* renderer, GameObject& obj, float cameraX,
                       bool flip = false)
{
    auto* t = obj.getComponent<TransformComponent>();
    auto* r = obj.getComponent<RenderableComponent>();
    auto* color = obj.getComponent<ColorComponent>();

    if (!t || (!r && !color)) {
        return;
    }

    // Far-away scenery (clouds) scrolls slower than the level
    if (auto* p = obj.getComponent<ParallaxComponent>()) {
        cameraX *= p->factor;
    }

    SDL_FRect dst = { t->x - cameraX, t->y, t->width, t->height };

    // Skip anything completely off screen
    if (dst.x + dst.w < 0.0f || dst.x > level::SCREEN_W) {
        return;
    }

    if (color) {
        SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);
        SDL_RenderFillRect(renderer, &dst);
        return;
    }

    if (!r->texture) {
        // No texture loaded: plain rectangle
        SDL_SetRenderDrawColor(renderer, 150, 100, 70, 255);
        SDL_RenderFillRect(renderer, &dst);
        return;
    }

    if (auto* tile = obj.getComponent<TileComponent>()) {
        for (float x = 0.0f; x < t->width; x += tile->tileWidth) {
            SDL_FRect piece = { dst.x + x, dst.y, tile->tileWidth, dst.h };
            SDL_RenderTexture(renderer, r->texture, nullptr, &piece);
        }
        return;
    }

    if (r->frameCount > 1 && r->frameWidth > 0 && r->frameHeight > 0) {
        SDL_FRect src = {
            static_cast<float>(r->currentFrame * r->frameWidth), 0.0f,
            static_cast<float>(r->frameWidth),
            static_cast<float>(r->frameHeight)
        };
        SDL_RenderTextureRotated(renderer, r->texture, &src, &dst, 0.0, nullptr,
                                 flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    } else {
        SDL_RenderTexture(renderer, r->texture, nullptr, &dst);
    }
}

// Debug view: draws the hidden objects as translucent shapes.
static void drawHiddenObjects(SDL_Renderer* renderer, GameWorld& w)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (auto& obj : w.objects) {
        auto* t = obj.getComponent<TransformComponent>();
        if (!t) {
            continue;
        }
        if (obj.hasComponent<RenderableComponent>() ||
            obj.hasComponent<ColorComponent>()) {
            continue;   // visible objects are not hidden
        }

        if (obj.hasComponent<SpawnPointComponent>()) {
            SDL_FRect marker = { t->x - w.cameraX - 14.0f, t->y - 90.0f, 28.0f, 90.0f };
            SDL_SetRenderDrawColor(renderer, 60, 220, 90, 150);
            SDL_RenderFillRect(renderer, &marker);

            auto* sp = obj.getComponent<SpawnPointComponent>();
            char label[16];
            std::snprintf(label, sizeof(label), "SPAWN %d", sp->index);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(renderer, marker.x - 14.0f, marker.y - 12.0f, label);
        }
        else if (obj.hasComponent<DeathZoneComponent>()) {
            SDL_FRect zone = { t->x - w.cameraX, t->y, t->width, t->height };
            SDL_SetRenderDrawColor(renderer, 230, 50, 50, 110);
            SDL_RenderFillRect(renderer, &zone);
        }
        else if (obj.hasComponent<ScrollBoundaryComponent>()) {
            SDL_FRect line = { t->x - w.cameraX, t->y, t->width, t->height };
            SDL_SetRenderDrawColor(renderer, 60, 120, 255, 140);
            SDL_RenderFillRect(renderer, &line);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if (!SDL_CreateWindowAndRenderer("Individual Game", 1280, 720,
                                     SDL_WINDOW_RESIZABLE,
                                     &window, &renderer)) {
        SDL_Log("Could not create window/renderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // The game is laid out for 1920x1080 and scales to any window size
    SDL_SetRenderLogicalPresentation(renderer,
                                     static_cast<int>(level::SCREEN_W),
                                     static_cast<int>(level::SCREEN_H),
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // Textures
    LevelTextures tex;
    tex.player = IMG_LoadTexture(renderer, "../assets/my_player.png");
    tex.platform = IMG_LoadTexture(renderer, "../assets/my_platform.png");
    tex.crate  = IMG_LoadTexture(renderer, "../assets/my_crate.png");
    tex.coin   = IMG_LoadTexture(renderer, "../assets/coin.png");
    tex.bush   = IMG_LoadTexture(renderer, "../assets/bush.png");
    tex.cloud  = IMG_LoadTexture(renderer, "../assets/cloud.png");
    tex.background = IMG_LoadTexture(renderer, "../assets/background.png");
    tex.skull  = IMG_LoadTexture(renderer, "../assets/enemy_skull.png");
    tex.portal = IMG_LoadTexture(renderer, "../assets/swirlingorb.png");
    tex.totem  = IMG_LoadTexture(renderer, "../assets/totem.png");

    if (!tex.player || !tex.platform || !tex.crate || !tex.coin || !tex.bush ||
        !tex.cloud || !tex.background || !tex.skull || !tex.portal || !tex.totem) {
        SDL_Log("Some textures did not load (run from the build folder): %s",
                SDL_GetError());
    }

    GameWorld world;
    game::buildLevel(world, tex);

    Timeline gameTime;

    bool running = true;
    bool showHidden = false;
    bool f1WasPressed = false;
    bool pauseWasPressed = false;
    bool minusWasPressed = false;
    bool plusWasPressed = false;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        if (Input::isKeyPressed(SDL_SCANCODE_ESCAPE)) {
            running = false;
        }

        float dt = static_cast<float>(gameTime.getDeltaTime());

        // Pause / unpause
        bool pauseNow = Input::isKeyPressed(SDL_SCANCODE_P);
        if (pauseNow && !pauseWasPressed) {
            if (gameTime.isPaused()) {
                gameTime.unpause();
            } else {
                gameTime.pause();
            }
        }
        pauseWasPressed = pauseNow;

        // Timeline speed: 0.5x, 1x, 2x
        bool minusNow = Input::isKeyPressed(SDL_SCANCODE_MINUS);
        bool plusNow = Input::isKeyPressed(SDL_SCANCODE_EQUALS);
        if (minusNow && !minusWasPressed) {
            double s = gameTime.getScale();
            if (s == 2.0) gameTime.setScale(1.0);
            else if (s == 1.0) gameTime.setScale(0.5);
        }
        if (plusNow && !plusWasPressed) {
            double s = gameTime.getScale();
            if (s == 0.5) gameTime.setScale(1.0);
            else if (s == 1.0) gameTime.setScale(2.0);
        }
        minusWasPressed = minusNow;
        plusWasPressed = plusNow;

        // Show / hide the hidden objects
        bool f1Now = Input::isKeyPressed(SDL_SCANCODE_F1);
        if (f1Now && !f1WasPressed) {
            showHidden = !showHidden;
        }
        f1WasPressed = f1Now;

        // Read the keyboard into a GameInput and advance the game
        GameInput input;
        input.left  = Input::isKeyPressed(SDL_SCANCODE_A);
        input.right = Input::isKeyPressed(SDL_SCANCODE_D);
        input.jump  = Input::isKeyPressed(SDL_SCANCODE_W);
        input.run   = Input::isKeyPressed(SDL_SCANCODE_LSHIFT) ||
                      Input::isKeyPressed(SDL_SCANCODE_RSHIFT);

        if (!gameTime.isPaused()) {
            game::step(world, input, dt);
        }

        // ---- Draw ----
        SDL_SetRenderDrawColor(renderer, 169, 186, 157, 255);
        SDL_RenderClear(renderer);

        if (tex.background) {
            SDL_RenderTexture(renderer, tex.background, nullptr, nullptr);
        }

        for (auto& obj : world.objects) {
            drawObject(renderer, obj, world.cameraX);
        }
        drawObject(renderer, world.player, world.cameraX, world.facingLeft);

        if (showHidden) {
            drawHiddenObjects(renderer, world);
        }

        // On-screen help and counters
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderDebugText(renderer, 10.0f, 8.0f,
                            "A/D move  Shift run  W jump  P pause  F1 hidden objects");

        char status[128];
        std::snprintf(status, sizeof(status),
                      "Stars: %d/%d   Deaths: %d   Wins: %d   Speed: %.1fx",
                      world.coins, world.totalCoins, world.deaths, world.wins,
                      gameTime.getScale());
        SDL_RenderDebugText(renderer, 10.0f, 24.0f, status);

        if (gameTime.isPaused()) {
            SDL_RenderDebugText(renderer, 10.0f, 40.0f, "PAUSED");
        }
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(tex.player);
    SDL_DestroyTexture(tex.platform);
    SDL_DestroyTexture(tex.crate);
    SDL_DestroyTexture(tex.coin);
    SDL_DestroyTexture(tex.bush);
    SDL_DestroyTexture(tex.cloud);
    SDL_DestroyTexture(tex.background);
    SDL_DestroyTexture(tex.skull);
    SDL_DestroyTexture(tex.portal);
    SDL_DestroyTexture(tex.totem);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
