#include "Input.h"

#include <cstddef>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>

namespace
{
constexpr Input::KeyMapping const KeyMappings[] //
    {
        {
            .Key = SDL_SCANCODE_ESCAPE,
            .Action = Input::ActionId::Quit,
            .Trigger = Input::Trigger::Pressed,
            .Value = 1,
        },
        {
            .Key = SDL_SCANCODE_A,
            .Action = Input::ActionId::MoveLeft,
            .Trigger = Input::Trigger::Held,
            .Value = 1,
        },
        {
            .Key = SDL_SCANCODE_D,
            .Action = Input::ActionId::MoveRight,
            .Trigger = Input::Trigger::Held,
            .Value = 1,
        },
        {
            .Key = SDL_SCANCODE_W,
            .Action = Input::ActionId::MoveForward,
            .Trigger = Input::Trigger::Held,
            .Value = 1,
        },
        {
            .Key = SDL_SCANCODE_S,
            .Action = Input::ActionId::MoveBackward,
            .Trigger = Input::Trigger::Held,
            .Value = 1,
        },
        {
            .Key = SDL_SCANCODE_LEFT,
            .Action = Input::ActionId::RotateLeft,
            .Trigger = Input::Trigger::Held,
            .Value = 1,
        },
        {
            .Key = SDL_SCANCODE_RIGHT,
            .Action = Input::ActionId::RotateRight,
            .Trigger = Input::Trigger::Held,
            .Value = 1,
        },
    };

constexpr Input::MouseButtonMapping const MouseButtonMappings[] //
    { {
        .Button = 0,
        .Action = Input::ActionId::None,
        .Trigger = Input::Trigger::None,
        .Value = 0,
    } };
} // namespace

Input::Input()
{
    for(size_t i = 0; i < static_cast<size_t>(Input::ActionId::NumStates); ++i)
    {
        m_ActionStates[i].Action = static_cast<Input::ActionId>(i);
        m_ActionStates[i].Triggered = false;
        m_ActionStates[i].Value = 1;
    }
}

void
Input::Update()
{
    int numKeys{};
    const bool* keys = SDL_GetKeyboardState(&numKeys);

    for(int i = 0; i < numKeys; ++i)
    {
        //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if(keys[i])
        {
            TriggerKey(static_cast<SDL_Scancode>(i), Input::Trigger::Held);
        }
    }
}

void
Input::TriggerKey(SDL_Scancode key, Input::Trigger trigger)
{
    for(const KeyMapping& mapping : KeyMappings)
    {
        if(mapping.Key == key && mapping.Trigger == trigger)
        {
            TriggerAction(mapping.Action, mapping.Value);
        }
    }
}

void
Input::TriggerMouseButton(Uint8 button, Input::Trigger trigger)
{
    for(const MouseButtonMapping& mapping : MouseButtonMappings)
    {
        if(mapping.Button == button && mapping.Trigger == trigger)
        {
            TriggerAction(mapping.Action, mapping.Value);
        }
    }
}

void
Input::TriggerAction(const Input::ActionId action, const float value)
{
    for(auto& state : m_ActionStates)
    {
        if(state.Action == action)
        {
            state.Triggered = true;
            state.Value = value;
            break;
        }
    }
}

bool
Input::IsTriggered(const Input::ActionId action, float& value) const
{
    for(const auto& state : m_ActionStates)
    {
        if(state.Action == action)
        {
            value = state.Value;
            return state.Triggered;
        }
    }
    return false;
}

void
Input::Reset()
{
    for(auto& state : m_ActionStates)
    {
        state.Triggered = false;
        state.Value = 1;
    }
}