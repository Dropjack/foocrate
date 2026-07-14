#include "playlist_browser_model.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool value, const char* message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

GUID guid(unsigned value) {
    return GUID{value, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
}

} // namespace

int main() {
    using namespace refrain;
    const std::vector<PlaylistBrowserRow> rows{
        {guid(1), L"Manual", 4, PlaylistBrowserKind::manual},
        {guid(2), L"Automatic", 8, PlaylistBrowserKind::automatic},
        {guid(3), L"Bridge", 2, PlaylistBrowserKind::reserved},
    };

    expect(findPlaylistRowByGuid(rows, guid(2)) == 1, "GUID lookup must survive index changes");
    expect(chooseDefaultPlaylistRow(rows, guid(2)) == 1, "valid preferred default must win");
    expect(chooseDefaultPlaylistRow(rows, guid(3)) == 0, "reserved default must fall back");
    expect(chooseDefaultPlaylistRow(rows, guid(99)) == 0, "missing default must fall back");
    expect(chooseDefaultPlaylistRow({rows[2]}, guid(3)) == static_cast<std::size_t>(-1),
        "reserved-only snapshots require a new manual default");

    expect(uniquePlaylistName({L"Dragged Items", L"Dragged Items (2)"}, L"Dragged Items")
            == L"Dragged Items (3)",
        "blank drops must use conflict-safe names");
    expect(uniquePlaylistName({L"Other"}, L"Dragged Items") == L"Dragged Items",
        "unused base name must remain unchanged");

    const auto selection = reconcilePlaylistSelection(rows, {guid(99), guid(2), guid(2)}, guid(1));
    expect(selection.size() == 1 && sameGuid(selection.front(), guid(2)),
        "selection reconciliation must remove stale and duplicate GUIDs");
    const auto fallback = reconcilePlaylistSelection(rows, {guid(99)}, guid(1));
    expect(fallback.size() == 1 && sameGuid(fallback.front(), guid(1)),
        "empty management selection must fall back to active row");

    const std::vector<PlaylistBrowserRow> filterRows{
        {guid(10), L"FavPop", 4, PlaylistBrowserKind::automatic},
        {guid(11), L"Classical Library", 8, PlaylistBrowserKind::manual},
        {guid(12), L"中文收藏", 2, PlaylistBrowserKind::manual},
    };
    const auto asciiFilter = filterPlaylistBrowserRows(filterRows, L"LIBRARY");
    expect(asciiFilter.size() == 1 && sameGuid(asciiFilter.front().guid, guid(11)),
        "playlist filter must use case-insensitive name containment");
    const auto chineseFilter = filterPlaylistBrowserRows(filterRows, L"收藏");
    expect(chineseFilter.size() == 1 && sameGuid(chineseFilter.front().guid, guid(12)),
        "playlist filter must preserve Unicode name matching");
    expect(filterPlaylistBrowserRows(filterRows, L"missing").empty(),
        "playlist filter must represent no-match results explicitly");
    expect(filterPlaylistBrowserRows(filterRows, L"").size() == filterRows.size(),
        "empty playlist filter must reveal every row");

    expect(normalizePlaylistBrowserSplitPermille(20) == 100, "split must have safe lower storage bound");
    expect(normalizePlaylistBrowserSplitPermille(500) == 350, "split must have 35 percent upper bound");
    expect(normalizePlaylistBrowserSplitPermille(150) == 150, "valid split must survive");
    return 0;
}
