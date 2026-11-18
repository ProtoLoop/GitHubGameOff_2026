#pragma once

#include "Sprite.h"

namespace AnimResources
{
constexpr const ResourceName kCT_AnimName_Idle = "CT_Idle";
constexpr const ResourceName kCT_AnimName_Run = "CT_Run";
constexpr const ResourceName kCT_AnimName_Walk = "CT_Walk";

constexpr const ResourceName kLS_AnimName_Idle = "CT_Idle";
constexpr const ResourceName kLS_AnimName_Run = "CT_Run";
constexpr const ResourceName kLS_AnimName_Walk = "SH_Hover";

constexpr const ResourceName kSH_AnimName_Walk = "SH_Walk";
constexpr const ResourceName kSH_AnimName_Hover = "SH_Hover";

constexpr const ResourceName kWarrior_AnimName_Idle = "Warrior_Idle";
constexpr const ResourceName kWarrior_AnimName_Run = "Warrior_Run";

std::span<const SpriteAnimDef> GetAnimDefs();
} // namespace AnimResources