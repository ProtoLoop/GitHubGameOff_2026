#include "SpriteResources.h"

namespace
{

constexpr const SpriteDef kSpriteDefs[] //
    {
        {
            .Name = SpriteResources::kSprite_Forgotten_Headstone_004,
            .TexturePath = "/assets/sprites/Forgotten_Headstone_004.png",
        },
        {
            .Name = SpriteResources::kCemetery_Ancient_Tree_004,
            .TexturePath = "/assets/sprites/Cemetery_Ancient_Tree_004.png",
        },
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