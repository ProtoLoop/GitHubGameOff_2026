#pragma once

/// A countdown timer that counts down from a specified duration.
/// When it reaches zero it stops.
/// The timer must be ticked periodically to update its state.
class CountdownTimer
{
public:
    /// Starts the countdown timer.
    void Start(const float duration);

    /// Returns the initial duration of the timer.
    float GetDuration() const;

    /// Returns true if the countdown has finished.
    bool IsExpired() const;
    
    /// Returns the remaining time until the timer expires.
    float GetRemaining() const;

    /// Updates the timer by delta time.
    void Tick(float deltaTime);

private:
    float m_Duration{ 0 };
    float m_Elapsed{ 0 };
};