#include "ResourceDefs.h"

#include <algorithm>

Result<const SpriteFrameSequenceDef&>
LevelDef::GetSpriteFrameSequenceDef(const ResourceName& name) const
{
    auto it = std::ranges::find(SpriteSeqDefs, name, &SpriteFrameSequenceDef::Name);

    GG_CHECKB(it != SpriteSeqDefs.end());
    return *it;
}

Result<const SpriteDef&>
LevelDef::GetSpriteDef(const ResourceName& name) const
{
    auto it = std::ranges::find(SpriteDefs, name, &SpriteDef::Name);

    GG_CHECKB(it != SpriteDefs.end());
    return *it;
}

Result<const PropTypeDef&>
LevelDef::GetPropTypeDef(const ResourceName& name) const
{
    auto it = std::ranges::find(PropTypeDefs, name, &PropTypeDef::Name);

    GG_CHECKB(it != PropTypeDefs.end());
    return *it;
}

Result<const ActorTypeDef&>
LevelDef::GetActorTypeDef(const ResourceName& name) const
{
    auto it = std::ranges::find(ActorTypeDefs, name, &ActorTypeDef::Name);

    GG_CHECKB(it != ActorTypeDefs.end());
    return *it;
}