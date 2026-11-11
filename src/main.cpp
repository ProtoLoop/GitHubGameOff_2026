#define SDL_MAIN_USE_CALLBACKS 1

#include "Angle.h"
#include "CellMap.h"
#include "Input.h"
#include "LevelLoader.h"
#include "Scene.h"
#include "SpriteLib.h"
#include "TextureLoader.h"
#include "WebGpu.h"

#include <algorithm>
#include <cstdint>
#include <numbers>
#include <optional>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace
{
constexpr int SquareSize = 64;

const SpriteSheetDef kSpriteSheetDefs[] //
    {
        /*{
            .Path{ "/assets/male_hero-walk.png" },
            .Name{ "male_hero-walk" },
            .SpriteDims{ .Width = 128, .Height = 128 },
        },*/
        {
            .Path{ "/assets/run/Run_Body_180.png" },
            .Name{ "lost_soul" },
            .SpriteDims{ .w = 180, .h = 180 },
        },
        {
            .Path{ "/assets/run/Run_Body_180.png" },
            .Name{ "run_body" },
            .SpriteDims{ .w = 180, .h = 180 },
        },
    };

const SpriteAnimDef kSpriteAnimDefs[] //
    {                                 //
        {
            .Name{ "run_body" },
            .SpriteDims{ .w = 180, .h = 180 },
            .Directions //
            {
                "/assets/run/Run_Body_000.png",
                "/assets/run/Run_Body_022.png",
                "/assets/run/Run_Body_045.png",
                "/assets/run/Run_Body_067.png",
                "/assets/run/Run_Body_090.png",
                "/assets/run/Run_Body_112.png",
                "/assets/run/Run_Body_135.png",
                "/assets/run/Run_Body_157.png",
                "/assets/run/Run_Body_180.png",
                "/assets/run/Run_Body_202.png",
                "/assets/run/Run_Body_225.png",
                "/assets/run/Run_Body_247.png",
                "/assets/run/Run_Body_270.png",
                "/assets/run/Run_Body_292.png",
                "/assets/run/Run_Body_315.png",
                "/assets/run/Run_Body_337.png",
            },
        },
    };

struct AppState
{
    static constexpr const char* const kWindowTitle = "GraveyardShift";
    static constexpr int kWindowWidth = 800;
    static constexpr int kWindowHeight = 600;
    static constexpr uint32_t kCellMapGridCount = 128;
    static constexpr uint32_t kLogicalDisplayWidth = 800;
    static constexpr uint32_t kLogicalDisplayHeight = 600;

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

        if(Window)
        {
            SDL_DestroyWindow(Window);
        }
    }

    enum class Stage
    {
        None,
        InitializingWebGpu,
        Loading,
        Running,
    };

    SDL_Window* Window = nullptr;
    WebGpu WebGpu;
    Result<Scene> Scene;
    FileFetcher FileFetcher;
    Result<SpriteLibLoader> SpriteLibLoader;
    Result<LevelLoader> LevelLoader;
    Result<SpriteLib> SpriteLib;
    Result<CellMap> CellMap;

    Input Input;

    float ViewRow{ 0 };
    float ViewCol{ 0 };

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

    state.Window = SDL_CreateWindow(AppState::kWindowTitle,
        AppState::kWindowWidth,
        AppState::kWindowHeight,
        SDL_WINDOW_RESIZABLE);

    if(!GG_VERIFYB(state.Window))
    {
        GG_ERROR("Window creation failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if(!state.WebGpu.Start(state.Window))
    {
        return SDL_APP_FAILURE;
    }

    state.m_Stage = AppState::Stage::InitializingWebGpu;

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

    // Call every frame to catch device lost.
    if(!state.WebGpu.Update())
    {
        return SDL_APP_FAILURE;
    }

    switch(state.m_Stage)
    {
        case AppState::Stage::None:
            break;
        case AppState::Stage::InitializingWebGpu:
        {
            if(!state.WebGpu.IsReady())
            {
                return SDL_APP_CONTINUE;
            }

            state.SpriteLibLoader =
                SpriteLibLoader::Load(kSpriteSheetDefs, state.FileFetcher, state.WebGpu);

            if(!state.SpriteLibLoader)
            {
                return SDL_APP_FAILURE;
            }

            state.LevelLoader = LevelLoader::Load("/assets/sample/level.json", state.FileFetcher);
            if(!state.LevelLoader)
            {
                return SDL_APP_FAILURE;
            }

            state.m_Stage = AppState::Stage::Loading;
            return SDL_APP_CONTINUE;
        }
        case AppState::Stage::Loading:
        {
            if(state.SpriteLibLoader->IsPending())
            {
                if(!state.SpriteLibLoader->Update())
                {
                    return SDL_APP_FAILURE;
                }

                return SDL_APP_CONTINUE;
            }

            if(state.LevelLoader->IsPending())
            {
                state.LevelLoader->Update();
                return SDL_APP_CONTINUE;
            }

            auto spriteLibResult = state.SpriteLibLoader->Take();
            if(!GG_VERIFYB(spriteLibResult, "Failed to load sprite lib"))
            {
                return SDL_APP_FAILURE;
            }

            state.SpriteLib = std::move(*spriteLibResult);
            state.SpriteLibLoader = {};

            BoundedVector<LostSoul> lostSouls(8); // NOLINT
            for(size_t i = 0; i < lostSouls.capacity(); ++i)
            {
                if(!GG_VERIFYB(lostSouls.push_back(LostSoul{})))
                {
                    return SDL_APP_FAILURE;
                }
            }

            const FilePath paths[]//
            {
                //"/assets/sample/assets/tiles/dirt_tiles_4x4_256.png",
                "/assets/sample/assets/tiles/coast_sand_05_diff_4k.png",
                "/assets/sample/assets/tiles/Ground053_4K-PNG_Color.png"
            };

            auto texLoadResult =
                TextureLoader::Load(paths, state.FileFetcher, state.WebGpu, "Test");
            if(!texLoadResult)
            {
                return SDL_APP_FAILURE;
            }

            while(texLoadResult->IsPending())
            {
            }

            auto texResult = texLoadResult->Take();
            if(!texResult)
            {
                return SDL_APP_FAILURE;
            }

            state.CellMap =
                CellMap::Create(*texResult, AppState::kCellMapGridCount);
            if(!state.CellMap)
            {
                return SDL_APP_FAILURE;
            }

            state.Scene = Scene::Create(AppState::kLogicalDisplayWidth,
                AppState::kLogicalDisplayHeight,
                state.WebGpu,
                *state.SpriteLib,
                *state.CellMap,
                std::move(lostSouls));
            if(!state.Scene)
            {
                return SDL_APP_FAILURE;
            }

            state.m_Stage = AppState::Stage::Running;
        }
        break;
        case AppState::Stage::Running:
            break;
    }

    static constexpr float kTurnsPerSec = 1;

    constexpr uint64_t kAnimFps = 10;
    constexpr uint64_t kmspf = 1000 / kAnimFps;
    static uint32_t frame = 0;
    static uint64_t ms = SDL_GetTicks();
    static uint64_t elapsedMs = 0;

    const uint64_t curMs = SDL_GetTicks();
    const uint64_t deltaMs = curMs - ms;
    elapsedMs += deltaMs;
    ms = curMs;

    while(elapsedMs > kmspf)
    {
        frame = frame + 1;
        elapsedMs -= kmspf;
    }

    if(state.Input.IsTriggered(Input::ActionId::MoveLeft, value))
    {
        state.ViewCol -= value * 0.1f;//NOLINT

        state.PlayerLoc.x -= static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveRight, value))
    {
        state.ViewCol += value * 0.1f;//NOLINT

        state.PlayerLoc.x += static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveForward, value))
    {
        state.ViewRow -= value * 0.1f;//NOLINT

        state.PlayerLoc.y -= static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        state.ViewRow += value * 0.1f;//NOLINT

        state.PlayerLoc.y += static_cast<int>(value);
    }
    if(state.Input.IsTriggered(Input::ActionId::RotateLeft, value))
    {
        const float turns = (static_cast<float>(deltaMs) / 1000.0f) * kTurnsPerSec;
        state.PlayerRot -= Angle(turns);
    }
    if(state.Input.IsTriggered(Input::ActionId::RotateRight, value))
    {
        const float turns = (static_cast<float>(deltaMs) / 1000.0f) * kTurnsPerSec;
        state.PlayerRot += Angle(turns);
    }

    state.Input.Reset();

    constexpr int halfSquareSize = SquareSize / 2;

    state.PlayerLoc.x = std::min(std::max(state.PlayerLoc.x, halfSquareSize),
        AppState::kWindowWidth - halfSquareSize);
    state.PlayerLoc.y = std::min(std::max(state.PlayerLoc.y, halfSquareSize),
        AppState::kWindowHeight - halfSquareSize);

    const auto spriteSheetResult = state.SpriteLib->GetSpriteSheet("run_body");
    GG_ASSERT(spriteSheetResult, "Failed to get sprite sheet");

    const SpriteSheet* spriteSheet = *spriteSheetResult;
    const uint32_t dirIndex = state.PlayerRot.GetDirection(spriteSheet->GetRowCount());
    const uint32_t animFrame = frame % spriteSheet->GetColCount();
    const auto srcRectResult = spriteSheet->GetFrameRect(dirIndex, animFrame);
    GG_ASSERT(srcRectResult);

    state.Scene->SetFrameAndDir(0, animFrame, dirIndex);
    state.Scene->SetView(state.ViewRow, state.ViewCol);

    if(!GG_VERIFYB(state.Scene->Render(), "Failed to render scene"))
    {
        return SDL_APP_FAILURE;
    }

    if(!GG_VERIFYB(state.WebGpu.Present(), "Failed to present swapchain texture"))
    {
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
