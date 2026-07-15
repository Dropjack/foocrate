#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace refrain {

enum class PlaylistDragStartMode { internalReorder, outgoingCopy };

[[nodiscard]] constexpr PlaylistDragStartMode playlistDragStartMode(bool sourceCanReorder) noexcept {
    return sourceCanReorder ? PlaylistDragStartMode::internalReorder
                            : PlaylistDragStartMode::outgoingCopy;
}

[[nodiscard]] inline std::vector<std::size_t> selectedIndexes(
    const std::vector<bool>& selected, std::size_t count) {
    std::vector<std::size_t> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count && index < selected.size(); ++index) {
        if (selected[index]) result.push_back(index);
    }
    return result;
}

struct PlaylistMovePlan {
    std::vector<std::size_t> order;
    std::vector<std::size_t> movedDestinations;
    bool changed{};
};

[[nodiscard]] inline PlaylistMovePlan planPlaylistMove(
    std::size_t count, const std::vector<std::size_t>& selectedInput, std::size_t insertionBoundary) {
    PlaylistMovePlan result;
    result.order.reserve(count);
    insertionBoundary = std::min(insertionBoundary, count);

    std::vector<bool> selected(count);
    std::vector<std::size_t> moved;
    moved.reserve(selectedInput.size());
    for (const auto index : selectedInput) {
        if (index < count && !selected[index]) {
            selected[index] = true;
            moved.push_back(index);
        }
    }
    std::sort(moved.begin(), moved.end());
    if (moved.empty()) {
        for (std::size_t index = 0; index < count; ++index) result.order.push_back(index);
        return result;
    }

    std::vector<std::size_t> remaining;
    remaining.reserve(count - moved.size());
    std::size_t removedBefore{};
    for (std::size_t index = 0; index < count; ++index) {
        if (selected[index]) {
            if (index < insertionBoundary) ++removedBefore;
        } else {
            remaining.push_back(index);
        }
    }
    const auto destination = std::min(insertionBoundary - removedBefore, remaining.size());
    result.order.insert(result.order.end(), remaining.begin(), remaining.begin() + static_cast<std::ptrdiff_t>(destination));
    const auto firstMoved = result.order.size();
    result.order.insert(result.order.end(), moved.begin(), moved.end());
    result.order.insert(result.order.end(), remaining.begin() + static_cast<std::ptrdiff_t>(destination), remaining.end());
    for (std::size_t offset = 0; offset < moved.size(); ++offset) {
        result.movedDestinations.push_back(firstMoved + offset);
    }
    for (std::size_t index = 0; index < result.order.size(); ++index) {
        if (result.order[index] != index) {
            result.changed = true;
            break;
        }
    }
    return result;
}

[[nodiscard]] inline std::wstring compactQueuePositions(const std::vector<std::size_t>& zeroBased) {
    if (zeroBased.empty()) return {};
    auto result = std::to_wstring(zeroBased.front() + 1);
    if (zeroBased.size() > 1) result += L" +" + std::to_wstring(zeroBased.size() - 1);
    return result;
}

[[nodiscard]] inline std::vector<std::size_t> prioritizeIndexOrder(
    std::size_t count, std::size_t priority) {
    std::vector<std::size_t> result;
    if (priority >= count) return result;
    result.reserve(count);
    result.push_back(priority);
    for (std::size_t index = 0; index < count; ++index) {
        if (index != priority) result.push_back(index);
    }
    return result;
}

template<typename T>
[[nodiscard]] inline std::vector<T> uniqueValuesNotIn(
    const std::vector<T>& existing, const std::vector<T>& incoming) {
    std::vector<T> result;
    result.reserve(incoming.size());
    for (const auto& value : incoming) {
        if (std::find(existing.begin(), existing.end(), value) == existing.end()
            && std::find(result.begin(), result.end(), value) == result.end()) {
            result.push_back(value);
        }
    }
    return result;
}

[[nodiscard]] constexpr std::size_t insertionBoundaryForRow(
    std::size_t row, bool lowerHalf, std::size_t itemCount) noexcept {
    if (itemCount == 0) return 0;
    return std::min(row + (lowerHalf ? 1U : 0U), itemCount);
}

} // namespace refrain
