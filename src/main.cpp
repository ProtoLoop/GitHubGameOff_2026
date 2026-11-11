#define SDL_MAIN_USE_CALLBACKS 1

#include "Angle.h"
#include "CellMap.h"
#include "Defer.h"
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
#include <variant>

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
    {
        //
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

struct Game
{
    static constexpr const char* const kWindowTitle = "GraveyardShift";
    static constexpr int kWindowWidth = 800;
    static constexpr int kWindowHeight = 600;
    static constexpr uint32_t kCellMapGridCount = 128;
    static constexpr uint32_t kLogicalDisplayWidth = 800;
    static constexpr uint32_t kLogicalDisplayHeight = 600;

    Game() = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    ~Game()
    {
        // Release resources.
        m_State = std::monostate{};

        if(Window)
        {
            SDL_DestroyWindow(Window);
        }
    }

    enum class Stage
    {
        None,
        InitializingWebGpu,
        WebGpuReady,
        Loading,
        Loaded,
        Running,
        Stopped
    };

    Result<> Update();

    bool IsStopped() const { return m_Stage == Stage::Stopped; }

    Result<> Run();

    SDL_Window* Window = nullptr;
    WebGpu WebGpu;
    FileFetcher FileFetcher;
    Input Input;

    struct LoadingState
    {
        Result<SpriteLibLoader> SpriteLibLoader;
        Result<LevelLoader> LevelLoader;
        Result<TextureLoader> CellMapTextureLoader;
    };

    struct RunState
    {
        Result<SpriteLib> SpriteLib;
        Result<CellMap> CellMap;
        Result<Scene> Scene;
    };

    std::variant<std::monostate, LoadingState, RunState> m_State = LoadingState{};

    float ViewRow{ 0 };
    float ViewCol{ 0 };

    SDL_Point PlayerLoc{};
    Angle PlayerRot;
    Stage m_Stage = Stage::None;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::optional<Game> g_Game;

[[maybe_unused]] float
TurnsToRadians(uint32_t turns)
{
    return 2.0f
        * std::numbers::pi_v<float>
        * static_cast<float>(turns)
        / static_cast<float>(std::numeric_limits<uint32_t>::max());
}

Result<>
Game::Update()
{
    // If we exit due to error then set the stage to Stopped.
    auto stopRunning = GG_DEFER
    {
        m_Stage = Stage::Stopped;
    };

    Input.Update();

    float value{};
    if(Input.IsTriggered(Input::ActionId::Quit, value))
    {
        m_Stage = Stage::Stopped;
    }

    // Call every frame to catch device lost.
    GG_CHECK(WebGpu.Update());

    switch(m_Stage)
    {
        case Stage::None:
            break;
        case Stage::InitializingWebGpu:
            GG_CHECK(WebGpu.Update(), "Failed to update WebGpu");

            if(WebGpu.IsReady())
            {
                m_Stage = Stage::WebGpuReady;
            }
            break;
        case Stage::WebGpuReady:
        {
            auto& state = std::get<LoadingState>(m_State);

            state.SpriteLibLoader = SpriteLibLoader::Load(kSpriteSheetDefs, FileFetcher, WebGpu);
            GG_CHECKB(state.SpriteLibLoader, "Failed to start loading sprite lib loader");

            state.LevelLoader = LevelLoader::Load("/assets/sample/level.json", FileFetcher);
            GG_CHECKB(state.LevelLoader, "Failed to start loading level");

            const FilePath paths[] //
                {                  //"/assets/sample/assets/tiles/dirt_tiles_4x4_256.png",
                    "/assets/sample/assets/tiles/coast_sand_05_diff_4k.png",
                    "/assets/sample/assets/tiles/Ground053_4K-PNG_Color.png"
                };

            state.CellMapTextureLoader = TextureLoader::Load(paths, FileFetcher, WebGpu, "Test");
            GG_CHECKB(state.CellMapTextureLoader, "Failed to start texture load");

            m_Stage = Stage::Loading;
        }
            break;
        case Stage::Loading:
        {
                auto& state = std::get<LoadingState>(m_State);
                
                GG_CHECK(state.SpriteLibLoader->Update(), "Failed to update sprite lib loader");
                GG_CHECK(state.LevelLoader->Update(), "Failed to update level loader");

                if(!state.SpriteLibLoader->IsPending() && !state.LevelLoader->IsPending() && !state.CellMapTextureLoader->IsPending())
                {
                    m_Stage = Stage::Loaded;
                }
            }
            break;
        case Stage::Loaded:
        {
            auto& state = std::get<LoadingState>(m_State);
                
            auto spriteLibResult = state.SpriteLibLoader->Take();
            GG_CHECKB(spriteLibResult, "Failed to load sprite lib");

            auto texResult = state.CellMapTextureLoader->Take();
            GG_CHECKB(texResult, "Failed to load texture");

            auto cellmapResult = CellMap::Create(*texResult, kCellMapGridCount);
            GG_CHECK(cellmapResult, "Failed to create cell map");

            m_State = RunState
            {
                .SpriteLib = std::move(*spriteLibResult),
                .CellMap = std::move(*cellmapResult),
                .Scene = {}  
            };

            auto& runState = std::get<RunState>(m_State);

            BoundedVector<LostSoul> lostSouls(8); // NOLINT
            for(size_t i = 0; i < lostSouls.capacity(); ++i)
            {
                GG_CHECKB(lostSouls.push_back(LostSoul{}), "Failed to add lost soul");
            }

            runState.Scene = Scene::Create(kLogicalDisplayWidth,
                kLogicalDisplayHeight,
                WebGpu,
                *runState.SpriteLib,
                *runState.CellMap,
                std::move(lostSouls));
            GG_CHECKB(runState.Scene, "Failed to create scene");

            m_Stage = Stage::Running;
        }
        break;
        case Stage::Running:
            GG_CHECK(Run(), "Failed to run game");
            break;

        case Stage::Stopped:
            break;
    }

    stopRunning.cancel();

    return Result<>::Success();
}

Result<>
Game::Run()
{
    static constexpr float kTurnsPerSec = 1;

    constexpr uint64_t kAnimFps = 10;
    constexpr uint64_t kmspf = 1000 / kAnimFps;
    static uint32_t frame = 0;
    static uint64_t ms = SDL_GetTicks();
    static uint64_t elapsedMs = 0;

    auto& state = std::get<RunState>(m_State);

    const uint64_t curMs = SDL_GetTicks();
    const uint64_t deltaMs = curMs - ms;
    elapsedMs += deltaMs;
    ms = curMs;

    while(elapsedMs > kmspf)
    {
        frame = frame + 1;
        elapsedMs -= kmspf;
    }

    float value{};
    if(Input.IsTriggered(Input::ActionId::MoveLeft, value))
    {
        ViewCol -= value * 0.1f; // NOLINT

        PlayerLoc.x -= static_cast<int>(value);
    }
    if(Input.IsTriggered(Input::ActionId::MoveRight, value))
    {
        ViewCol += value * 0.1f; // NOLINT

        PlayerLoc.x += static_cast<int>(value);
    }
    if(Input.IsTriggered(Input::ActionId::MoveForward, value))
    {
        ViewRow -= value * 0.1f; // NOLINT

        PlayerLoc.y -= static_cast<int>(value);
    }
    if(Input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        ViewRow += value * 0.1f; // NOLINT

        PlayerLoc.y += static_cast<int>(value);
    }
    if(Input.IsTriggered(Input::ActionId::RotateLeft, value))
    {
        const float turns = (static_cast<float>(deltaMs) / 1000.0f) * kTurnsPerSec;
        PlayerRot -= Angle(turns);
    }
    if(Input.IsTriggered(Input::ActionId::RotateRight, value))
    {
        const float turns = (static_cast<float>(deltaMs) / 1000.0f) * kTurnsPerSec;
        PlayerRot += Angle(turns);
    }

    Input.Reset();

    constexpr int halfSquareSize = SquareSize / 2;

    PlayerLoc.x =
        std::min(std::max(PlayerLoc.x, halfSquareSize), kWindowWidth - halfSquareSize);
    PlayerLoc.y =
        std::min(std::max(PlayerLoc.y, halfSquareSize), kWindowHeight - halfSquareSize);

    const auto spriteSheetResult = state.SpriteLib->GetSpriteSheet("run_body");
    GG_ASSERT(spriteSheetResult, "Failed to get sprite sheet");

    const SpriteSheet* spriteSheet = *spriteSheetResult;
    const uint32_t dirIndex = PlayerRot.GetDirection(spriteSheet->GetRowCount());
    const uint32_t animFrame = frame % spriteSheet->GetColCount();
    const auto srcRectResult = spriteSheet->GetFrameRect(dirIndex, animFrame);
    GG_ASSERT(srcRectResult);

    state.Scene->SetFrameAndDir(0, animFrame, dirIndex);
    state.Scene->SetView(ViewRow, ViewCol);

    GG_CHECKB(state.Scene->Render(), "Failed to render scene");

    GG_CHECKB(WebGpu.Present(), "Failed to present");

    return Result<>::Success();
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

    g_Game.emplace();

    Game& game = *g_Game;

    game.Window = SDL_CreateWindow(Game::kWindowTitle,
        Game::kWindowWidth,
        Game::kWindowHeight,
        SDL_WINDOW_RESIZABLE);

    if(!GG_VERIFYB(game.Window))
    {
        GG_ERROR("Window creation failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if(!game.WebGpu.Start(game.Window))
    {
        return SDL_APP_FAILURE;
    }

    game.m_Stage = Game::Stage::InitializingWebGpu;

    *appstate = &game;
    return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppEvent(void* appstate, SDL_Event* event)
{
    Game& game = *static_cast<Game*>(appstate);
    switch(event->type)
    {
        case SDL_EVENT_QUIT:
            game.Input.TriggerAction(Input::ActionId::Quit, 1);
            break;
        case SDL_EVENT_KEY_DOWN:
            game.Input.TriggerKey(event->key.scancode, Input::Trigger::Pressed);
            break;
        case SDL_EVENT_KEY_UP:
            game.Input.TriggerKey(event->key.scancode, Input::Trigger::Released);
            break;
        default:
            break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppIterate(void* appstate)
{
    Game& game = *static_cast<Game*>(appstate);

    if(!game.Update())
    {
        return SDL_APP_FAILURE;
    }

    if(game.IsStopped())
    {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void
SDL_AppQuit(void* /*appstate*/, SDL_AppResult)
{
    // Destroy the renderer and window before shutting down SDL, including on failure.
    g_Game.reset();
    // SDL calls SDL_Quit after this callback returns.
}
