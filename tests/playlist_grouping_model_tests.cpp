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
    using namespace foocrate;
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
    expect(displayRowForGroupHeader(rows, 1) == 4,
        "group header lookup must preserve the requested group identity");
    expect(groupForTrack(groups, 2) == 1 && displayRowForTrack(rows, 2) == 5, "track lookup must cross groups");
    groups[0].collapsed = true;
    rows = buildPlaylistDisplayRows(groups, 3);
    expect(rows.size() == 5 && displayRowForTrack(rows, 0) == static_cast<std::size_t>(-1),
        "collapsed tracks must be absent without changing real indexes");
    expect(togglePlaylistGroupsCollapsed(groups)
            && std::all_of(groups.begin(), groups.end(), [](const auto& group) { return group.collapsed; }),
        "Tab-style toggle must collapse every group from a mixed state");
    expect(togglePlaylistGroupsCollapsed(groups)
            && std::none_of(groups.begin(), groups.end(), [](const auto& group) { return group.collapsed; }),
        "Tab-style toggle must expand every group after all are collapsed");
    std::vector<PlaylistGroup> emptyGroups;
    expect(!togglePlaylistGroupsCollapsed(emptyGroups), "empty playlists must not report a group toggle");
    expect(displayRowForGroupHeader(rows, 99) == static_cast<std::size_t>(-1),
        "missing group headers must not resolve to another group");

    std::vector<PlaylistGroupInput> largeInput;
    largeInput.reserve(100000);
    for (std::size_t index = 0; index < 100000; ++index) {
        const auto group = std::to_string(index / 10);
        largeInput.push_back({group, "Album " + group, "2025", "Artist", "Genre"});
    }
    const auto largeGroups = buildPlaylistGroups(largeInput, false);
    const auto largeRows = buildPlaylistDisplayRows(largeGroups, 4);
    expect(largeGroups.size() == 10000 && largeRows.size() == 110000,
        "large grouped playlists must preserve deterministic row counts");
    expect(groupForTrack(largeGroups, 99999) == 9999,
        "large grouped playlists must retain last-track lookup");

    auto settings = defaultPlaylistViewSettings();
    expect(settings.activeGroupId == "builtin.album.disc"
            && !settings.profiles.empty() && settings.profiles.front().groupId == "builtin.album.disc",
        "new and unassigned playlists must use album artist/date/album/disc by default");
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
    expect(profiledRestored == normalizePlaylistViewSettings(profiled), "RPV3 profiles and assignments must round trip");
    auto legacyDefault = defaultPlaylistViewSettings();
    legacyDefault.activeGroupId = "builtin.album.simple";
    legacyDefault.profiles.front().groupId = "builtin.album.simple";
    auto legacyText = serializePlaylistViewSettings(legacyDefault);
    legacyText.replace(0, 4, "RPV2");
    const auto migratedDefault = deserializePlaylistViewSettings(legacyText);
    expect(migratedDefault.activeGroupId == "builtin.album.disc"
            && migratedDefault.profiles.front().groupId == "builtin.album.disc",
        "RPV2 default profiles must migrate once to album artist/date/album/disc");
    const auto effective = applyLayoutProfile(profiledRestored, profile.id);
    expect(!effective.columns[1].visible, "profile column visibility must be applied");
    auto broken = defaultPlaylistViewSettings();
    broken.columns.clear();
    broken.activeGroupId = "missing";
    broken = normalizePlaylistViewSettings(std::move(broken));
    expect(broken.activeGroupId == "builtin.album.disc", "missing active group must fall back");
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
