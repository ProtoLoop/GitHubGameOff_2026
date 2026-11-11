/// Occupancy grid dimensions, in cells.
export const kGridSize = 64;
export const kCellCount = kGridSize * kGridSize;
export const kJunctionGridSize = kGridSize + 1;

/// Cell and junction-mask dimensions, in world pixels.
export const kCellSize = 64;
export const kHalfCellSize = kCellSize / 2;
export const kMaskPixelCount = kCellSize * kCellSize;
export const kMaskAtlasGridSize = 4;
export const kMaskAtlasPixelSize = kMaskAtlasGridSize * kCellSize;
