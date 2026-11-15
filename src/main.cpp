#define SDL_MAIN_USE_CALLBACKS 1

#include "CellMap.h"
#include "Defer.h"
#include "Entities.h"
#include "Input.h"
#include "Level.h"
#include "LevelFetcher.h"
#include "Renderer.h"
#include "Sprite.h"
#include "TextureLoader.h"
#include "WebGpu.h"

#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <variant>

namespace
{
constexpr const ResourceName kCtAnimName_Idle = "CtIdle";
constexpr const ResourceName kCtAnimName_Run = "CtRun";
constexpr const ResourceName kCtAnimName_Walk = "CtWalk";

const SpriteAnimDef kCtAnimDef_Idle //
    {
        .Name = kCtAnimName_Idle,
        .Dim{ .w = 180, .h = 180 },
        .Textures //
        {
            "/assets/Anims/Caretaker/Idle/Idle_Body_337.png", // 337 actually points north
            "/assets/Anims/Caretaker/Idle/Idle_Body_000.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_022.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_045.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_067.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_090.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_112.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_135.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_157.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_180.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_202.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_225.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_247.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_270.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_292.png",
            "/assets/Anims/Caretaker/Idle/Idle_Body_315.png",
        },
    };

const SpriteAnimDef kCtAnimDef_Walk //
    {
        .Name = kCtAnimName_Walk,
        .Dim{ .w = 180, .h = 180 },
        .Textures //
        {
            "/assets/Anims/Caretaker/Walk/Walk_Body_000.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_022.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_045.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_067.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_090.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_112.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_135.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_157.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_180.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_202.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_225.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_247.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_270.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_292.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_315.png",
            "/assets/Anims/Caretaker/Walk/Walk_Body_337.png",
        },
    };

const SpriteAnimDef kCtAnimDef_Run //
    {
        .Name = kCtAnimName_Run,
        .Dim{ .w = 180, .h = 180 },
        .Textures //
        {
            "/assets/Anims/Caretaker/Run/Run_Body_000.png",
            "/assets/Anims/Caretaker/Run/Run_Body_022.png",
            "/assets/Anims/Caretaker/Run/Run_Body_045.png",
            "/assets/Anims/Caretaker/Run/Run_Body_067.png",
            "/assets/Anims/Caretaker/Run/Run_Body_090.png",
            "/assets/Anims/Caretaker/Run/Run_Body_112.png",
            "/assets/Anims/Caretaker/Run/Run_Body_135.png",
            "/assets/Anims/Caretaker/Run/Run_Body_157.png",
            "/assets/Anims/Caretaker/Run/Run_Body_180.png",
            "/assets/Anims/Caretaker/Run/Run_Body_202.png",
            "/assets/Anims/Caretaker/Run/Run_Body_225.png",
            "/assets/Anims/Caretaker/Run/Run_Body_247.png",
            "/assets/Anims/Caretaker/Run/Run_Body_270.png",
            "/assets/Anims/Caretaker/Run/Run_Body_292.png",
            "/assets/Anims/Caretaker/Run/Run_Body_315.png",
            "/assets/Anims/Caretaker/Run/Run_Body_337.png",
        },
    };

const SpriteAnimDef kCtSpriteAnimDefs[] //
    {
        kCtAnimDef_Idle,
        kCtAnimDef_Walk,
        kCtAnimDef_Run,
    };

/// Number of cells along each axis of the cell map.
constexpr uint32_t kCellMapCellsPerSide = 128;

const CellMapDef kCellMapDef //
    {
        .CellsPerSide = kCellMapCellsPerSide,
        .TileSets //
        {
            "/assets/sample/assets/tiles/dirt_tiles_4x4_256.png",
            "/assets/sample/assets/tiles/grass_tiles_4x4_256.png",
            //"/assets/sample/assets/tiles/coast_sand_05_diff_4k.png",
            //"/assets/sample/assets/tiles/Ground053_4K-PNG_Color.png",
        },
    };

struct Game
{
    static constexpr const char* const kWindowTitle = "GraveyardShift";
    static constexpr int kWindowWidth = 800;
    static constexpr int kWindowHeight = 600;

    // Logical width of the display in pixels
    static constexpr uint32_t kLogicalDisplayWidth = 800;

