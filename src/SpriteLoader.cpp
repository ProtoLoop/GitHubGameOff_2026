#include "SpriteLoader.h"

#include "Renderer.h"
#include "WebGpu.h"

#include <cstdint>
#include <limits>

namespace
{
constexpr uint32_t kNullXYWH = std::numeric_limits<uint32_t>::max();
// Sentinel rect to use if no rect is specified in the sprite def.
// Will use the entire texture as the sprite rect.
constexpr Rect kNullRect{ kNullXYWH, kNullXYWH, kNullXYWH, kNullXYWH };

}

SpriteLoader::SpriteLoader(BoundedVector<TextureLoader>&& textureLoaders,
    BoundedVector<size_t>&& spriteToTextureMap,
    BoundedVector<NamedSprite>&& sprites,
    Renderer& renderer)
    : m_TextureLoaders(std::move(textureLoaders)),
      m_SpriteToTextureMap(std::move(spriteToTextureMap)),
      m_Sprites(std::move(sprites)),
      m_Renderer(&renderer)
{
}

Result<SpriteLoader>
SpriteLoader::Load(const std::span<const SpriteDef> spriteDefs,
    FileFetcher& fileFetcher,
    Renderer& renderer,
    WebGpu& webGpu)
{
    if(spriteDefs.empty())
    {
        GG_WARN("Sprite definitions are empty");

        return SpriteLoader(BoundedVector<TextureLoader>(0),
            BoundedVector<size_t>(0),
            BoundedVector<NamedSprite>(0),
            renderer);
    }

    GG_CHECKB(!spriteDefs.empty(), "Sprite definitions must not be empty");

    const wgpu::Texture defaultTexture = webGpu.GetDefaultTexture();
    GG_TRYB(defaultBindGroup, renderer.CreateSpriteTextureBindGroup(defaultTexture));

    BoundedVector<FilePath> uniqueTexPaths(spriteDefs.size());
    BoundedVector<size_t> spriteToTextureMap(spriteDefs.size());
    BoundedVector<NamedSprite> sprites(spriteDefs.size());

    // Collect unique texture paths and create sprites
    // using the default texture, which will be replaced later when
    // texture loading completes.
    for(const SpriteDef& def : spriteDefs)
    {
        GG_CHECKB(def.Pivot.x >= 0.0f && def.Pivot.x <= 1.0f && def.Pivot.y >= 0.0f && def.Pivot.y <= 1.0f,
            "Invalid pivot coordinates: {} : {} - ({}, {})",
            def.Name,
            def.TexturePath,
            def.Pivot.x,
            def.Pivot.y);

        Rect rect = kNullRect;
        if(def.Rect)
        {
            GG_CHECKB(def.Rect->w > 0 && def.Rect->h > 0,
                "Invalid sprite dimensions: {} : {} -  {}x{}",
                def.Name,
                def.TexturePath,
                def.Rect->w,
                def.Rect->h);

            rect = *def.Rect;
        }

        GG_CHECKB(!def.TexturePath.empty(),
            "Sprite texture path must not be empty: {} : {}",
            def.Name,
            def.TexturePath);

        // In cases where multiple sprites share the same texture,
        // we avoid loading the same texture multiple times by maintaining
        // a list of unique texture paths and mapping sprites to them.
        auto it = std::ranges::find(uniqueTexPaths, def.TexturePath);
        if(uniqueTexPaths.end() == it)
        {
            spriteToTextureMap.push_back(uniqueTexPaths.size());
            uniqueTexPaths.push_back(def.TexturePath);
        }
        else
        {
            const size_t index = static_cast<size_t>(std::distance(uniqueTexPaths.begin(), it));
            spriteToTextureMap.push_back(index);
        }

        Sprite sprite(defaultBindGroup, rect, def.Pivot, def.Color, def.Scale);
        NamedSprite namedSprite(def.Name, std::move(sprite));

        sprites.push_back(std::move(namedSprite));
    }

    // Load the textures.
    BoundedVector<TextureLoader> textureLoaders(uniqueTexPaths.size());
    for(const FilePath& texPath : uniqueTexPaths)
    {
        auto texLoadResult = TextureLoader::Load(texPath, fileFetcher, webGpu, "Anim");
        GG_CHECKB(texLoadResult, "Failed to start texture load: {}", texPath);
        textureLoaders.push_back(std::move(*texLoadResult));
    }

    return SpriteLoader(std::move(textureLoaders),
        std::move(spriteToTextureMap),
        std::move(sprites),
        renderer);
}

bool
SpriteLoader::IsPending() const
{
    for(const auto& texLoader : m_TextureLoaders)
    {
        if(texLoader.IsPending())
        {
            return true;
        }
    }
    return false;
}

Result<BoundedVector<NamedSprite>>
SpriteLoader::Take()
{
    GG_CHECKB(!IsPending(), "Sprite loader is still pending");
    GG_CHECKB(!m_Taken, "Sprites have already been taken");

    m_Taken = true;

    BoundedVector<wgpu::BindGroup> bindGroups(m_TextureLoaders.size());
    BoundedVector<wgpu::Texture> textures(m_TextureLoaders.size());

    // Create the bind groups.
    for(TextureLoader& texLoader : m_TextureLoaders)
    {
        GG_TRYB(tex, texLoader.Take(), "Failed to load texture");
        GG_TRYB(bindGroup,
            m_Renderer->CreateSpriteTextureBindGroup(tex),
            "Failed to get bind group from texture");

        bindGroups.push_back(std::move(bindGroup));
        textures.push_back(std::move(tex));
    }

    // For each sprite replace its bind group with a bindgroup containing the loaded texture.
    for(size_t i = 0; i < m_Sprites.size(); ++i)
    {
        const NamedSprite& sprite = m_Sprites[i];
        const size_t texIndex = m_SpriteToTextureMap[i];
        const wgpu::BindGroup& bindGroup = bindGroups[texIndex];
        const wgpu::Texture& texture = textures[texIndex];

        Rect rect = sprite.GetRect();
        const Dimension2D texDims{ .w = texture.GetWidth(), .h = texture.GetHeight() };

        // If no rect was specified, use the entire texture.
        if(kNullRect == rect)
        {
            rect = Rect{ 0, 0, texDims.w, texDims.h };
        }

        GG_CHECKB(
            rect.w <= texDims.w && rect.h <= texDims.h && rect.x < texDims.w && rect.y < texDims.h,
            "Sprite rect exceeds texture bounds: {} - {}x{}x{}x{} > {}x{}",
            sprite.GetName(),
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            texDims.w,
            texDims.h);

        GG_CHECKB(texDims.w - rect.x >= rect.w && texDims.h - rect.y >= rect.h,
            "Sprite rect exceeds texture bounds: {} - {}x{}x{}x{} > {}x{}",
            sprite.GetName(),
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            texDims.w - rect.x,
            texDims.h - rect.y);

        // Create a new sprite and replace the old one.
        NamedSprite newSprite(sprite.GetName(),
            Sprite(bindGroup, rect, sprite.GetPivot(), sprite.GetColor(), sprite.GetScale()));

        m_Sprites[i] = std::move(newSprite);
    }

    auto bye = std::move(m_Sprites);

    return std::move(bye);
}

Result<>
SpriteLoader::Update()
{
    for(auto& texLoader : m_TextureLoaders)
    {
        GG_CHECKB(texLoader.Update(), "Failed to update texture loader");
    }
    return Result<>::Ok;
}
