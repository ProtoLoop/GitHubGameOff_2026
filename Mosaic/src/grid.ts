/// Square map dimensions, in cells. New projects retain the default size.
export const kDefaultGridSize = 64;
export const kMinGridSize = 2;
export const kMaxGridSize = 512;

export function validGridSize(size: number): boolean
{
    return Number.isInteger(size) && size >= kMinGridSize && size <= kMaxGridSize && (size & (size - 1)) === 0;
}

/// Cell and junction-mask dimensions, in world pixels.
export const kCellSize = 64;
export const kHalfCellSize = kCellSize / 2;
export const kMaskPixelCount = kCellSize * kCellSize;
export const kMaskAtlasGridSize = 4;
export const kMaskAtlasPixelSize = kMaskAtlasGridSize * kCellSize;
