#define SDL_MAIN_USE_CALLBACKS 1

#include "AnimData.h"
#include "CellMap.h"
#include "Defer.h"
#include "Game.h"
#include "Input.h"
#include "Level.h"
#include "LevelFetcher.h"
#include "Renderer.h"
#include "Sprite.h"
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

struct App
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

    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    ~App()
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
    Input Input;

    struct LoadingState
    {
        LoadingState(const std::string_view levelPath,
            std::unique_ptr<Renderer> renderer,
            class WebGpu& webGpu)
            : Renderer(std::move(renderer)),
              LevelFetcher(LevelFetcher::Fetch(levelPath, FileFetcher)),
              LevelLoader(LevelLoader::Load(
                  AnimData::GetAnimDefs(), kCellMapDef, FileFetcher, *Renderer, webGpu))
        {
            GG_ASSERT(LevelFetcher, "Failed to initialize level fetcher");
            GG_ASSERT(LevelLoader, "Failed to initialize level loader");
        }

        class FileFetcher FileFetcher;
        std::unique_ptr<Renderer> Renderer;
        Result<LevelFetcher> LevelFetcher;
        Result<LevelLoader> LevelLoader;

        Result<> Update()
        {
            GG_CHECKB(LevelFetcher, "Level fetcher not initialized");
            GG_CHECKB(LevelLoader, "Level loader not initialized");

            GG_CHECK(LevelFetcher->Update(), "Failed to update level fetcher");
            GG_CHECK(LevelLoader->Update(), "Failed to update level loader");

            return Result<>::Ok;
        }

        bool IsPending() const { return LevelFetcher->IsPending() || LevelLoader->IsPending(); }
    };

    struct FailedState
    {
    };

    struct EndState
    {
    };

    std::variant<FailedState, LoadingState, Game, EndState> m_State;

    Stage m_Stage = Stage::None;

    static std::optional<App> m_App;
};

std::optional<App> App::m_App;

[[maybe_unused]] float
TurnsToRadians(uint32_t turns)
{
    return 2.0f
        * std::numbers::pi_v<float>
        * static_cast<float>(turns)
        / static_cast<float>(std::numeric_limits<uint32_t>::max());
}

Result<>
App::Update()
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

            m_State.emplace<LoadingState>("/assets/sample/level.json", std::move(renderer), WebGpu);

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

            auto levelResult = oldState.LevelLoader->Take();
            GG_CHECKB(levelResult, "Failed to load level");

            // Save the renderer prior to destroying the old state
            std::unique_ptr<Renderer> renderer = std::move(oldState.Renderer);

            m_State.emplace<Game>(Input, std::move(*levelResult), std::move(renderer));

            auto& game = std::get<Game>(m_State);

            GG_CHECKB(game.Begin(), "Failed to begin game");

            m_Stage = Stage::Running;
        }
        break;
        case Stage::Running:
            GG_CHECK(Run(), "Failed to run app");
            break;

        case Stage::Stopped:
            break;
    }

    stopRunning.cancel();

    return Result<>::Ok;
}

Result<>
App::Run()
{
    static uint64_t ms = SDL_GetTicks();

    auto& game = std::get<Game>(m_State);

    const uint64_t curMs = SDL_GetTicks();
    const uint64_t deltaMs = curMs - ms;
    ms = curMs;
    const float deltaSec = static_cast<float>(deltaMs) / 1000.0f;

    GG_CHECKB(game.Tick(deltaSec), "Failed to tick game");

    Input.Reset();

    GG_CHECKB(game.GetRenderer().Render(game.GetLevel()), "Failed to render level");

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

    App::m_App.emplace();

    App::m_App->Window = SDL_CreateWindow(App::kWindowTitle,
        App::kWindowWidth,
        App::kWindowHeight,
        SDL_WINDOW_RESIZABLE);

    if(!GG_VERIFYB(App::m_App->Window))
    {
        GG_ERROR("Window creation failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if(!App::m_App->WebGpu.Start(App::m_App->Window))
    {
        return SDL_APP_FAILURE;
    }

    App::m_App->m_Stage = App::Stage::InitializingWebGpu;

    *appstate = &App::m_App.value();
    return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppEvent(void* appstate, SDL_Event* event)
{
    App& game = *static_cast<App*>(appstate);
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
    App& app = *static_cast<App*>(appstate);

    if(!app.Update())
    {
        return SDL_APP_FAILURE;
    }

    if(app.IsStopped())
    {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void
SDL_AppQuit(void* /*appstate*/, SDL_AppResult)
{
    // Destroy the renderer and window before shutting down SDL, including on failure.
    App::m_App.reset();
    // SDL calls SDL_Quit after this callback returns.
}
