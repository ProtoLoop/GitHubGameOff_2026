#define SDL_MAIN_USE_CALLBACKS 1

#include "Input.h"

#include <memory>
#include <new>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace
{
constexpr int WindowWidth = 960;
constexpr int WindowHeight = 540;
constexpr float SquareSize = 64.0F;
constexpr Uint64 AnimationPeriodMs = 3000;

struct AppState
{
    AppState() = default;
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;
    AppState(AppState&&) = delete;
    AppState& operator=(AppState&&) = delete;

    ~AppState()
    {
        SDL_DestroyRenderer(Renderer);
        SDL_DestroyWindow(Window);
    }

    SDL_Window* Window = nullptr;
    SDL_Renderer* Renderer = nullptr;

    Input Input;
};

void
LogSdlError(const char* message)
{
    // SDL's logging API requires varargs. Both arguments are null-terminated strings.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    SDL_Log("%s: %s", message, SDL_GetError());
}

} // namespace

SDL_AppResult
SDL_AppInit(void** appstate, int, char**)
{
    *appstate = nullptr;
    if(!SDL_Init(SDL_INIT_VIDEO))
    {
        LogSdlError("SDL initialization failed");
        return SDL_APP_FAILURE;
    }

    auto state = std::unique_ptr<AppState>(new(std::nothrow) AppState);
    if(!state)
    {
        SDL_OutOfMemory();
        LogSdlError("Could not allocate application state");
        return SDL_APP_FAILURE;
    }
    if(!SDL_CreateWindowAndRenderer("gameoff2026",
           WindowWidth,
           WindowHeight,
           SDL_WINDOW_RESIZABLE,
           &state->Window,
           &state->Renderer)
        || !SDL_SetRenderLogicalPresentation(state->Renderer,
            WindowWidth,
            WindowHeight,
            SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        LogSdlError("Window/renderer setup failed");
        return SDL_APP_FAILURE;
    }

    // Vsync is a preference. The browser also supplies its own frame scheduling.
    if(!SDL_SetRenderVSync(state->Renderer, 1))
    {
        LogSdlError("Vsync unavailable");
    }
    *appstate = state.release(); // SDL returns this state to SDL_AppQuit for cleanup.
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
    const auto& state = *static_cast<AppState*>(appstate);
    const float phase = static_cast<float>(SDL_GetTicks() % AnimationPeriodMs)
        / static_cast<float>(AnimationPeriodMs);
    const float travel = 0.5F - (0.5F * SDL_cosf(phase * 2.0F * SDL_PI_F));
    const SDL_FRect square //
        {
            .x = travel * (WindowWidth - SquareSize),
            .y = (WindowHeight - SquareSize) * 0.5F,
            .w = SquareSize,
            .h = SquareSize,
        };

    if(!SDL_SetRenderDrawColorFloat(state.Renderer, 0.0F, 0.0F, 0.25F, 1.0F)
        || !SDL_RenderClear(state.Renderer)
        || !SDL_SetRenderDrawColorFloat(state.Renderer, 0.0F, 0.75F, 1.0F, 1.0F)
        || !SDL_RenderFillRect(state.Renderer, &square)
        || !SDL_RenderPresent(state.Renderer))
    {
        LogSdlError("Rendering failed");
        return SDL_APP_FAILURE;
    }

    float value{};
    if(state.Input.IsTriggered(Input::ActionId::Quit, value))
    {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void
SDL_AppQuit(void* appstate, SDL_AppResult)
{
    // Destroy the renderer and window before shutting down SDL, including on failure.
    const std::unique_ptr<AppState> state(static_cast<AppState*>(appstate));
    // SDL calls SDL_Quit after this callback returns.
}
