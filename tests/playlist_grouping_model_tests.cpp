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
    const auto albumDisc = std::find_if(settings.groups.begin(), settings.groups.end(),
        [](const auto& group) { return group.id == "builtin.album.disc"; });
    expect(albumDisc != settings.groups.end() && albumDisc->sortFormat == kAlbumArtistYearAlbumSort,
        "album/disc grouping must use album artist then chronological album order");
    settings.groups.push_back({"custom.percent", "Custom %\t", "%album%\n", "%title%", "%album%", "", "", "",
        GroupArtworkSource::placeholder, false});
    settings.activeGroupId = "custom.percent";
    const auto restored = deserializePlaylistViewSettings(serializePlaylistViewSettings(settings));
    expect(restored == normalizePlaylistViewSettings(settings), "playlist settings must round trip arbitrary delimiters");
    auto profiled = defaultPlaylistViewSettings();
    auto profile = profiled.profiles.front(); profile.id = "custom.profile.test"; profile.label = "Test";
    profile.trackRowLayout = TrackRowLayout::twoLine; profile.groupHeaderStyle = GroupHeaderStyle::compactLine;
    profile.columns[1].visible = false; profiled.profiles.push_back(profile);
    profiled.assignments.push_back({"{00000000-0000-0000-0000-000000000001}", profile.id});
    const auto profiledRestored = deserializePlaylistViewSettings(serializePlaylistViewSettings(profiled));
    expect(profiledRestored == normalizePlaylistViewSettings(profiled), "RPV2 profiles and assignments must round trip");
    const auto effective = applyLayoutProfile(profiledRestored, profile.id);
    expect(!effective.columns[1].visible, "profile column visibility must be applied");
    auto broken = defaultPlaylistViewSettings();
    broken.columns.clear();
    broken.activeGroupId = "missing";
    broken = normalizePlaylistViewSettings(std::move(broken));
    expect(broken.activeGroupId == "builtin.album.simple", "missing active group must fall back");
    expect(!broken.columns.empty(), "missing built-in columns must be restored");
    const auto state = std::find_if(broken.columns.begin(), broken.columns.end(),
        [](const auto& column) { return column.id == "builtin.state"; });
    expect(state != broken.columns.end() && state->builtIn && state->visible,
        "State / Queue must be a stable visible built-in column");
    expect(std::any_of(broken.columns.begin(), broken.columns.end(), [](const auto& c) { return c.visible && !c.cover; }),
        "at least one text column must remain visible");
    expect(shouldPhysicallySortForGroupChange(true, true, 2),
        "a changed group on a reorderable playlist should request physical sorting");
    expect(!shouldPhysicallySortForGroupChange(true, false, 20),
        "a locked autoplaylist must skip physical sorting without blocking visual grouping");
    expect(!shouldPhysicallySortForGroupChange(false, true, 20),
        "an unchanged group must not request physical sorting");
    return failures == 0 ? 0 : 1;
}
