#define SDL_MAIN_USE_CALLBACKS 1

#include "Angle.h"
#include "CellMap.h"
#include "Defer.h"
#include "Input.h"
#include "Level.h"
#include "LevelLoader.h"
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
constexpr const ResourceName kHeroIdleAnim = "HeroIdle";
constexpr const ResourceName kHeroRunAnim = "HeroRun";

const SpriteAnimDef kHeroIdleAnimDef //
    {
        .Name = kHeroIdleAnim,
        .Dim{ .w = 180, .h = 180 },
        .Textures //
        {
            "/assets/HeroAnims/Idle/Idle_Body_337.png",
            "/assets/HeroAnims/Idle/Idle_Body_000.png",
            "/assets/HeroAnims/Idle/Idle_Body_022.png",
            "/assets/HeroAnims/Idle/Idle_Body_045.png",
            "/assets/HeroAnims/Idle/Idle_Body_067.png",
            "/assets/HeroAnims/Idle/Idle_Body_090.png",
            "/assets/HeroAnims/Idle/Idle_Body_112.png",
            "/assets/HeroAnims/Idle/Idle_Body_135.png",
            "/assets/HeroAnims/Idle/Idle_Body_157.png",
            "/assets/HeroAnims/Idle/Idle_Body_180.png",
            "/assets/HeroAnims/Idle/Idle_Body_202.png",
            "/assets/HeroAnims/Idle/Idle_Body_225.png",
            "/assets/HeroAnims/Idle/Idle_Body_247.png",
            "/assets/HeroAnims/Idle/Idle_Body_270.png",
            "/assets/HeroAnims/Idle/Idle_Body_292.png",
            "/assets/HeroAnims/Idle/Idle_Body_315.png",
        },
    };

