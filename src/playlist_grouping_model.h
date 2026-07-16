#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace foocrate {

struct PlaylistGroupInput {
    std::string key;
    std::string leftPrimary;
    std::string rightPrimary;
    std::string leftSecondary;
    std::string rightSecondary;
};

struct PlaylistGroup {
    std::size_t start{};
    std::size_t count{};
    std::string key;
    std::string leftPrimary;
    std::string rightPrimary;
    std::string leftSecondary;
    std::string rightSecondary;
    bool collapsed{};
};

enum class PlaylistDisplayRowKind { groupHeader, track, spacer };

struct PlaylistDisplayRow {
    PlaylistDisplayRowKind kind{PlaylistDisplayRowKind::track};
    std::size_t group{};
    std::size_t track{};
};

[[nodiscard]] inline std::vector<PlaylistGroup> buildPlaylistGroups(
    const std::vector<PlaylistGroupInput>& items, bool collapsedByDefault) {
    std::vector<PlaylistGroup> groups;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto& item = items[index];
        if (groups.empty() || groups.back().key != item.key) {
            groups.push_back({index, 1, item.key, item.leftPrimary, item.rightPrimary,
                item.leftSecondary, item.rightSecondary, collapsedByDefault});
        } else {
            ++groups.back().count;
        }
    }
    return groups;
}

[[nodiscard]] inline std::vector<PlaylistDisplayRow> buildPlaylistDisplayRows(
    const std::vector<PlaylistGroup>& groups, std::size_t minimumExpandedTracks) {
    std::vector<PlaylistDisplayRow> rows;
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        const auto& group = groups[groupIndex];
        rows.push_back({PlaylistDisplayRowKind::groupHeader, groupIndex, group.start});
        if (group.collapsed) continue;
        for (std::size_t offset = 0; offset < group.count; ++offset) {
            rows.push_back({PlaylistDisplayRowKind::track, groupIndex, group.start + offset});
        }
        for (std::size_t offset = group.count; offset < minimumExpandedTracks; ++offset) {
            rows.push_back({PlaylistDisplayRowKind::spacer, groupIndex, group.start});
        }
    }
    return rows;
}

[[nodiscard]] inline bool togglePlaylistGroupsCollapsed(std::vector<PlaylistGroup>& groups) noexcept {
    if (groups.empty()) return false;
    const auto collapse = std::any_of(groups.begin(), groups.end(),
        [](const PlaylistGroup& group) { return !group.collapsed; });
    for (auto& group : groups) group.collapsed = collapse;
    return true;
}

[[nodiscard]] inline std::size_t groupForTrack(
    const std::vector<PlaylistGroup>& groups, std::size_t track) noexcept {
    const auto found = std::upper_bound(groups.begin(), groups.end(), track,
        [](std::size_t value, const PlaylistGroup& group) { return value < group.start; });
    if (found == groups.begin()) return static_cast<std::size_t>(-1);
    const auto index = static_cast<std::size_t>(std::prev(found) - groups.begin());
    return track < groups[index].start + groups[index].count ? index : static_cast<std::size_t>(-1);
}

[[nodiscard]] inline std::size_t displayRowForTrack(
    const std::vector<PlaylistDisplayRow>& rows, std::size_t track) noexcept {
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const PlaylistDisplayRow& row) {
        return row.kind == PlaylistDisplayRowKind::track && row.track == track;
    });
    return found == rows.end() ? static_cast<std::size_t>(-1)
                              : static_cast<std::size_t>(found - rows.begin());
}

} // namespace foocrate
