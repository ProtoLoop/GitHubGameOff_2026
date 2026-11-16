#include "Sprite.h"

#include "Angle.h"
#include "Renderer.h"
#include "WebGpu.h"

#include <webgpu/webgpu_cpp.h>

/// Sprite

Sprite::Sprite(wgpu::BindGroup bindGroup, const Rect& rect)
    : m_TextureBindGroup(std::move(bindGroup)),
      m_Rect(rect)
{
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
    Renderer* renderer)
    : m_Name(name),
      m_TextureLoaders(std::move(textureLoaders)),
      m_SequenceToTextureMap(std::move(sequenceToTextureMap)),
      m_SpriteSequences(std::move(spriteSequences)),
      m_Renderer(renderer)
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

    std::vector<FilePath> uniqueTexPaths;
    BoundedVector<size_t> sequenceToTextureMap(animDef.SequenceDefs.size());
    BoundedVector<SpriteSequence> spriteSequences(animDef.SequenceDefs.size());
    uniqueTexPaths.reserve(animDef.SequenceDefs.size());

    // Collect unique texture paths and create sprite sequences
    // using the default texture, which will be replaced later when
    // texture loading completes.
    for(const SpriteSequenceDef& seqDef : animDef.SequenceDefs)
    {
        GG_CHECKB(seqDef.Dim.w > 0 && seqDef.Dim.h > 0,
            "Invalid animation dimensions: {} : {} -  {}x{}",
            animDef.Name,
            seqDef.Name,
            seqDef.Dim.w,
            seqDef.Dim.h);

        GG_CHECKB(seqDef.Dim.w <= seqDef.Rect.w && seqDef.Dim.h <= seqDef.Rect.h,
            "Sequence dimensions exceed rect dimensions: {} : {} : {}x{} > {}x{}",
            animDef.Name,
            seqDef.Name,
            seqDef.Dim.w,
            seqDef.Dim.h,
            seqDef.Rect.w,
            seqDef.Rect.h);

        GG_CHECKB(!seqDef.TexturePath.empty(),
            "Animation texture path must not be empty: {} : {}",
            animDef.Name,
            seqDef.Name);

        // In cases where multiple sequences share the same texture,
        // we avoid loading the same texture multiple times by maintaining
        // a list of unique texture paths and mapping sequences to them.
        auto it = std::ranges::find(uniqueTexPaths, seqDef.TexturePath);
        if(uniqueTexPaths.end() == it)
        {
            sequenceToTextureMap.push_back(uniqueTexPaths.size());
            uniqueTexPaths.push_back(seqDef.TexturePath);
        }
        else
        {
            const size_t index = static_cast<size_t>(std::distance(uniqueTexPaths.begin(), it));
            sequenceToTextureMap.push_back(index);
        }

        SpriteSequence spriteSeq(seqDef.Name, defaultBindGroup, seqDef.Dim, seqDef.Rect);

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
        &renderer);
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

    std::vector<wgpu::BindGroup> bindGroups;
    std::vector<wgpu::Texture> textures;
    bindGroups.reserve(m_TextureLoaders.size());
    textures.reserve(m_TextureLoaders.size());

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

        const Rect& seqRect = seq.GetRect();
        const Dimension2D texDims{ .w = texture.GetWidth(), .h = texture.GetHeight() };

        GG_CHECKB(seqRect.w <= texDims.w
                && seqRect.h <= texDims.h
                && seqRect.x < texDims.w
                && seqRect.y < texDims.h,
            "Sprite sequence rect exceeds texture bounds: {} - {}x{}x{}x{} > {}x{}",
            seq.GetName(),
            seqRect.x,
            seqRect.y,
            seqRect.w,
            seqRect.h,
            texDims.w,
            texDims.h);

        GG_CHECKB(texDims.w - seqRect.x >= seqRect.w && texDims.h - seqRect.y >= seqRect.h,
            "Sprite sequence rect exceeds texture bounds: {} - {}x{}x{}x{} > {}x{}",
            seq.GetName(),
            seqRect.x,
            seqRect.y,
            seqRect.w,
            seqRect.h,
            texDims.w - seqRect.x,
            texDims.h - seqRect.y);

        // Create a new sequence and replace the old one.
        SpriteSequence newSeq(seq.GetName(), bindGroup, seq.GetDim(), seq.GetRect());

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