#include "Timer.h"

/// CountdownTimer

void
CountdownTimer::Start(const float duration)
{
    m_Duration = duration;
    m_Elapsed = 0;
}

bool
CountdownTimer::IsExpired() const
{
    return m_Elapsed >= m_Duration;
}

void
CountdownTimer::Tick(float deltaTime)
{
    if(IsExpired())
    {
        return;
    }
    m_Elapsed += deltaTime;
}