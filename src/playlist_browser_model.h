#pragma once

#include <guiddef.h>

#include <algorithm>
#include <cstddef>
#include <cwctype>
#include <string>
#include <vector>

namespace refrain {

enum class PlaylistBrowserKind {
    manual,
    automatic,
    reserved,
};

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

} // namespace refrain