const SpriteAnimDef kHeroRunAnimDef //
    {
        .Name = kHeroRunAnim,
        .Dim{ .w = 180, .h = 180 },
        .Textures //
        {
            "/assets/HeroAnims/Run/Run_Body_000.png",
            "/assets/HeroAnims/Run/Run_Body_022.png",
            "/assets/HeroAnims/Run/Run_Body_045.png",
            "/assets/HeroAnims/Run/Run_Body_067.png",
            "/assets/HeroAnims/Run/Run_Body_090.png",
            "/assets/HeroAnims/Run/Run_Body_112.png",
            "/assets/HeroAnims/Run/Run_Body_135.png",
            "/assets/HeroAnims/Run/Run_Body_157.png",
            "/assets/HeroAnims/Run/Run_Body_180.png",
            "/assets/HeroAnims/Run/Run_Body_202.png",
            "/assets/HeroAnims/Run/Run_Body_225.png",
            "/assets/HeroAnims/Run/Run_Body_247.png",
            "/assets/HeroAnims/Run/Run_Body_270.png",
            "/assets/HeroAnims/Run/Run_Body_292.png",
            "/assets/HeroAnims/Run/Run_Body_315.png",
            "/assets/HeroAnims/Run/Run_Body_337.png",
        },
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
        Result<LevelLoader> LevelLoader;
        Level Level;

        Result<> Update()
        {
            GG_CHECK(LevelLoader->Update(), "Failed to update level loader");
            GG_CHECK(Level.Update(), "Failed to update level");

            return Result<>::Ok;
        }

        bool IsPending() const { return LevelLoader->IsPending() || Level.IsPending(); }
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

    float PlayerX{ 0 };
    float PlayerY{ 0 };

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

            auto levelLoaderResult = LevelLoader::Load("/assets/sample/level.json", FileFetcher);
            GG_CHECKB(levelLoaderResult, "Failed to start loading level");

            const SpriteAnimDef spriteAnimDefs[]//
            {
                kHeroIdleAnimDef,
                kHeroRunAnimDef
            };

            auto levelResult =
                Level::Load(spriteAnimDefs, kCellMapDef, FileFetcher, *renderer, WebGpu);
            GG_CHECKB(levelResult, "Failed to start loading level");

            m_State = LoadingState//
            {
                .Renderer = std::move(renderer),
                .LevelLoader = std::move(levelLoaderResult),
                .Level = std::move(*levelResult)
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
            auto& state = std::get<LoadingState>(m_State);

            m_State = RunState //
                {
                    .Renderer = std::move(state.Renderer),
                    .Level = std::move(state.Level)
                };

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
    static constexpr float kTurnsPerSec = 1;

    static float animPhase = 0;
    static constexpr float kAnimPhaseScale = 1.5;
    static uint64_t ms = SDL_GetTicks();

    auto& state = std::get<RunState>(m_State);

    const uint64_t curMs = SDL_GetTicks();
    const uint64_t deltaMs = curMs - ms;
    ms = curMs;
    const float deltaSec = static_cast<float>(deltaMs) / 1000.0f;
    animPhase += deltaSec * kAnimPhaseScale;

    animPhase = std::fmod(animPhase, 1.0f);

    float value{};
    if(Input.IsTriggered(Input::ActionId::RotateLeft, value))
    {
        const float dturns = (static_cast<float>(deltaMs) / 1000.0f) * kTurnsPerSec;
        PlayerRot -= Angle(dturns);
    }
    if(Input.IsTriggered(Input::ActionId::RotateRight, value))
    {
        const float dturns = (static_cast<float>(deltaMs) / 1000.0f) * kTurnsPerSec;
        PlayerRot += Angle(dturns);
    }

    auto spriteAnimResult = state.Level.GetSpriteAnim(kHeroIdleAnim);
    GG_CHECKB(spriteAnimResult, "Failed to get sprite anim: {}", kHeroIdleAnim);
    const SpriteAnim* spriteAnim = &spriteAnimResult.Value();

    if(Input.IsTriggered(Input::ActionId::MoveLeft, value)
        || Input.IsTriggered(Input::ActionId::MoveRight, value)
        || Input.IsTriggered(Input::ActionId::MoveForward, value)
        || Input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        spriteAnimResult = state.Level.GetSpriteAnim(kHeroRunAnim);
        GG_CHECKB(spriteAnimResult, "Failed to get sprite anim: {}", kHeroRunAnim);
        spriteAnim = &spriteAnimResult.Value();
    }

    // Snap heading to the nearest sprite direction.
    const float heading = spriteAnim->SnapAngle(PlayerRot).ToTurns();
    const float forwardX = std::sin(heading * 2.0f * std::numbers::pi_v<float>);
    const float forwardY = std::cos(heading * 2.0f * std::numbers::pi_v<float>);
    const float rightX = forwardY;
    const float rightY = -forwardX;

    constexpr float kMoveSpeed = 2.0f;

    float moveLR = 0.0f, moveFB = 0.0f;

    if(Input.IsTriggered(Input::ActionId::MoveLeft, value))
    {
        moveLR -= value * kMoveSpeed * deltaSec;
    }
    if(Input.IsTriggered(Input::ActionId::MoveRight, value))
    {
        moveLR += value * kMoveSpeed * deltaSec;
    }
    if(Input.IsTriggered(Input::ActionId::MoveForward, value))
    {
        moveFB += value * kMoveSpeed * deltaSec;
    }
    if(Input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        moveFB -= value * kMoveSpeed * deltaSec;
    }

    PlayerX += (moveLR * rightX) + (moveFB * forwardX);
    PlayerY += (moveLR * rightY) + (moveFB * forwardY);

    Input.Reset();

    // Offset to align the sprite with the isometric view.
    static const Angle kAngleOffset(0.125f);

    const Angle spriteAngle = PlayerRot + kAngleOffset;
    const auto spriteResult = spriteAnim->GetSprite(animPhase, spriteAngle);
    GG_CHECKB(spriteResult, "Failed to get sprite animation frame");

    const Sprite* sprite = *spriteResult;

    state.Renderer->SetView(PlayerX, PlayerY);

    auto cellMapResult = state.Level.GetCellMap();
    GG_CHECKB(cellMapResult, "Failed to get cell map");

    GG_CHECKB(state.Renderer->Begin(), "Failed to begin rendering");
    GG_CHECKB(state.Renderer->Render(*cellMapResult,*sprite), "Failed to render");
    GG_CHECKB(state.Renderer->End(), "Failed to end rendering");

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
