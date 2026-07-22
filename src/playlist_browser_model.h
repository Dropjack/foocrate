#pragma once

#include <guiddef.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

namespace foocrate {

enum class PlaylistBrowserKind {
    manual,
    automatic,
    reserved,
};

enum class AutoplaylistPresetSort {
    album,
    query,
};

struct AutoplaylistPreset {
    std::wstring_view name;
    std::string_view query;
    AutoplaylistPresetSort sort{AutoplaylistPresetSort::album};
    bool requiresPlaybackStatistics{};
};

inline constexpr std::array<AutoplaylistPreset, 11> kAutoplaylistPresets{{
    {L"Library (full)", "ALL"},
    {L"Never played", "%play_count% IS 0", AutoplaylistPresetSort::album, true},
    {L"History - last week",
        "%last_played% DURING LAST 1 WEEK SORT DESCENDING BY %last_played%",
        AutoplaylistPresetSort::query, true},
    {L"Played often", "%play_count% GREATER 0 SORT DESCENDING BY %play_count%",
        AutoplaylistPresetSort::query, true},
    {L"Recently added - 12 weeks",
        "%added% DURING LAST 12 WEEKS SORT DESCENDING BY %added%",
        AutoplaylistPresetSort::query, true},
    {L"Unrated", "%rating% MISSING"},
    {L"Rated 1", "%rating% IS 1"},
    {L"Rated 2", "%rating% IS 2"},
    {L"Rated 3", "%rating% IS 3"},
    {L"Rated 4", "%rating% IS 4"},
    {L"Rated 5", "%rating% IS 5"},
}};

enum class AutoplaylistRollbackResult {
    noContainer,
    removed,
    failed,
};

[[nodiscard]] inline std::wstring autoplaylistFailureMessage(
    std::wstring_view name, std::wstring_view technicalDetail,
    bool requiresPlaybackStatistics, AutoplaylistRollbackResult rollback) {
    std::wstring message = L"FooCrate could not create the autoplaylist \"";
    message.append(name);
    message += L"\".\r\n\r\n";
    if (requiresPlaybackStatistics) {
        message += L"This preset uses Playback Statistics fields. Confirm that Playback Statistics "
                   L"is installed and enabled.\r\n\r\n";
    }
    switch (rollback) {
    case AutoplaylistRollbackResult::noContainer:
        message += L"No playlist container was created.";
        break;
    case AutoplaylistRollbackResult::removed:
        message += L"The empty playlist container was removed.";
        break;
    case AutoplaylistRollbackResult::failed:
        message += L"The empty playlist container could not be removed; delete it manually.";
        break;
    }
    if (!technicalDetail.empty()) {
        message += L"\r\n\r\nTechnical detail: ";
        message.append(technicalDetail);
    }
    return message;
}

[[nodiscard]] inline std::wstring autoplaylistDependencyMessage(std::wstring_view name) {
    std::wstring message = L"The autoplaylist preset \"";
    message.append(name);
    message += L"\" requires Playback Statistics. Install or enable Playback Statistics, "
               L"then try again.\r\n\r\nNo playlist container was created.";
    return message;
}

struct PlaylistBrowserRow {
    GUID guid{};
    std::wstring name;
    std::size_t itemCount{};
    PlaylistBrowserKind kind{PlaylistBrowserKind::manual};
    unsigned lockMask{};
    bool active{};
    bool playing{};
};

[[nodiscard]] inline bool sameGuid(const GUID& left, const GUID& right) noexcept {
    return InlineIsEqualGUID(left, right) != 0;
}

[[nodiscard]] inline bool nullGuid(const GUID& value) noexcept {
    return sameGuid(value, GUID{});
}

[[nodiscard]] inline std::size_t findPlaylistRowByGuid(
    const std::vector<PlaylistBrowserRow>& rows, const GUID& guid) noexcept {
    const auto found = std::find_if(rows.begin(), rows.end(),
        [&](const PlaylistBrowserRow& row) { return sameGuid(row.guid, guid); });
    return found == rows.end() ? static_cast<std::size_t>(-1)
                               : static_cast<std::size_t>(found - rows.begin());
}

[[nodiscard]] inline std::size_t chooseDefaultPlaylistRow(
    const std::vector<PlaylistBrowserRow>& rows, const GUID& preferred) noexcept {
    const auto preferredIndex = findPlaylistRowByGuid(rows, preferred);
    if (preferredIndex != static_cast<std::size_t>(-1)
        && rows[preferredIndex].kind != PlaylistBrowserKind::reserved) {
        return preferredIndex;
    }
    const auto found = std::find_if(rows.begin(), rows.end(), [](const PlaylistBrowserRow& row) {
        return row.kind != PlaylistBrowserKind::reserved;
    });
    return found == rows.end() ? static_cast<std::size_t>(-1)
                               : static_cast<std::size_t>(found - rows.begin());
}

[[nodiscard]] inline std::wstring uniquePlaylistName(
    const std::vector<std::wstring>& names, const std::wstring& base) {
    const auto exists = [&](const std::wstring& candidate) {
        return std::find(names.begin(), names.end(), candidate) != names.end();
    };
    if (!exists(base)) return base;
    for (std::size_t suffix = 2;; ++suffix) {
        auto candidate = base + L" (" + std::to_wstring(suffix) + L")";
        if (!exists(candidate)) return candidate;
    }
}

[[nodiscard]] inline std::vector<GUID> reconcilePlaylistSelection(
    const std::vector<PlaylistBrowserRow>& rows, const std::vector<GUID>& prior,
    const GUID& active) {
    std::vector<GUID> result;
    for (const auto& guid : prior) {
        if (findPlaylistRowByGuid(rows, guid) != static_cast<std::size_t>(-1)
            && std::none_of(result.begin(), result.end(),
                [&](const GUID& value) { return sameGuid(value, guid); })) {
            result.push_back(guid);
        }
    }
    if (result.empty() && findPlaylistRowByGuid(rows, active) != static_cast<std::size_t>(-1)) {
        result.push_back(active);
    }
    return result;
}

[[nodiscard]] inline int normalizePlaylistBrowserSplitPermille(int value) noexcept {
    return std::clamp(value, 100, 350);
}

} // namespace foocrate
