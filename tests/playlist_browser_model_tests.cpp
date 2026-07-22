#include "playlist_browser_model.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
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
    using namespace foocrate;
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

    expect(normalizePlaylistBrowserSplitPermille(20) == 100, "split must have safe lower storage bound");
    expect(normalizePlaylistBrowserSplitPermille(500) == 350, "split must have 35 percent upper bound");
    expect(normalizePlaylistBrowserSplitPermille(150) == 150, "valid split must survive");

    expect(kAutoplaylistPresets.size() == 11, "all approved autoplaylist presets must remain present");
    expect(kAutoplaylistPresets[1].requiresPlaybackStatistics,
        "Never played must declare its Playback Statistics dependency");
    expect(kAutoplaylistPresets[2].sort == AutoplaylistPresetSort::query
            && kAutoplaylistPresets[3].sort == AutoplaylistPresetSort::query
            && kAutoplaylistPresets[4].sort == AutoplaylistPresetSort::query,
        "history presets must retain their explicit descending query sort");
    expect(kAutoplaylistPresets[0].sort == AutoplaylistPresetSort::album
            && kAutoplaylistPresets[5].sort == AutoplaylistPresetSort::album,
        "library and rating presets must retain album sorting");

    const auto failure = autoplaylistFailureMessage(L"Never played", L"Cannot create here", true,
        AutoplaylistRollbackResult::removed);
    expect(failure.find(L"Never played") != std::wstring::npos,
        "autoplaylist errors must identify the selected preset");
    expect(failure.find(L"Playback Statistics") != std::wstring::npos,
        "dynamic preset errors must explain their dependency");
    expect(failure.find(L"empty playlist container was removed") != std::wstring::npos,
        "autoplaylist errors must report successful rollback");
    expect(failure.find(L"Cannot create here") != std::wstring::npos,
        "autoplaylist errors must preserve the technical detail");

    const auto dependency = autoplaylistDependencyMessage(L"History - last week");
    expect(dependency.find(L"History - last week") != std::wstring::npos
            && dependency.find(L"No playlist container was created") != std::wstring::npos,
        "missing dependency feedback must identify the preset and confirm no side effect");
    return 0;
}
