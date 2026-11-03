#include "Input.h"

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
    };

constexpr Input::MouseButtonMapping const MouseButtonMappings[] //
    { {
        .Button = 0,
        .Action = Input::ActionId::None,
        .Trigger = Input::Trigger::None,
        .Value = 0,
    } };
} // namespace

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