#define SDL_MAIN_USE_CALLBACKS 1

#include "BoundedVector.h"
#include "Input.h"
#include "Sprite.h"
#include "TextureLoader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace
{
constexpr int WindowWidth = 960;
constexpr int WindowHeight = 540;
constexpr int SquareSize = 64;

struct AppState
{
    AppState() = default;
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;
    AppState(AppState&&) = delete;
    AppState& operator=(AppState&&) = delete;

    ~AppState()
    {
        Sprites.clear();
        Textures.clear();
        TextureLoader.reset();

        if(Renderer)
        {
            SDL_DestroyRenderer(Renderer);
        }
        if(Window)
        {
            SDL_DestroyWindow(Window);
        }
    }

    enum class Stage
    {
        None,
        Loading,
        Running,
    };

    SDL_Window* Window = nullptr;
    SDL_Renderer* Renderer = nullptr;
    std::optional<TextureLoader> TextureLoader;

    std::array<TextureLoadRequest, 1> TexLoadRequests //
        {
            TextureLoadRequest{ "/assets/male_hero-walk.png" },
        };

    static constexpr size_t kSpriteCount = 10;

    BoundedVector<TexturePtr> Textures{1};
    BoundedVector<Sprite> Sprites{kSpriteCount};

    Input Input;

    static constexpr uint32_t kTurnsPerCircle = 180;
    static constexpr uint32_t kTurnStep = std::numeric_limits<uint32_t>::max() / kTurnsPerCircle;
    SDL_Point PlayerLoc{};
    uint32_t PlayerRot = 0;
    Stage m_Stage = Stage::None;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::optional<AppState> g_AppState;

float
TurnsToRadians(uint32_t turns)
{
    return 2.0f
        * std::numbers::pi_v<float>
        * static_cast<float>(turns)
        / static_cast<float>(std::numeric_limits<uint32_t>::max());
}

} // namespace

SDL_AppResult
SDL_AppInit(void** appstate, int, char**)
{
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_TRACE);

    *appstate = nullptr;
    if(!SDL_Init(SDL_INIT_VIDEO))
    {
        GG_ERROR("SDL initialization failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    g_AppState.emplace();

    AppState& state = *g_AppState;

    if(!SDL_CreateWindowAndRenderer("gameoff2026",
           WindowWidth,
           WindowHeight,
           SDL_WINDOW_RESIZABLE,
           &state.Window,
           &state.Renderer)
        || !SDL_SetRenderLogicalPresentation(state.Renderer,
            WindowWidth,
            WindowHeight,
            SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        GG_ERROR("Window/renderer setup failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Vsync is a preference. The browser also supplies its own frame scheduling.
    if(!SDL_SetRenderVSync(state.Renderer, 1))
    {
        GG_WARN("Vsync unavailable: {}", SDL_GetError());
    }

    state.TextureLoader.emplace(state.TexLoadRequests);

    if(GG_VERIFY(state.TextureLoader->Start(state.Renderer)))
    {
        state.m_Stage = AppState::Stage::Loading;
    }

    *appstate = &state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppEvent(void* appstate, SDL_Event* event)
{
    AppState& state = *static_cast<AppState*>(appstate);
    switch(event->type)
    {
        case SDL_EVENT_QUIT:
            state.Input.TriggerAction(Input::ActionId::Quit, 1);
            break;
        case SDL_EVENT_KEY_DOWN:
            state.Input.TriggerKey(event->key.scancode, Input::Trigger::Pressed);
            break;
        case SDL_EVENT_KEY_UP:
            state.Input.TriggerKey(event->key.scancode, Input::Trigger::Released);
            break;
        default:
            break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppIterate(void* appstate)
{
    AppState& state = *static_cast<AppState*>(appstate);

    state.Input.Update();

    float value{};
    if(state.Input.IsTriggered(Input::ActionId::Quit, value))
    {
        return SDL_APP_SUCCESS;
    }

    switch(state.m_Stage)
    {
        case AppState::Stage::None:
            break;
        case AppState::Stage::Loading:
            if(!state.TextureLoader || state.TextureLoader->IsPending())
            {
                return SDL_APP_CONTINUE;
            }

            for(TextureLoadRequest& request : state.TexLoadRequests)
            {
                if(request.Succeeded())
                {
                    auto textureResult = request.Take();
                    if(!GG_VERIFY(textureResult))
                    {
                        continue;
                    }

                    constexpr float size = 128;
                    float spriteX{};
                    for(size_t i = 0; i < AppState::kSpriteCount; ++i, spriteX += size)
                    {
                        auto spriteResult = Sprite::Create(textureResult->get(),
                            SDL_FRect{ .x = spriteX, .y = 0, .w = size, .h = size });
                        if(spriteResult)
                        {
                            state.Sprites.push_back(std::move(*spriteResult));
                        }
                    }

                    GG_VERIFY(state.Textures.push_back(std::move(*textureResult)));
                }
            }

            state.m_Stage = AppState::Stage::Running;
            break;
        case AppState::Stage::Running:
            break;
    }

    if(state.Input.IsTriggered(Input::ActionId::MoveLeft, value))
    {
        state.PlayerLoc.x -= static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveRight, value))
    {
        state.PlayerLoc.x += static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveForward, value))
    {
        state.PlayerLoc.y -= static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        state.PlayerLoc.y += static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::RotateLeft, value))
    {
        state.PlayerRot -= static_cast<uint32_t>(value * static_cast<float>(AppState::kTurnStep));
    }
    if(state.Input.IsTriggered(Input::ActionId::RotateRight, value))
    {
        state.PlayerRot += static_cast<uint32_t>(value * static_cast<float>(AppState::kTurnStep));
    }

    state.Input.Reset();

    constexpr int halfSquareSize = SquareSize / 2;

    state.PlayerLoc.x =
        std::min(std::max(state.PlayerLoc.x, halfSquareSize), WindowWidth - halfSquareSize);
    state.PlayerLoc.y =
        std::min(std::max(state.PlayerLoc.y, halfSquareSize), WindowHeight - halfSquareSize);

    const float a = TurnsToRadians(state.PlayerRot);
    const float c = std::cos(a);
    const float s = std::sin(a);

    constexpr float w = static_cast<float>(SquareSize);
    constexpr float h = static_cast<float>(SquareSize);
    const float cx = static_cast<float>(state.PlayerLoc.x);
    const float cy = static_cast<float>(state.PlayerLoc.y);

    const SDL_FPoint corners[] //
        {
            { .x = -w / 2, .y = -h / 2 },
            { .x = w / 2, .y = -h / 2 },
            { .x = w / 2, .y = h / 2 },
            { .x = -w / 2, .y = h / 2 },
        };

    SDL_FPoint points[5]; // NOLINT
    for(int i = 0; i < 4; ++i)
    {
        const float x = corners[i].x;
        const float y = corners[i].y;
        points[i] = { .x = cx + (x * c) - (y * s), .y = cy + (x * s) + (y * c) };
    }
    points[4] = points[0]; // close the outline

    const SDL_FRect square //
        {
            .x = static_cast<float>(state.PlayerLoc.x - halfSquareSize),
            .y = static_cast<float>(state.PlayerLoc.y - halfSquareSize),
            .w = SquareSize,
            .h = SquareSize,
        };

    constexpr SDL_FColor color = { .r = 1, .g = 1, .b = 1, .a = 1 };

    const SDL_Vertex vertices[] //
        {
            { .position = points[0], .color = color, .tex_coord{ .x = 0, .y = 0 } },
            { .position = points[1], .color = color, .tex_coord{ .x = 1, .y = 0 } },
            { .position = points[2], .color = color, .tex_coord{ .x = 1, .y = 1 } },
            { .position = points[3], .color = color, .tex_coord{ .x = 0, .y = 1 } },
        };

    const int indices[] = { 0, 1, 2, 0, 2, 3 };

    constexpr size_t kNumFrames = 10;
    constexpr uint64_t kFps = 10;
    constexpr uint64_t kmspf = 1000 / kFps;
    static size_t frame = 0;
    static uint64_t ms = SDL_GetTicks();
    static uint64_t elapsedMs = 0;

    const uint64_t curMs = SDL_GetTicks();
    elapsedMs += curMs - ms;
    ms = curMs;

    while(elapsedMs > kmspf)
    {
        frame = (frame + 1) % kNumFrames;
        elapsedMs -= kmspf;
    }

    const SDL_FRect dstRect //
        {
            .x = static_cast<float>(state.PlayerLoc.x - halfSquareSize),
            .y = static_cast<float>(state.PlayerLoc.y - halfSquareSize),
            .w = static_cast<float>(128),
            .h = static_cast<float>(128),
        };

    const Sprite* sprite = state.Sprites.TryGet(frame);

    auto renderSprite = [&]()
    {
        if(sprite)
        {
            if(!SDL_RenderTexture(state.Renderer,
                   sprite->GetTexture(),
                   &sprite->GetRect(),
                   &dstRect))
            {
                GG_ERROR("Failed to render texture: {}", SDL_GetError());
                return false;
            }
        }
        return true;
    };

    if(!SDL_SetRenderDrawColorFloat(state.Renderer, 0.0F, 0.0F, 0.25F, 1.0F)
        || !SDL_RenderClear(state.Renderer)
        || !SDL_SetRenderDrawColorFloat(state.Renderer, 0.0F, 0.75F, 1.0F, 1.0F)
        || !SDL_RenderFillRect(state.Renderer, &square)
        //|| !SDL_SetRenderDrawColorFloat(state.Renderer, 1, 0, 0, 1)
        //|| !SDL_RenderLines(state.Renderer, &points[0], std::size(points))
        || !SDL_RenderGeometry(state.Renderer,
            nullptr,
            &vertices[0],
            std::size(vertices),
            &indices[0],
            std::size(indices))
        || !renderSprite()
        || !SDL_RenderPresent(state.Renderer))
    {
        GG_ERROR("Rendering failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

void
SDL_AppQuit(void* /*appstate*/, SDL_AppResult)
{
    // Destroy the renderer and window before shutting down SDL, including on failure.
    g_AppState.reset();
    // SDL calls SDL_Quit after this callback returns.
}
