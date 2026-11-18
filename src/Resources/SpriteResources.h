#pragma once

#include "Sprite.h"

namespace SpriteResources
{
constexpr ResourceName kSprite_Forgotten_Headstone_004 = "Forgotten_Headstone_004";
constexpr ResourceName kCemetery_Ancient_Tree_004 = "Cemetery_Ancient_Tree_004";
    
std::span<const SpriteDef> GetSpriteDefs();
} // namespace SpriteResources