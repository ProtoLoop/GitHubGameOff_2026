#define SDL_MAIN_USE_CALLBACKS 1

#include "AudioPlayer.h"
#include "Defer.h"
#include "Game.h"
#include "Input.h"
#include "LevelLoader.h"
#include "LevelParser.h"
#include "Renderer.h"
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
constexpr const char* kLevelFilePath = "/level.json";

struct App
{
    static constexpr const char* const kWindowTitle = "GraveyardShift";
    static constexpr int kWindowWidth = 1024;
    static constexpr int kWindowHeight = 768;

    // Logical width of the display in pixels
    static constexpr uint32_t kLogicalDisplayWidth = kWindowWidth * 3 / 2;

    // Logical height of the display in pixels
    static constexpr uint32_t kLogicalDisplayHeight = kWindowHeight * 3 / 2;

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
        LoadingState(const FilePath& levelPath,
            std::unique_ptr<FileFetcher> fileFetcher,
            std::unique_ptr<Renderer> renderer,
            AudioPlayer&& audioPlayer,
            class WebGpu& webGpu)
            : FileFetcher(std::move(fileFetcher)),
              Renderer(std::move(renderer)),
              AudioPlayer(std::move(audioPlayer)),
              WebGpu(&webGpu),
              LevelParser(LevelParser::Fetch(levelPath, *FileFetcher))
        {
            GG_ASSERT(LevelParser, "Failed to initialize level parser");
        }

        enum class Stage
        {
            FetchingLevel,
            LoadingLevel,
            Succeeded,
            Failed
        };

        // Had to allocate FileFetcher and Renderer on the heap
        // because they are too large to be allocated inline when running web assembly.
        std::unique_ptr<FileFetcher> FileFetcher;
        std::unique_ptr<Renderer> Renderer;
        AudioPlayer AudioPlayer;
        class WebGpu* WebGpu;
        Result<LevelParser> LevelParser;
        Result<ParsedLevel> ParsedLevel;
        Result<LevelLoader> LevelLoader;

        Stage m_Stage{ Stage::FetchingLevel };

        Result<> Update()
        {
            switch(m_Stage)
            {
                case Stage::FetchingLevel:
                    // If we early exit due to error then we'll be in the failure stage.
                    m_Stage = Stage::Failed;

                    GG_CHECK(LevelParser, "Level parser is not initialized");

                    GG_CHECK(LevelParser->Update(), "Failed to update level parser");

                    if(LevelParser->IsPending())
                    {
                        m_Stage = Stage::FetchingLevel;
                    }
                    else
                    {
                        ParsedLevel = LevelParser->Take();
                        GG_CHECK(ParsedLevel, "Failed to take parsed level");

                        LevelLoader = LevelLoader::Load(ParsedLevel->GetLevelDef(),
                            *FileFetcher,
                            *Renderer,
                            *WebGpu,
                            AudioPlayer);

                        GG_CHECK(LevelLoader, "Failed to create level loader");

                        // Release the level fetcher once it's no longer needed.
                        LevelParser.Reset();

                        m_Stage = Stage::LoadingLevel;
                    }
                    break;
                case Stage::LoadingLevel:
                    m_Stage = Stage::Failed;
                    GG_CHECK(LevelLoader->Update(), "Failed to update level loader");
                    if(LevelLoader->IsPending())
                    {
                        m_Stage = Stage::LoadingLevel;
                    }
                    else
                    {
                        m_Stage = Stage::Succeeded;
                    }
                    break;

                case Stage::Succeeded:
                case Stage::Failed:
                    break;
            }

            return Result<>::Ok;
        }

        bool IsPending() const
        {
            return Stage::FetchingLevel == m_Stage || Stage::LoadingLevel == m_Stage;
        }
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

            auto audioPlayerResult = AudioPlayer::Create();
            GG_CHECKB(audioPlayerResult, "Failed to create audio player");

            GG_TRY(levelPath,
                FilePath::Create(kLevelFilePath),
                "Failed to create file path from {}",
                kLevelFilePath);

            m_State.emplace<LoadingState>(levelPath,
                std::make_unique<FileFetcher>(),
                std::move(*rendererResult),
                std::move(*audioPlayerResult),
                WebGpu);

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

            // Save the references to systems prior to destroying the old state
            std::unique_ptr<Renderer> renderer = std::move(oldState.Renderer);
            AudioPlayer audioPlayer = std::move(oldState.AudioPlayer);

            auto gameResult = Game::Create(Input,
                oldState.ParsedLevel->GetLevelDef(),
                std::move(*levelResult),
                std::move(renderer),
                std::move(audioPlayer));
            GG_CHECKB(gameResult, "Failed to create game");

            m_State.emplace<Game>(std::move(*gameResult));

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
    if(!GG_VERIFYB(SDL_Init(SDL_INIT_VIDEO)))
    {
        GG_ERROR("SDL_Init failed: {}", SDL_GetError());
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
