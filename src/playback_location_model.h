#pragma once

#include <algorithm>
#include <cstddef>

namespace foocrate {

[[nodiscard]] constexpr std::size_t restoredPlaylistTopRow(std::size_t displayRow,
    std::size_t viewportAnchor, std::size_t rowCount, std::size_t visibleCapacity) noexcept {
    if (rowCount == 0 || visibleCapacity == 0) return 0;
    const auto maximum = rowCount > visibleCapacity ? rowCount - visibleCapacity : 0;
    const auto candidate = displayRow > viewportAnchor ? displayRow - viewportAnchor : 0;
    return std::min(candidate, maximum);
}

} // namespace foocrate
