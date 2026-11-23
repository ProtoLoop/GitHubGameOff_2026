#include "SpriteSequenceLoader.h"

#include "Geometry2D.h"
#include "Renderer.h"
#include "WebGpu.h"

#include <cstdint>
#include <limits>

namespace
{
constexpr uint32_t kNullXYWH = std::numeric_limits<uint32_t>::max();
// Sentinel rect to use if no rect is specified.
// If no rect is specified will use the entire texture as the rect.
constexpr Rect kNullRect{ kNullXYWH, kNullXYWH, kNullXYWH, kNullXYWH };

// Sentinel size to use if no size is specified.
// If no size is specified will use the entire source rectangle size as the size.
constexpr Dimension2D kNullSize{ .w = kNullXYWH, .h = kNullXYWH };

} // namespace

SpriteSequenceLoader::SpriteSequenceLoader(BoundedVector<TextureLoader>&& textureLoaders,
    BoundedVector<size_t>&& sequenceToTextureMap,
    BoundedVector<SpriteFrameSequence>&& spriteSequences,
    Renderer& renderer)
    : m_TextureLoaders(std::move(textureLoaders)),
      m_SequenceToTextureMap(std::move(sequenceToTextureMap)),
      m_SpriteSequences(std::move(spriteSequences)),
      m_Renderer(&renderer)
{
}

Result<SpriteSequenceLoader>
SpriteSequenceLoader::Load(const std::span<const SpriteFrameSequenceDef> spriteSeqDefs,
    FileFetcher& fileFetcher,
    Renderer& renderer,
    WebGpu& webGpu)
{
    if(spriteSeqDefs.empty())
    {
        GG_WARN("Sprite sequence definitions are empty");

        return SpriteSequenceLoader(BoundedVector<TextureLoader>(0),
            BoundedVector<size_t>(0),
            BoundedVector<SpriteFrameSequence>(0),
            renderer);
    }

    const wgpu::Texture defaultTexture = webGpu.GetDefaultTexture();
    GG_TRYB(defaultBindGroup, renderer.CreateSpriteTextureBindGroup(defaultTexture));

    BoundedVector<FilePath> uniqueTexPaths(spriteSeqDefs.size());
    BoundedVector<size_t> sequenceToTextureMap(spriteSeqDefs.size());
    BoundedVector<SpriteFrameSequence> spriteSequences(spriteSeqDefs.size());

    // Collect unique texture paths and create sprites
    // using the default texture, which will be replaced later when
    // texture loading completes.
    for(const SpriteFrameSequenceDef& def : spriteSeqDefs)
    {
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
            sequenceToTextureMap.push_back(uniqueTexPaths.size());
            uniqueTexPaths.push_back(def.TexturePath);
        }
        else
        {
            const size_t index = static_cast<size_t>(std::distance(uniqueTexPaths.begin(), it));
            sequenceToTextureMap.push_back(index);
        }

        InlineVector<Rectf, 1> colRects;
        if(def.ColRect)
        {
            colRects.push_back(*def.ColRect);
        }

        const Dimension2D size = def.Size ? *def.Size : kNullSize;
        const Rect srcRect = def.SrcRect ? *def.SrcRect : kNullRect;

        GG_CHECKB(size.w> 0 && size.h > 0,
            "Sprite size must be greater than zero: {} : {}",
            def.Name,
            def.TexturePath);

        // If src rect is not specified it will be set to the
        // the size of the texture when the texture is loaded.
        SpriteFrameSequence spriteSeq(def.Name,
            defaultBindGroup,
            size,
            srcRect,
            colRects,
            def.Pivot,
            def.Tint,
            def.Scale);

        spriteSequences.push_back(std::move(spriteSeq));
    }

    // Load the textures.
    BoundedVector<TextureLoader> textureLoaders(uniqueTexPaths.size());
    for(const FilePath& texPath : uniqueTexPaths)
    {
        auto texLoadResult = TextureLoader::Load(texPath, fileFetcher, webGpu, "Anim");
        GG_CHECKB(texLoadResult, "Failed to start texture load: {}", texPath);
        textureLoaders.push_back(std::move(*texLoadResult));
    }

    return SpriteSequenceLoader(std::move(textureLoaders),
        std::move(sequenceToTextureMap),
        std::move(spriteSequences),
        renderer);
}

bool
SpriteSequenceLoader::IsPending() const
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

Result<BoundedVector<SpriteFrameSequence>> 
SpriteSequenceLoader::Take()
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
    for(size_t i = 0; i < m_SpriteSequences.size(); ++i)
    {
        const SpriteFrameSequence& seq = m_SpriteSequences[i];
        const size_t texIndex = m_SequenceToTextureMap[i];
        const wgpu::BindGroup& bindGroup = bindGroups[texIndex];
        const wgpu::Texture& texture = textures[texIndex];

        Rect rect = seq.GetSrcRect();
        Dimension2D spriteSize = seq.m_Size;
        const Dimension2D texSize{ .w = texture.GetWidth(), .h = texture.GetHeight() };
        InlineVector<Rectf, 1> colRects;
        
        for(const Rectf& colRect : seq.GetCollisionRects())
        {
            colRects.push_back(colRect);
        }

        // If no rect was specified, use the entire texture.
        if(kNullRect == rect)
        {
            rect = Rect{ 0, 0, texSize.w, texSize.h };
        }

        if(kNullSize == spriteSize)
        {
            spriteSize.w = rect.w;
            spriteSize.h = rect.h;
        }

        GG_CHECKB(spriteSize.w <= rect.w && spriteSize.h <= rect.h,
            "Sequence dimensions exceed rect dimensions: {} : {}x{} > {}x{}",
            seq.GetName(),
            spriteSize.w,
            spriteSize.h,
            rect.w,
            rect.h);

        GG_CHECKB(rect.w > 0 && rect.h > 0,
            "Sprite rect has zero dimensions: {} - {}x{}x{}x{}",
            seq.GetName(),
            rect.x,
            rect.y,
            rect.w,
            rect.h);

        GG_CHECKB(
            rect.w <= texSize.w && rect.h <= texSize.h && rect.x < texSize.w && rect.y < texSize.h,
            "Sprite rect exceeds texture bounds: {} - {}x{}x{}x{} > {}x{}",
            seq.GetName(),
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            texSize.w,
            texSize.h);

        GG_CHECKB(texSize.w - rect.x >= rect.w && texSize.h - rect.y >= rect.h,
            "Sprite rect exceeds texture bounds: {} - {}x{}x{}x{} > {}x{}",
            seq.GetName(),
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            texSize.w - rect.x,
            texSize.h - rect.y);

        // Create a new sequence and replace the old one.
        SpriteFrameSequence newSeq(seq.GetName(),
            bindGroup,
            spriteSize,
            rect,
            seq.m_ColRects,
            seq.m_Pivot,
            seq.GetTint(),
            seq.GetScale());

        m_SpriteSequences[i] = std::move(newSeq);
    }

    auto bye = std::move(m_SpriteSequences);

    return std::move(bye);
}

Result<>
SpriteSequenceLoader::Update()
{
    for(auto& texLoader : m_TextureLoaders)
    {
        GG_CHECKB(texLoader.Update(), "Failed to update texture loader");
    }
    return Result<>::Ok;
}
