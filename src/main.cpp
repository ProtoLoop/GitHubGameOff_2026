#define SDL_MAIN_USE_CALLBACKS 1

#include "Angle.h"
#include "Input.h"
#include "SpriteLib.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace
{
constexpr int SquareSize = 64;

const SpriteSheetDef kSpriteSheetDefs[] //
    {
        {
            .Path{ "/assets/male_hero-walk.png" },
            .Sprites //
            {
                {
                    .Name{ "male_hero-walk" },
                    .Frames = SpriteFrameListDef //
                    {
                        .Rects//
                        {
                            { .X = 0, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 128, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 256, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 384, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 512, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 640, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 768, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 896, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 1024, .Y = 0, .Width = 128, .Height = 128 },
                            { .X = 1152, .Y = 0, .Width = 128, .Height = 128 },
                        },
                    },
                },            }
        },
        {
            .Path{ "/assets/walk.png" },
            .Sprites //
            {
                {
                    .Name{ "walk-down" },
                    .Frames =
                        SpriteFrameDef //
                    {
                        .Rect = { .X = 0, .Y = 0 * 64, .Width = 384, .Height = 64 },
                        .Count = 8,
                    },
                },
                {
                    .Name{ "walk-down-left" },
                    .Frames =
                        SpriteFrameDef //
                    {
                        .Rect = { .X = 0, .Y = 1 * 64, .Width = 384, .Height = 64 },
                        .Count = 8,
                    },
                },
                {
                    .Name{ "walk-up-left" },
                    .Frames =
                        SpriteFrameDef //
                    {
                        .Rect = { .X = 0, .Y = 2 * 64, .Width = 384, .Height = 64 },
                        .Count = 8,
                    },
                },
                {
                    .Name{ "walk-up" },
                    .Frames =
                        SpriteFrameDef //
                    {
                        .Rect = { .X = 0, .Y = 3 * 64, .Width = 384, .Height = 64 },
                        .Count = 8,
                    },
                },
                {
                    .Name{ "walk-up-right" },
                    .Frames =
                        SpriteFrameDef //
                    {
                        .Rect = { .X = 0, .Y = 4 * 64, .Width = 384, .Height = 64 },
                        .Count = 8,
                    },
                },
                {
                    .Name{ "walk-down-right" },
                    .Frames =
                        SpriteFrameDef //
                    {
                        .Rect = { .X = 0, .Y = 5 * 64, .Width = 384, .Height = 64 },
                        .Count = 8,
                    },
                },
            },
        },
    };

struct AppState
{
    static constexpr const char* WindowTitle = "GraveyardShift";
    static constexpr int WindowWidth = 800;
    static constexpr int WindowHeight = 600;

//constexpr int WindowWidth = 960;
//constexpr int WindowHeight = 540;

    AppState() = default;
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;
    AppState(AppState&&) = delete;
    AppState& operator=(AppState&&) = delete;

    ~AppState()
    {
        // Release sprite resources.
        SpriteLibLoader = {};
        SpriteLib = {};

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
    Result<SpriteLibLoader> SpriteLibLoader;
    SpriteLib SpriteLib;

    Input Input;

    SDL_Point PlayerLoc{};
    Angle PlayerRot;
    Stage m_Stage = Stage::None;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::optional<AppState> g_AppState;

[[maybe_unused]] float
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

    state.Window = SDL_CreateWindow(AppState::WindowTitle,
                     AppState::WindowWidth,
                     AppState::WindowHeight,
                     SDL_WINDOW_RESIZABLE);

    if(!GG_VERIFY(state.Window))
    {
        GG_ERROR("Window creation failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state.Renderer = SDL_CreateRenderer(state.Window, nullptr);
    //state.Renderer = SDL_CreateGPURenderer(nullptr, state.Window);
    if(!GG_VERIFY(state.Renderer))
    {
        GG_ERROR("Renderer creation failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if(!GG_VERIFY(SDL_SetRenderLogicalPresentation(state.Renderer,
        AppState::WindowWidth,
        AppState::WindowHeight,
        SDL_LOGICAL_PRESENTATION_LETTERBOX)))
    {
        GG_ERROR("Failed to set logical presentation for renderer: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Vsync is a preference. The browser also supplies its own frame scheduling.
    if(!SDL_SetRenderVSync(state.Renderer, 1))
    {
        GG_WARN("Vsync unavailable: {}", SDL_GetError());
    }

    state.SpriteLibLoader = SpriteLibLoader::Load(kSpriteSheetDefs, state.Renderer);

    if(!GG_VERIFY(state.SpriteLibLoader))
    {
        return SDL_APP_FAILURE;
    }

    state.m_Stage = AppState::Stage::Loading;

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
        {
            state.SpriteLibLoader->Update();
            if(state.SpriteLibLoader->IsPending())
            {
                return SDL_APP_CONTINUE;
            }

            if(!GG_VERIFY(state.SpriteLibLoader->IsLoaded()))
            {
                return SDL_APP_FAILURE;
            }

            auto spriteLibResult = state.SpriteLibLoader->Take();
            if(!GG_VERIFY(spriteLibResult, "Failed to load sprite lib"))
            {
                return SDL_APP_FAILURE;
            }

            state.SpriteLib = std::move(*spriteLibResult);
            state.SpriteLibLoader = {};
            state.m_Stage = AppState::Stage::Running;
        }
            break;
        case AppState::Stage::Running:
            break;
    }

    static constexpr std::string_view kSpriteDirNames[]//
    {
        "walk-up",
        "walk-up-right",
        "walk-down-right",
        "walk-down",
        "walk-down-left",
        "walk-up-left",
    };

    static std::string_view spriteName = kSpriteDirNames[0];

    static constexpr float kTurnsPerSec = 1;
    static constexpr uint32_t kNumDirs = std::size(kSpriteDirNames);

    constexpr uint64_t kAnimFps = 10;
    constexpr uint64_t kmspf = 1000 / kAnimFps;
    static uint32_t frame = 0;
    static uint64_t ms = SDL_GetTicks();
    static uint64_t elapsedMs = 0;

    const uint64_t curMs = SDL_GetTicks();
    elapsedMs += curMs - ms;
    ms = curMs;

    uint64_t msThisFrame = 0;

    while(elapsedMs > kmspf)
    {
        frame = frame + 1;
        elapsedMs -= kmspf;
        msThisFrame += kmspf;
    }

    if(state.Input.IsTriggered(Input::ActionId::MoveLeft, value))
    {
        spriteName = "walk-up-left";
        state.PlayerLoc.x -= static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveRight, value))
    {
        spriteName = "walk-up-right";
        state.PlayerLoc.x += static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveForward, value))
    {
        spriteName = "walk-up";
        state.PlayerLoc.y -= static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        spriteName = "walk-down";
        state.PlayerLoc.y += static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::RotateLeft, value))
    {
        const float turns = (static_cast<float>(msThisFrame) / 1000.0f) * kTurnsPerSec;
        state.PlayerRot -= Angle(turns);
    }
    if(state.Input.IsTriggered(Input::ActionId::RotateRight, value))
    {
        const float turns = (static_cast<float>(msThisFrame) / 1000.0f) * kTurnsPerSec;
        state.PlayerRot += Angle(turns);
    }

    state.Input.Reset();

    constexpr int halfSquareSize = SquareSize / 2;

    state.PlayerLoc.x =
        std::min(std::max(state.PlayerLoc.x, halfSquareSize), AppState::WindowWidth - halfSquareSize);
    state.PlayerLoc.y =
        std::min(std::max(state.PlayerLoc.y, halfSquareSize), AppState::WindowHeight - halfSquareSize);

    const SDL_FRect square //
        {
            .x = static_cast<float>(state.PlayerLoc.x - halfSquareSize),
            .y = static_cast<float>(state.PlayerLoc.y - halfSquareSize),
            .w = SquareSize,
            .h = SquareSize,
        };

    const uint32_t dirIndex = state.PlayerRot.GetDirection(kNumDirs);
    spriteName = kSpriteDirNames[dirIndex];
    const auto spriteResult = state.SpriteLib.GetSprite(spriteName);
    GG_ASSERT(spriteResult, "Failed to get sprite");

    const Sprite* sprite = *spriteResult;

    const SDL_FRect srcRect = sprite->GetRect(frame % sprite->GetFrameCount());

    const SDL_FRect dstRect //
        {
            .x = static_cast<float>(state.PlayerLoc.x - halfSquareSize),
            .y = static_cast<float>(state.PlayerLoc.y - halfSquareSize),
            .w = static_cast<float>(srcRect.w),
            .h = static_cast<float>(srcRect.h),
        };

    auto renderSprite = [&]()
    {
        if(sprite)
        {
            if(!SDL_RenderTexture(state.Renderer,
                   sprite->GetTexture(),
                   &srcRect,
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
        || !SDL_SetRenderDrawColorFloat(state.Renderer, 1, 1, 1, 1)
        || !SDL_RenderFillRect(state.Renderer, &square)
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
