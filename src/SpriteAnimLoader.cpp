#include "SpriteAnimLoader.h"

#include "Renderer.h"
#include "WebGpu.h"

#include <cstdint>
#include <limits>
#include <webgpu/webgpu_cpp.h>

namespace
{
constexpr uint32_t kNullXYWH = std::numeric_limits<uint32_t>::max();
// Sentinel rect to use if no rect is specified in the sprite def.
// Will use the entire texture as the sprite rect.
constexpr Rect kNullRect{ .x = kNullXYWH, .y = kNullXYWH, .w = kNullXYWH, .h = kNullXYWH };

}

SpriteAnimLoader::SpriteAnimLoader(const ResourceName& name,
    BoundedVector<TextureLoader>&& textureLoaders,
    BoundedVector<size_t>&& sequenceToTextureMap,
    BoundedVector<SpriteSequence>&& spriteSequences,
    Renderer& renderer)
    : m_Name(name),
      m_TextureLoaders(std::move(textureLoaders)),
      m_SequenceToTextureMap(std::move(sequenceToTextureMap)),
      m_SpriteSequences(std::move(spriteSequences)),
      m_Renderer(&renderer)
{
}

Result<SpriteAnimLoader>
SpriteAnimLoader::Load(
    const SpriteAnimDef& animDef, FileFetcher& fileFetcher, Renderer& renderer, WebGpu& webGpu)
{
    GG_CHECKB(!animDef.SequenceDefs.empty(),
        "Animation sequences must not be empty: {}",
        animDef.Name);

    const wgpu::Texture defaultTexture = webGpu.GetDefaultTexture();
    GG_TRYB(defaultBindGroup, renderer.CreateSpriteTextureBindGroup(defaultTexture));

    BoundedVector<FilePath> uniqueTexPaths(animDef.SequenceDefs.size());
    BoundedVector<size_t> sequenceToTextureMap(animDef.SequenceDefs.size());
    BoundedVector<SpriteSequence> spriteSequences(animDef.SequenceDefs.size());

    // Collect unique texture paths and create sprite sequences
    // using the default texture, which will be replaced later when
    // texture loading completes.
    for(const SpriteSequenceDef& def : animDef.SequenceDefs)
    {
        GG_CHECKB(def.Dim.w > 0 && def.Dim.h > 0,
            "Invalid animation dimensions: {} : {} -  {}x{}",
            animDef.Name,
            def.Name,
            def.Dim.w,
            def.Dim.h);

        Rect rect = kNullRect;
        if(def.Rect)
        {
            rect = *def.Rect;
        }

        GG_CHECKB(!def.TexturePath.empty(),
            "Animation texture path must not be empty: {} : {}",
            animDef.Name,
            def.Name);

        // In cases where multiple sequences share the same texture,
        // we avoid loading the same texture multiple times by maintaining
        // a list of unique texture paths and mapping sequences to them.
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

        SpriteSequence spriteSeq(def.Name, defaultBindGroup, def.Dim, rect, def.Color);

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

    return SpriteAnimLoader(animDef.Name,
        std::move(textureLoaders),
        std::move(sequenceToTextureMap),
        std::move(spriteSequences),
        renderer);
}

const ResourceName&
SpriteAnimLoader::GetName() const
{
    return m_Name;
}

bool
SpriteAnimLoader::IsPending() const
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

Result<>
SpriteAnimLoader::Update()
{
    for(auto& texLoader : m_TextureLoaders)
    {
        GG_CHECKB(texLoader.Update(), "Failed to update texture loader");
    }
    return Result<>::Ok;
}

Result<SpriteAnim>
SpriteAnimLoader::Take()
{
    GG_CHECKB(!IsPending(), "Sprite animation loader is still pending");
    GG_CHECKB(!m_Taken, "Sprite animation has already been taken");

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

    // For each sequence replace its bind group with a bindgroup containing the loaded texture.
    for(size_t i = 0; i < m_SpriteSequences.size(); ++i)
    {
        const SpriteSequence& seq = m_SpriteSequences[i];
        const size_t texIndex = m_SequenceToTextureMap[i];
        const wgpu::BindGroup& bindGroup = bindGroups[texIndex];
        const wgpu::Texture& texture = textures[texIndex];

        Rect rect = seq.GetRect();
        const Dimension2D texDims{ .w = texture.GetWidth(), .h = texture.GetHeight() };

        // If no rect was specified, use the entire texture.
        if(kNullRect == rect)
        {
            rect = Rect{ .x = 0, .y = 0, .w = texDims.w, .h = texDims.h };
        }

        GG_CHECKB(seq.GetDim().w <= rect.w && seq.GetDim().h <= rect.h,
            "Sequence dimensions exceed rect dimensions: {} : {} : {}x{} > {}x{}",
            m_Name,
            seq.GetName(),
            seq.GetDim().w,
            seq.GetDim().h,
            rect.w,
            rect.h);
                
        GG_CHECKB(rect.w <= texDims.w
                && rect.h <= texDims.h
                && rect.x < texDims.w
                && rect.y < texDims.h,
            "Sprite sequence rect exceeds texture bounds: {} : {} - {}x{}x{}x{} > {}x{}",
            m_Name,
            seq.GetName(),
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            texDims.w,
            texDims.h);

        GG_CHECKB(texDims.w - rect.x >= rect.w && texDims.h - rect.y >= rect.h,
            "Sprite sequence rect exceeds texture bounds: {}: {} - {}x{}x{}x{} > {}x{}",
            m_Name,
            seq.GetName(),
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            texDims.w - rect.x,
            texDims.h - rect.y);

        // Create a new sequence and replace the old one.
        SpriteSequence newSeq(seq.GetName(), bindGroup, seq.GetDim(), rect, seq.GetColor());

        m_SpriteSequences[i] = std::move(newSeq);
    }

    return SpriteAnim(m_Name, std::move(m_SpriteSequences));
}