#include "playlist_config_model.h"
#include "playlist_grouping_model.h"

#include <iostream>

namespace {
int failures{};
void expect(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; ++failures; }
}
}

int main() {
    using namespace refrain;
    const std::vector<PlaylistGroupInput> input{
        {"A", "Album A", "2020", "Artist", "Rock"},
        {"A", "Album A", "2020", "Artist", "Rock"},
        {"B", "Album B", "2021", "Artist", "Pop"},
    };
    auto groups = buildPlaylistGroups(input, false);
    expect(groups.size() == 2 && groups[0].start == 0 && groups[0].count == 2, "adjacent keys must group");
    auto rows = buildPlaylistDisplayRows(groups, 3);
    expect(rows.size() == 8, "expanded groups must include stable cover spacer rows");
    expect(rows[0].kind == PlaylistDisplayRowKind::groupHeader && rows[1].track == 0 && rows[2].track == 1,
        "display rows must preserve real track indexes");
    expect(groupForTrack(groups, 2) == 1 && displayRowForTrack(rows, 2) == 5, "track lookup must cross groups");
    groups[0].collapsed = true;
    rows = buildPlaylistDisplayRows(groups, 3);
    expect(rows.size() == 5 && displayRowForTrack(rows, 0) == static_cast<std::size_t>(-1),
        "collapsed tracks must be absent without changing real indexes");

    auto settings = defaultPlaylistViewSettings();
    settings.groups.push_back({"custom.percent", "Custom %\t", "%album%\n", "%title%", "%album%", "", "", "",
        GroupArtworkSource::placeholder, false});
    settings.activeGroupId = "custom.percent";
    const auto restored = deserializePlaylistViewSettings(serializePlaylistViewSettings(settings));
    expect(restored == normalizePlaylistViewSettings(settings), "playlist settings must round trip arbitrary delimiters");
    auto broken = defaultPlaylistViewSettings();
    broken.columns.clear();
    broken.activeGroupId = "missing";
    broken = normalizePlaylistViewSettings(std::move(broken));
    expect(broken.activeGroupId == "builtin.album.simple", "missing active group must fall back");
    expect(!broken.columns.empty(), "missing built-in columns must be restored");
    expect(std::any_of(broken.columns.begin(), broken.columns.end(), [](const auto& c) { return c.visible && !c.cover; }),
        "at least one text column must remain visible");
    return failures == 0 ? 0 : 1;
}
