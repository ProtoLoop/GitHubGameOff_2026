#pragma once

#include <compare> // IWYU pragma: keep
#include <cstdint>

class EntityId
{
public:
    EntityId() = delete;

    static EntityId NextId()
    {
        static uint32_t s_NextId = 0;
        return EntityId(s_NextId++);
    }

    friend auto operator<=>(const EntityId& lhs, const EntityId& rhs) = default;

private:
    explicit EntityId(uint32_t id)
        : m_Id(id)
    {
    }
    uint32_t m_Id;
};