#include "Timer.h"

/// CountdownTimer

void
CountdownTimer::Start(const float duration)
{
    m_Duration = duration;
    m_Elapsed = 0;
}

float
CountdownTimer::GetDuration() const
{
    return m_Duration;
}

bool
CountdownTimer::IsExpired() const
{
    return m_Elapsed >= m_Duration;
}

float
CountdownTimer::GetRemaining() const
{
    return m_Duration - m_Elapsed;
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