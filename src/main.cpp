#include <webgpu/webgpu_cpp.h>
#define SDL_MAIN_USE_CALLBACKS 1

#include "Angle.h"
#include "CellMap.h"
#include "Defer.h"
#include "Input.h"
#include "LevelLoader.h"
#include "Renderer.h"
#include "Sprite.h"
#include "TextureLoader.h"
#include "WebGpu.h"

#include <algorithm>
#include <cstdint>
#include <numbers>
#include <optional>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <variant>

namespace
{
constexpr int SquareSize = 64;

constexpr FilePath kHeroIdleAnimTextures[] //
    {
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
        "/assets/HeroAnims/Idle/Idle_Body_337.png",
    };

constexpr FilePath kHeroRunAnimTextures[] //
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
    };

constexpr Rect kDefaultSpriteDims{ .x = 0, .y = 0, .w = 180, .h = 180 };
constexpr uint32_t kSpriteCols = 6;
constexpr uint32_t kSpriteRows = 4;
constexpr uint32_t kStateCount = kSpriteCols * kSpriteRows;
constexpr uint32_t kDirectionCount = std::size(kHeroRunAnimTextures);

constexpr const FilePath kCellMapTextures[] //
    {                                       //"/assets/sample/assets/tiles/dirt_tiles_4x4_256.png",
        "/assets/sample/assets/tiles/coast_sand_05_diff_4k.png",
        "/assets/sample/assets/tiles/Ground053_4K-PNG_Color.png"
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
        Result<LevelLoader> LevelLoader;
        Result<TextureLoader> CellMapTextureLoader;
        Result<TextureLoader> HeroIdleAnimTextureLoader;
        Result<TextureLoader> HeroRunAnimTextureLoader;
    };

    struct RunState
    {
        std::unique_ptr<CellMap> CellMap;
        std::unique_ptr<Renderer> Renderer;
        std::unique_ptr<SpriteCollection> HeroIdleSprites;
        std::unique_ptr<SpriteCollection> HeroRunSprites;
        SpriteAnim HeroIdleSpriteAnim;
        SpriteAnim HeroRunSpriteAnim;
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

            state.LevelLoader = LevelLoader::Load("/assets/sample/level.json", FileFetcher);
            GG_CHECKB(state.LevelLoader, "Failed to start loading level");

            state.CellMapTextureLoader =
                TextureLoader::Load(kCellMapTextures, FileFetcher, WebGpu, "Test");
            GG_CHECKB(state.CellMapTextureLoader, "Failed to start texture load");

            state.HeroIdleAnimTextureLoader =
                TextureLoader::Load(kHeroIdleAnimTextures, FileFetcher, WebGpu, "Anim");
            GG_CHECKB(state.HeroIdleAnimTextureLoader, "Failed to start anim texture load");

            state.HeroRunAnimTextureLoader =
                TextureLoader::Load(kHeroRunAnimTextures, FileFetcher, WebGpu, "Anim");
            GG_CHECKB(state.HeroRunAnimTextureLoader, "Failed to start anim texture load");

            m_Stage = Stage::Loading;
        }
        break;
        case Stage::Loading:
        {
            auto& state = std::get<LoadingState>(m_State);

            GG_CHECK(state.LevelLoader->Update(), "Failed to update level loader");

            if(!state.LevelLoader->IsPending()
                && !state.CellMapTextureLoader->IsPending()
                && !state.HeroIdleAnimTextureLoader->IsPending()
                && !state.HeroRunAnimTextureLoader->IsPending())
            {
                m_Stage = Stage::Loaded;
            }
        }
        break;
        case Stage::Loaded:
        {
            auto& state = std::get<LoadingState>(m_State);

            auto cellmapTexResult = state.CellMapTextureLoader->Take();
            GG_CHECKB(cellmapTexResult, "Failed to load cellmap texture");

            auto heroIdleAnimTexResult = state.HeroIdleAnimTextureLoader->Take();
            GG_CHECKB(heroIdleAnimTexResult, "Failed to load anim texture");

            auto heroRunAnimTexResult = state.HeroRunAnimTextureLoader->Take();
            GG_CHECKB(heroRunAnimTexResult, "Failed to load anim texture");

            auto cellmapResult = CellMap::Create(*cellmapTexResult, kCellMapGridCount);
            GG_CHECK(cellmapResult, "Failed to create cell map");

            BoundedVector<LostSoul> lostSouls(8); // NOLINT
            for(size_t i = 0; i < lostSouls.capacity(); ++i)
            {
                GG_CHECKB(lostSouls.push_back(LostSoul{}), "Failed to add lost soul");
            }

            auto cellmap = std::move(*cellmapResult);

            auto rendererResult = Renderer::Create(kLogicalDisplayWidth,
                kLogicalDisplayHeight,
                WebGpu,
                *cellmap,
                std::move(lostSouls));
            GG_CHECKB(rendererResult, "Failed to create renderer");

            const size_t heroIdleSpriteCount = std::size(kHeroIdleAnimTextures) * kSpriteCols * kSpriteRows;
            std::unique_ptr<SpriteCollection> heroIdleSprites =
                std::make_unique<SpriteCollection>(heroIdleSpriteCount);

            for(uint32_t layer = 0; layer < std::size(kHeroIdleAnimTextures); ++layer)
            {
                for(uint32_t row = 0; row < kSpriteRows; ++row)
                {
                    for(uint32_t col = 0; col < kSpriteCols; ++col)
                    {
                        const Rect rect //
                            {
                                .x = col * kDefaultSpriteDims.w,
                                .y = row * kDefaultSpriteDims.h,
                                .w = kDefaultSpriteDims.w,
                                .h = kDefaultSpriteDims.h,
                            };

                        GG_CHECKB(heroIdleSprites->AddSprite(*heroIdleAnimTexResult, rect, layer),
                            "Failed to add sprite");
                    }
                }
            }

            const size_t heroRunSpriteCount = std::size(kHeroRunAnimTextures) * kSpriteCols * kSpriteRows;
            std::unique_ptr<SpriteCollection> heroRunSprites =
                std::make_unique<SpriteCollection>(heroRunSpriteCount);

            for(uint32_t layer = 0; layer < std::size(kHeroRunAnimTextures); ++layer)
            {
                for(uint32_t row = 0; row < kSpriteRows; ++row)
                {
                    for(uint32_t col = 0; col < kSpriteCols; ++col)
                    {
                        const Rect rect //
                            {
                                .x = col * kDefaultSpriteDims.w,
                                .y = row * kDefaultSpriteDims.h,
                                .w = kDefaultSpriteDims.w,
                                .h = kDefaultSpriteDims.h,
                            };

                        GG_CHECKB(heroRunSprites->AddSprite(*heroRunAnimTexResult, rect, layer),
                            "Failed to add sprite");
                    }
                }
            }

            auto heroIdleSpriteAnimResult = SpriteAnim::Create(*heroIdleSprites,
                SpriteAnim::Descriptor{ .StatesPerDirection = kStateCount,
                    .DirectionCount = kDirectionCount });
            GG_CHECKB(heroIdleSpriteAnimResult, "Failed to create sprite animation");

            auto heroRunSpriteAnimResult = SpriteAnim::Create(*heroRunSprites,
                SpriteAnim::Descriptor{ .StatesPerDirection = kStateCount,
                    .DirectionCount = kDirectionCount });
            GG_CHECKB(heroRunSpriteAnimResult, "Failed to create sprite animation");

            m_State = RunState //
                {
                    .CellMap = std::move(cellmap),
                    .Renderer = std::move(*rendererResult),
                    .HeroIdleSprites = std::move(heroIdleSprites),
                    .HeroRunSprites = std::move(heroRunSprites),
                    .HeroIdleSpriteAnim = std::move(*heroIdleSpriteAnimResult),
                    .HeroRunSpriteAnim = std::move(*heroRunSpriteAnimResult),
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

    return Result<>::Success();
}

Result<>
Game::Run()
{
    static constexpr float kTurnsPerSec = 1;

    constexpr uint64_t kAnimFps = 20;
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
        // ViewCol -= value * 0.1f; // NOLINT

        PlayerLoc.x -= static_cast<int>(value);
    }
    if(Input.IsTriggered(Input::ActionId::MoveRight, value))
    {
        // ViewCol += value * 0.1f; // NOLINT

        PlayerLoc.x += static_cast<int>(value);
    }
    if(Input.IsTriggered(Input::ActionId::MoveForward, value))
    {
        // ViewRow -= value * 0.1f; // NOLINT

        PlayerLoc.y -= static_cast<int>(value);
    }
    if(Input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        // ViewRow += value * 0.1f; // NOLINT

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

    PlayerLoc.x = std::min(std::max(PlayerLoc.x, halfSquareSize), kWindowWidth - halfSquareSize);
    PlayerLoc.y = std::min(std::max(PlayerLoc.y, halfSquareSize), kWindowHeight - halfSquareSize);

    //const SpriteAnim& spriteAnim = state.HeroRunSpriteAnim;
    const SpriteAnim& spriteAnim = state.HeroIdleSpriteAnim;

    const uint32_t dirIndex = PlayerRot.GetDirection(spriteAnim.GetDirectionCount());
    const uint32_t animFrame = frame % spriteAnim.GetStatesPerDirection();
    const auto frameResult = spriteAnim.GetFrame(animFrame, dirIndex);
    GG_CHECKB(frameResult, "Failed to get sprite animation frame");

    const Sprite* sprite = *frameResult;

    state.Renderer->SetView(ViewRow, ViewCol);

    GG_CHECKB(state.Renderer->BeginFrame(), "Failed to begin frame");
    GG_CHECKB(state.Renderer->Render(*sprite), "Failed to render");
    GG_CHECKB(state.Renderer->EndFrame(), "Failed to end frame");

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
