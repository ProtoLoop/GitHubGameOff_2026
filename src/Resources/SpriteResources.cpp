#include "SpriteResources.h"

namespace
{

#define SPRITE_PATH(name) "/assets/Sprites/Haunted-Cemetery-1-Free-24-Assets/" #name ".png"
#define SPRITE_PATH2(dir, name)                                                                    \
    "/assets/Sprites/Haunted-Cemetery-1-Free-24-Assets/" #dir "/" #name ".png"
#define SPRITE_NAME(name) SpriteResources::kSprite_##name
#define SPRITE_DEF(name)                                                                           \
    {                                                                                              \
        .Name = SPRITE_NAME(name),                                                                 \
        .TexturePath = SPRITE_PATH(name),                                                         \
    }
#define SPRITE_DEF2(dir, name)                                                                     \
    {                                                                                              \
        .Name = SPRITE_NAME(name),                                                                 \
        .TexturePath = SPRITE_PATH2(dir, name),                                                  \
    }

constexpr const SpriteDef kSpriteDefs[] //
    {
        SPRITE_DEF2(Cemetery_Bell, Cemetery_Bell_001),
        SPRITE_DEF2(Cemetery_Bell, Cemetery_Bell_002),
        SPRITE_DEF2(Cemetery_Bell, Cemetery_Bell_003),
        SPRITE_DEF2(Cemetery_Bell, Cemetery_Bell_004),

        SPRITE_DEF2(Cemetery_Stonewall_and_Boundary, Cemetery_Stonewall_and_Boundary_001),
        SPRITE_DEF2(Cemetery_Stonewall_and_Boundary, Cemetery_Stonewall_and_Boundary_002),
        SPRITE_DEF2(Cemetery_Stonewall_and_Boundary, Cemetery_Stonewall_and_Boundary_003),
        SPRITE_DEF2(Cemetery_Stonewall_and_Boundary, Cemetery_Stonewall_and_Boundary_004),

        {
            .Name = SPRITE_NAME(Forgotten_Headstone_001),
            .TexturePath = SPRITE_PATH2(Forgotten_Headstone, Forgotten_Headstone_001),
            .Scale = 0.25f,
        },
        {
            .Name = SPRITE_NAME(Forgotten_Headstone_002),
            .TexturePath = SPRITE_PATH2(Forgotten_Headstone, Forgotten_Headstone_002),
            .Scale = 0.25f,
        },
        {
            .Name = SPRITE_NAME(Forgotten_Headstone_003),
            .TexturePath = SPRITE_PATH2(Forgotten_Headstone, Forgotten_Headstone_003),
            .Scale = 0.25f,
        },
        {
            .Name = SPRITE_NAME(Forgotten_Headstone_004),
            .TexturePath = SPRITE_PATH2(Forgotten_Headstone, Forgotten_Headstone_004),
            .Scale = 0.25f,
        },

        // SPRITE_DEF2(Forgotten_Headstone, Forgotten_Headstone_001),
        // SPRITE_DEF2(Forgotten_Headstone, Forgotten_Headstone_002),
        // SPRITE_DEF2(Forgotten_Headstone, Forgotten_Headstone_003),
        // SPRITE_DEF2(Forgotten_Headstone, Forgotten_Headstone_004),

        SPRITE_DEF2(Gothic_Necropolis_Gate, Gothic_Necropolis_Gate_001),
        SPRITE_DEF2(Gothic_Necropolis_Gate, Gothic_Necropolis_Gate_002),
        SPRITE_DEF2(Gothic_Necropolis_Gate, Gothic_Necropolis_Gate_003),
        SPRITE_DEF2(Gothic_Necropolis_Gate, Gothic_Necropolis_Gate_004),

        SPRITE_DEF2(Noble_Family_Tombstone, Noble_Family_Tombstone_001),
        SPRITE_DEF2(Noble_Family_Tombstone, Noble_Family_Tombstone_002),
        SPRITE_DEF2(Noble_Family_Tombstone, Noble_Family_Tombstone_003),
        SPRITE_DEF2(Noble_Family_Tombstone, Noble_Family_Tombstone_004),

        SPRITE_DEF2(Cemetery_Ancient_Tree, Cemetery_Ancient_Tree_004),

    };

} // namespace

namespace SpriteResources
{
std::span<const SpriteDef>
GetSpriteDefs()
{
    return kSpriteDefs;
}
} // namespace SpriteResources

#undef SPRITE_PATH
#undef SPRITE_PATH2
#undef SPRITE_NAME
#undef SPRITE_DEF