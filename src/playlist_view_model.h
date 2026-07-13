#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>

namespace refrain {

struct VisibleRows {
    std::size_t first{};
    std::size_t end{};
};

[[nodiscard]] constexpr std::size_t visibleRowCapacity(float viewportHeight, float rowHeight) noexcept {
    if (viewportHeight <= 0.0F || rowHeight <= 0.0F) return 0;
    return static_cast<std::size_t>(viewportHeight / rowHeight) + 1;
}

[[nodiscard]] constexpr std::size_t maximumTopRow(
    std::size_t itemCount, std::size_t visibleCapacity) noexcept {
    return itemCount > visibleCapacity ? itemCount - visibleCapacity : 0;
}

[[nodiscard]] constexpr std::size_t clampTopRow(
    std::size_t topRow, std::size_t itemCount, std::size_t visibleCapacity) noexcept {
    return std::min(topRow, maximumTopRow(itemCount, visibleCapacity));
}

[[nodiscard]] constexpr VisibleRows visibleRows(std::size_t topRow, std::size_t itemCount,
    float viewportHeight, float rowHeight, std::size_t bufferRows = 1) noexcept {
    const auto capacity = visibleRowCapacity(viewportHeight, rowHeight);
    const auto first = topRow > bufferRows ? topRow - bufferRows : 0;
    return {first, std::min(itemCount, topRow + capacity + bufferRows)};
}

[[nodiscard]] constexpr std::size_t ensureRowVisible(std::size_t topRow, std::size_t row,
    std::size_t itemCount, std::size_t visibleCapacity) noexcept {
    if (itemCount == 0 || visibleCapacity == 0) return 0;
    row = std::min(row, itemCount - 1);
    if (row < topRow) return row;
    if (row >= topRow + visibleCapacity) return row - visibleCapacity + 1;
    return clampTopRow(topRow, itemCount, visibleCapacity);
}

[[nodiscard]] constexpr std::pair<std::size_t, std::size_t> inclusiveRange(
    std::size_t anchor, std::size_t target) noexcept {
    return anchor <= target ? std::pair{anchor, target} : std::pair{target, anchor};
}

[[nodiscard]] constexpr float scrollbarThumbHeight(
    float trackHeight, std::size_t itemCount, std::size_t visibleCapacity) noexcept {
    if (trackHeight <= 0.0F || itemCount == 0 || visibleCapacity >= itemCount) return trackHeight;
    return std::clamp(trackHeight * static_cast<float>(visibleCapacity) / static_cast<float>(itemCount),
        std::min(24.0F, trackHeight), trackHeight);
}

[[nodiscard]] constexpr float scrollbarThumbTop(float trackTop, float trackHeight, float thumbHeight,
    std::size_t topRow, std::size_t maximumTop) noexcept {
    if (maximumTop == 0 || trackHeight <= thumbHeight) return trackTop;
    return trackTop + (trackHeight - thumbHeight)
        * static_cast<float>(std::min(topRow, maximumTop)) / static_cast<float>(maximumTop);
}

[[nodiscard]] constexpr std::size_t topRowFromScrollbar(float pointerY, float grabOffset,
    float trackTop, float trackHeight, float thumbHeight, std::size_t maximumTop) noexcept {
    if (maximumTop == 0 || trackHeight <= thumbHeight) return 0;
    const auto fraction = std::clamp((pointerY - grabOffset - trackTop) / (trackHeight - thumbHeight), 0.0F, 1.0F);
    return static_cast<std::size_t>(fraction * static_cast<float>(maximumTop) + 0.5F);
}

} // namespace refrain
