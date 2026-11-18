#include "Sprite.h"

#include "Angle.h"
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

/// Sprite

Sprite::Sprite(wgpu::BindGroup bindGroup, const Rect& rect)
    : m_TextureBindGroup(std::move(bindGroup)),
      m_Rect(rect)
{
}

/// SpriteLoader

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

        Sprite sprite(defaultBindGroup, rect);
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
            rect = Rect{ .x = 0, .y = 0, .w = texDims.w, .h = texDims.h };
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
        NamedSprite newSprite(sprite.GetName(), Sprite(bindGroup, rect));

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

/// SpriteSequence

SpriteSequence::SpriteSequence(
    const ResourceName& name, wgpu::BindGroup bindGroup, const Dimension2D& dim, const Rect& rect)
    : m_Name(name),
      m_BindGroup(std::move(bindGroup)),
      m_Dim(dim),
      m_Rect(rect),
      m_SpriteCount((rect.w / dim.w) * (rect.h / dim.h))
{
}

Result<Sprite>
SpriteSequence::GetSprite(const uint32_t index) const
{
    GG_CHECKB(index < m_SpriteCount, "Index out of bounds");

    const uint32_t spritesPerRow = m_Rect.w / m_Dim.w;
    const uint32_t row = index / spritesPerRow;
    const uint32_t col = index % spritesPerRow;
    const Rect rect //
        {
            .x = m_Rect.x + (col * m_Dim.w),
            .y = m_Rect.y + (row * m_Dim.h),
            .w = m_Dim.w,
            .h = m_Dim.h,
        };

    return Sprite(m_BindGroup, rect);
}

/// SpriteAnimLoader

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

        SpriteSequence spriteSeq(def.Name, defaultBindGroup, def.Dim, rect);

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
        SpriteSequence newSeq(seq.GetName(), bindGroup, seq.GetDim(), rect);

        m_SpriteSequences[i] = std::move(newSeq);
    }

    return SpriteAnim(m_Name, std::move(m_SpriteSequences));
}

/// SpriteAnim

SpriteAnim::SpriteAnim(const ResourceName& name, BoundedVector<SpriteSequence>&& spriteSeqs)
    : m_Name(name),
      m_SpriteSequences(std::move(spriteSeqs))
{
}

const ResourceName&
SpriteAnim::GetName() const
{
    return m_Name;
}

Result<SpriteAnim::Instance>
SpriteAnim::CreateInstance() const
{
    return Instance(*this);
}

Sprite
SpriteAnim::GetFrame(const float phase, const Angle& angle) const
{
    const size_t dirCount = m_SpriteSequences.size();
    const float fdir = angle.ToTurns() * static_cast<float>(dirCount);
    const size_t direction = static_cast<size_t>(std::round(fdir)) % dirCount;

    const SpriteSequence& sequence = m_SpriteSequences[direction];

    const uint32_t framesPerDirection = sequence.GetSpriteCount();
    const float normalizedPhase = phase - std::floor(phase);
    const uint32_t frame =
        static_cast<uint32_t>(normalizedPhase * static_cast<float>(framesPerDirection))
        % framesPerDirection;

    return *sequence.GetSprite(frame);
}

/// SpriteAnim::Instance

SpriteAnim::Instance::Instance(const SpriteAnim& anim)
    : m_Anim(&anim)
{
}

const ResourceName&
SpriteAnim::Instance::GetName() const
{
    return m_Anim->GetName();
}

void
SpriteAnim::Instance::Tick(const float deltaTime)
{
    SetPhase(m_Phase + (deltaTime / m_Period));
}

void
SpriteAnim::Instance::SetPeriod(const float period)
{
    constexpr float minPeriod = 1e-6f; // Minimum allowed animation period
    if(GG_VERIFYB(period > minPeriod, "Animation period must be positive"))
    {
        m_Period = period;
    }
}

void
SpriteAnim::Instance::SetPhase(const float phase)
{
    m_Phase = phase - std::floor(phase);
}

void
SpriteAnim::Instance::SetAngle(const Angle& angle)
{
    m_Angle = angle;
}

const Angle&
SpriteAnim::Instance::GetAngle() const
{
    return m_Angle;
}

Sprite
SpriteAnim::Instance::GetFrame() const
{
    return m_Anim->GetFrame(m_Phase, m_Angle);
}