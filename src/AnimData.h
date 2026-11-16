#pragma once

#include "Sprite.h"

namespace AnimData
{
constexpr const ResourceName kCT_AnimName_Idle = "CT_Idle";
constexpr const ResourceName kCT_AnimName_Run = "CT_Run";
constexpr const ResourceName kCT_AnimName_Walk = "CT_Walk";
constexpr const ResourceName kSH_AnimName_Walk = "SH_Walk";

std::span<const SpriteAnimDef> GetAnimDefs();
}