    // Logical height of the display in pixels
    static constexpr uint32_t kLogicalDisplayHeight = 600;

    /// Number of pixels along each axis of a cell.
    static constexpr uint32_t kCellPixelsPerSide = 64;

    /// Number of tiles along each axis in of a tile sheet.
    static constexpr uint32_t kSheetTilesPerSide = 1;

    Game() = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    ~Game()
    {
        // Release resources.
        m_State = EndState{};

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
        std::unique_ptr<Renderer> Renderer;
        Result<LevelFetcher> LevelFetcher;
        LevelLoader LevelLoader;

        Result<> Update()
        {
            GG_CHECK(LevelFetcher->Update(), "Failed to update level fetcher");
            GG_CHECK(LevelLoader.Update(), "Failed to update level loader");

            return Result<>::Ok;
        }

        bool IsPending() const { return LevelFetcher->IsPending() || LevelLoader.IsPending(); }
    };

    struct RunState
    {
        std::unique_ptr<Renderer> Renderer;
        Level Level;
    };

    struct FailedState
    {
    };

    struct EndState
    {
    };

    std::variant<FailedState, LoadingState, RunState, EndState> m_State;

    std::optional<Caretaker> Caretaker0;

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
            const Renderer::Params rendererParams //
                {
                    .LogicalDisplayWidth = kLogicalDisplayWidth,
                    .LogicalDisplayHeight = kLogicalDisplayHeight,
                    .CellPixelsPerSide = kCellPixelsPerSide,
                    .SheetTilesPerSide = kSheetTilesPerSide,
                };

            auto rendererResult = Renderer::Create(rendererParams, WebGpu);
            GG_CHECKB(rendererResult, "Failed to create renderer");

            std::unique_ptr<Renderer> renderer = std::move(*rendererResult);

            auto levelFetcherResult = LevelFetcher::Fetch("/assets/sample/level.json", FileFetcher);
            GG_CHECKB(levelFetcherResult, "Failed to start loading level");

            auto levelLoaderResult =
                LevelLoader::Load(kCtSpriteAnimDefs, kCellMapDef, FileFetcher, *renderer, WebGpu);
            GG_CHECKB(levelLoaderResult, "Failed to start loading level");

            m_State = LoadingState //
                {
                    .Renderer = std::move(renderer),
                    .LevelFetcher = std::move(levelFetcherResult),
                    .LevelLoader = std::move(*levelLoaderResult),
                };

            m_Stage = Stage::Loading;
        }
        break;
        case Stage::Loading:
        {
            auto& state = std::get<LoadingState>(m_State);

            GG_CHECK(state.Update(), "Failed to update loading state");

            if(!state.IsPending())
            {
                m_Stage = Stage::Loaded;
            }
        }
        break;
        case Stage::Loaded:
        {
            auto& oldState = std::get<LoadingState>(m_State);

            auto levelResult = oldState.LevelLoader.Take();
            GG_CHECKB(levelResult, "Failed to load level");

            m_State = RunState //
                {
                    .Renderer = std::move(oldState.Renderer),
                    .Level = std::move(*levelResult),
                };

            auto& newState = std::get<RunState>(m_State);

            auto caretakerResult = Caretaker::Create(newState.Level);
            GG_CHECKB(caretakerResult, "Failed to create caretaker");
            Caretaker0 = std::move(*caretakerResult);

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

    return Result<>::Ok;
}

Result<>
Game::Run()
{
    static uint64_t ms = SDL_GetTicks();

    auto& state = std::get<RunState>(m_State);

    const uint64_t curMs = SDL_GetTicks();
    const uint64_t deltaMs = curMs - ms;
    ms = curMs;
    const float deltaSec = static_cast<float>(deltaMs) / 1000.0f;

    GG_CHECKB(Caretaker0->Tick(deltaSec, Input, state.Level));

    state.Level.Tick(deltaSec);

    Input.Reset();

    state.Renderer->SetView(Caretaker0->GetPos().x, Caretaker0->GetPos().y);

    GG_CHECKB(state.Renderer->Render(state.Level), "Failed to render level");

    GG_CHECKB(WebGpu.Present(), "Failed to present");

    return Result<>::Ok;
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
