#pragma once

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

class Input
{
public:
    enum class ActionId : Uint8
    {
        None,
        Quit,

        NumStates
    };

    enum class Trigger
    {
        None,
        Pressed,
        Released,
        Held,
    };

    struct ActionState
    {
        ActionId Action{ ActionId::None };
        bool Triggered{ false };
        float Value{ 1 };
    };

    struct KeyMapping
    {
        SDL_Scancode Key{ SDL_SCANCODE_UNKNOWN };
        ActionId Action{ ActionId::None };
        Trigger Trigger{ Trigger::None };
        float Value{ 1 };
    };

    struct MouseButtonMapping
    {
        Uint8 Button{ 0 };
        ActionId Action{ ActionId::None };
        Trigger Trigger{ Trigger::None };
        float Value{ 1 };
    };

    /// Triggers the action associated with the key and trigger type.
    void TriggerKey(SDL_Scancode key, Input::Trigger trigger);

    /// Triggers the action associated with the mouse button and trigger type.
    void TriggerMouseButton(Uint8 button, Input::Trigger trigger);

    /// Triggers the action with the value.
    void TriggerAction(const Input::ActionId action, const float value);

    /// Returns true if the action is triggered, and retrieves its value.
    bool IsTriggered(const Input::ActionId action, float& value) const;

    /// Resets all action states to their default values.
    void Reset();

private:

    Input::ActionState m_ActionStates[static_cast<Uint8>(Input::ActionId::NumStates)] //
        {
            { .Action = Input::ActionId::Quit },
        };
};