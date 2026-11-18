#pragma once

#include "Sprite.h"

namespace SpriteResources
{
constexpr ResourceName kSprite_Cemetery_Bell_001 = "Cemetery_Bell_001";
constexpr ResourceName kSprite_Cemetery_Bell_002 = "Cemetery_Bell_002";
constexpr ResourceName kSprite_Cemetery_Bell_003 = "Cemetery_Bell_003";
constexpr ResourceName kSprite_Cemetery_Bell_004 = "Cemetery_Bell_004";

constexpr ResourceName kSprite_Cemetery_Stonewall_and_Boundary_001 = "Cemetery_Stonewall_and_Boundary_001";
constexpr ResourceName kSprite_Cemetery_Stonewall_and_Boundary_002 = "Cemetery_Stonewall_and_Boundary_002";
constexpr ResourceName kSprite_Cemetery_Stonewall_and_Boundary_003 = "Cemetery_Stonewall_and_Boundary_003";
constexpr ResourceName kSprite_Cemetery_Stonewall_and_Boundary_004 = "Cemetery_Stonewall_and_Boundary_004";

constexpr ResourceName kSprite_Forgotten_Headstone_001 = "Forgotten_Headstone_001";
constexpr ResourceName kSprite_Forgotten_Headstone_002 = "Forgotten_Headstone_002";
constexpr ResourceName kSprite_Forgotten_Headstone_003 = "Forgotten_Headstone_003";
constexpr ResourceName kSprite_Forgotten_Headstone_004 = "Forgotten_Headstone_004";

constexpr ResourceName kSprite_Gothic_Necropolis_Gate_001 = "Gothic_Necropolis_Gate_001";
constexpr ResourceName kSprite_Gothic_Necropolis_Gate_002 = "Gothic_Necropolis_Gate_002";
constexpr ResourceName kSprite_Gothic_Necropolis_Gate_003 = "Gothic_Necropolis_Gate_003";
constexpr ResourceName kSprite_Gothic_Necropolis_Gate_004 = "Gothic_Necropolis_Gate_004";

constexpr ResourceName kSprite_Noble_Family_Tombstone_001 = "Noble_Family_Tombstone_001";
constexpr ResourceName kSprite_Noble_Family_Tombstone_002 = "Noble_Family_Tombstone_002";
constexpr ResourceName kSprite_Noble_Family_Tombstone_003 = "Noble_Family_Tombstone_003";
constexpr ResourceName kSprite_Noble_Family_Tombstone_004 = "Noble_Family_Tombstone_004";

constexpr ResourceName kSprite_Cemetery_Ancient_Tree_004 = "Cemetery_Ancient_Tree_004";
    
std::span<const SpriteDef> GetSpriteDefs();
} // namespace SpriteResources