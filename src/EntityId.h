#pragma once

#include <compare> // IWYU pragma: keep
#include <cstdint>
#include <format>

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
    friend std::formatter<EntityId>;

    explicit EntityId(uint32_t id)
        : m_Id(id)
    {
    }
    uint32_t m_Id;
};

/// Formatter specialization for EntityId to be used with std::format.
template<>
struct std::formatter<EntityId> : std::formatter<uint32_t>
{
    auto format(const EntityId& id, auto& context) const
    {
        return std::formatter<uint32_t>::format(id.m_Id, context);
    }
};