#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace refrain {

enum class GroupArtworkSource : std::int32_t { front = 0, artist = 1, placeholder = 2 };
enum class ColumnAlignment : std::int32_t { leading = 0, center = 1, trailing = 2 };
enum class TrackRowLayout : std::int32_t { compact = 0, standard = 1, twoLine = 2 };
enum class GroupHeaderStyle : std::int32_t { detailed = 0, compactLine = 1 };

struct GroupDefinition {
    std::string id;
    std::string label;
    std::string keyFormat;
    std::string sortFormat;
    std::string leftPrimary;
    std::string rightPrimary;
    std::string leftSecondary;
    std::string rightSecondary;
    GroupArtworkSource artwork{GroupArtworkSource::front};
    bool builtIn{};
    bool operator==(const GroupDefinition&) const = default;
};

struct ColumnDefinition {
    std::string id;
    std::string label;
    std::string displayFormat;
    std::string secondaryDisplayFormat;
    std::string sortFormat;
    ColumnAlignment alignment{ColumnAlignment::leading};
    bool visible{};
    std::int32_t widthWeight{800};
    bool builtIn{};
    bool cover{};
    bool operator==(const ColumnDefinition&) const = default;
};

struct ProfileColumn {
    std::string columnId;
    bool visible{};
    std::int32_t widthWeight{800};
    bool operator==(const ProfileColumn&) const = default;
};

struct LayoutProfile {
    std::string id;
    std::string label;
    std::string groupId{"builtin.album.simple"};
    TrackRowLayout trackRowLayout{TrackRowLayout::standard};
    GroupHeaderStyle groupHeaderStyle{GroupHeaderStyle::detailed};
    bool autoCollapse{};
    bool collapseByDefault{};
    bool builtIn{};
    std::vector<ProfileColumn> columns;
    bool operator==(const LayoutProfile&) const = default;
};

struct PlaylistProfileAssignment {
    std::string playlistGuid;
    std::string profileId;
    bool operator==(const PlaylistProfileAssignment&) const = default;
};

struct PlaylistViewSettings {
    std::int32_t version{2};
    std::string activeGroupId{"builtin.album.simple"};
    bool autoCollapse{};
    bool collapseByDefault{};
    std::vector<GroupDefinition> groups;
    std::vector<ColumnDefinition> columns;
    std::vector<LayoutProfile> profiles;
    std::vector<PlaylistProfileAssignment> assignments;
    bool operator==(const PlaylistViewSettings&) const = default;
};

inline constexpr std::int32_t kPlaylistViewSettingsVersion = 2;

[[nodiscard]] inline std::vector<GroupDefinition> defaultGroupDefinitions() {
    return {
        {"builtin.album.simple", "Album (simple)", "$if2(%album%,'Single')",
            "%album% | %discnumber% | %tracknumber% | %title%",
            "$if2(%album%,'Single')", "$if(%year%,%year%,' ')",
            "$if2(%album artist%,'Unknown artist')", "$if2(%genre%,' ')",
            GroupArtworkSource::front, true},
        {"builtin.album.disc", "Album Artist / Album / Disc",
            "$if2(%album artist%,'Unknown artist')|$if2(%album%,'Single')|$if2(%discnumber%,1)",
            "%album artist% | $if(%album%,%date%,'9999') | %album% | %discnumber% | %tracknumber% | %title%",
            "$if2(%album%,'Single')$ifgreater(%totaldiscs%,1,' - Disc '$if2(%discnumber%,1),)",
            "$if(%year%,%year%,' ')", "$if2(%album artist%,'Unknown artist')", "$if2(%genre%,' ')",
            GroupArtworkSource::front, true},
        {"builtin.albumartist", "Album Artist", "$if2(%album artist%,'Unknown artist')",
            "%album artist% | $if(%album%,%date%,'9999') | %album% | %discnumber% | %tracknumber% | %title%",
            "$if2(%album artist%,'Unknown artist')", "$if2(%genre%,' ')", "", "",
            GroupArtworkSource::artist, true},
        {"builtin.artist", "Artist", "$if2(%artist%,'Unknown artist')",
            "%artist% | $if(%album%,%date%,'9999') | %album% | %discnumber% | %tracknumber% | %title%",
            "$if2(%artist%,'Unknown artist')", "$if2(%genre%,' ')", "", "",
            GroupArtworkSource::artist, true},
        {"builtin.genre", "Genre", "$if2(%genre%,'Unknown genre')",
            "%genre% | %album artist% | $if(%album%,%date%,'9999') | %album% | %discnumber% | %tracknumber% | %title%",
            "$if2(%genre%,'Unknown genre')", "", "", "",
            GroupArtworkSource::placeholder, true},
        {"builtin.directory", "Directory", "$directory_path(%path%)",
            "$directory_path(%path%) | %album artist% | $if(%album%,%date%,'9999') | %album% | %discnumber% | %tracknumber% | %title%",
            "$directory(%path%,1)", "$if(%year%,%year%,' ')", "$directory(%path%,2)", "$if2(%genre%,' ')",
            GroupArtworkSource::front, true},
    };
}

[[nodiscard]] inline std::vector<ColumnDefinition> defaultColumnDefinitions() {
    return {
        {"builtin.cover", "Cover", "", "", "", ColumnAlignment::center, true, 900, true, true},
        {"builtin.state", "State", "", "", "", ColumnAlignment::center, true, 600, true, false},
        {"builtin.track", "#", "$if(%discnumber%,$num(%discnumber%,1)'.',)$if(%tracknumber%,$num(%tracknumber%,2),%list_index%)",
            "$if2(%play_count%,0)",
            "%tracknumber% | %album artist% | %album% | %discnumber% | %title%",
            ColumnAlignment::trailing, true, 650, true, false},
        {"builtin.index", "Index", "$num(%list_index%,$len(%list_total%))", "",
            "",
            ColumnAlignment::trailing, false, 650, true, false},
        {"builtin.title", "Title", "$if2(%title%,$filename_ext(%path%))",
            "$if2(%album artist%,'Unknown artist')",
            "%title% | %album artist% | %album% | %discnumber% | %tracknumber%",
            ColumnAlignment::leading, true, 2300, true, false},
        {"builtin.artist", "Artist", "[%artist%]", "",
            "%artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::leading, true, 1600, true, false},
        {"builtin.albumartist", "Album Artist", "[%album artist%]",
            "",
            "%album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::leading, false, 1500, true, false},
        {"builtin.album", "Album", "[%album%]", "$if2(%genre%,'Other')", "%album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::leading, true, 1700, true, false},
        {"builtin.date", "Date", "[%date%]", "",
            "%date% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::center, false, 700, true, false},
        {"builtin.genre", "Genre", "[%genre%]", "",
            "%genre% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::leading, true, 900, true, false},
        {"builtin.mood", "Mood", "$if(%mood%,1,0)", "",
            "%mood% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::center, false, 650, true, false},
        {"builtin.rating", "Rating", "$if2(%rating%,0)", "",
            "%rating% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::center, true, 900, true, false},
        {"builtin.plays", "Plays", "$if2(%play_count%,0)", "",
            "$if2(%play_count%,0) | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::trailing, false, 650, true, false},
        {"builtin.bitrate", "Bitrate", "[$if2(%__bitrate_dynamic%,%bitrate%)K]",
            "",
            "%bitrate% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::trailing, true, 750, true, false},
        {"builtin.codec", "Codec", "[%codec%]", "",
            "%codec% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::center, true, 650, true, false},
        {"builtin.samplerate", "Sample Rate", "[$div(%samplerate%,1000) kHz]",
            "",
            "%samplerate% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::trailing, false, 800, true, false},
        {"builtin.length", "Length", "[%length%]", "[$if2(%__bitrate_dynamic%,%bitrate%)K]", "$if2(%length%,' 0:00') | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::trailing, true, 700, true, false},
    };
}

[[nodiscard]] inline PlaylistViewSettings defaultPlaylistViewSettings() {
    PlaylistViewSettings value;
    value.groups = defaultGroupDefinitions();
    value.columns = defaultColumnDefinitions();
    LayoutProfile profile;
    profile.id = "builtin.default";
    profile.label = "Default";
    profile.builtIn = true;
    for (const auto& column : value.columns) profile.columns.push_back({column.id, column.visible, column.widthWeight});
    value.profiles.push_back(std::move(profile));
    return value;
}

namespace playlist_config_detail {

inline void limit(std::string& value, std::size_t size) {
    if (value.size() > size) value.resize(size);
}

[[nodiscard]] inline std::string encode(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const auto byte : value) {
        const auto c = static_cast<unsigned char>(byte);
        if (c == '%' || c == '\t' || c == '\r' || c == '\n') {
            out.push_back('%'); out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]);
        } else out.push_back(static_cast<char>(c));
    }
    return out;
}

[[nodiscard]] inline int hexValue(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

[[nodiscard]] inline bool decode(std::string_view value, std::string& out) {
    out.clear();
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') { out.push_back(value[index]); continue; }
        if (index + 2 >= value.size()) return false;
        const auto high = hexValue(value[index + 1]);
        const auto low = hexValue(value[index + 2]);
        if (high < 0 || low < 0) return false;
        out.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return true;
}

[[nodiscard]] inline std::vector<std::string_view> split(std::string_view value, char delimiter) {
    std::vector<std::string_view> parts;
    std::size_t start{};
    while (start <= value.size()) {
        const auto end = value.find(delimiter, start);
        parts.push_back(value.substr(start, end == std::string_view::npos ? value.size() - start : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return parts;
}

template<typename T> [[nodiscard]] inline bool number(std::string_view text, T& out) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

} // namespace playlist_config_detail

[[nodiscard]] inline PlaylistViewSettings normalizePlaylistViewSettings(PlaylistViewSettings value) {
    value.version = kPlaylistViewSettingsVersion;
    if (value.groups.size() > 32) value.groups.resize(32);
    if (value.columns.size() > 32) value.columns.resize(32);
    std::unordered_set<std::string> groupIds;
    std::erase_if(value.groups, [&](GroupDefinition& group) {
        playlist_config_detail::limit(group.id, 96); playlist_config_detail::limit(group.label, 128);
        playlist_config_detail::limit(group.keyFormat, 2048); playlist_config_detail::limit(group.sortFormat, 2048);
        playlist_config_detail::limit(group.leftPrimary, 2048); playlist_config_detail::limit(group.rightPrimary, 2048);
        playlist_config_detail::limit(group.leftSecondary, 2048); playlist_config_detail::limit(group.rightSecondary, 2048);
        if (group.id.empty() || group.label.empty() || group.keyFormat.empty() || !groupIds.insert(group.id).second) return true;
        if (group.artwork < GroupArtworkSource::front || group.artwork > GroupArtworkSource::placeholder)
            group.artwork = GroupArtworkSource::placeholder;
        return false;
    });
    for (const auto& preset : defaultGroupDefinitions()) {
        const auto found = std::find_if(value.groups.begin(), value.groups.end(), [&](const auto& group) {
            return group.id == preset.id;
        });
        if (found == value.groups.end()) { value.groups.push_back(preset); groupIds.insert(preset.id); }
        else *found = preset;
    }
    if (!groupIds.contains(value.activeGroupId)) value.activeGroupId = "builtin.album.simple";

    std::unordered_set<std::string> columnIds;
    std::erase_if(value.columns, [&](ColumnDefinition& column) {
        playlist_config_detail::limit(column.id, 96); playlist_config_detail::limit(column.label, 128);
        playlist_config_detail::limit(column.displayFormat, 2048);
        playlist_config_detail::limit(column.secondaryDisplayFormat, 2048);
        playlist_config_detail::limit(column.sortFormat, 2048);
        if (column.id.empty() || column.label.empty() || !columnIds.insert(column.id).second) return true;
        if (column.alignment < ColumnAlignment::leading || column.alignment > ColumnAlignment::trailing)
            column.alignment = ColumnAlignment::leading;
        column.widthWeight = std::clamp(column.widthWeight, 100, 5000);
        return false;
    });
    for (const auto& column : defaultColumnDefinitions()) {
        const auto found = std::find_if(value.columns.begin(), value.columns.end(), [&](const auto& candidate) {
            return candidate.id == column.id;
        });
        if (found == value.columns.end()) { value.columns.push_back(column); columnIds.insert(column.id); }
        else {
            const auto visible = found->visible;
            const auto width = found->widthWeight;
            const auto secondary = found->secondaryDisplayFormat;
            *found = column;
            found->visible = visible;
            found->widthWeight = width;
            if (!secondary.empty()) found->secondaryDisplayFormat = secondary;
        }
    }
    if (const auto cover = std::find_if(value.columns.begin(), value.columns.end(),
            [](const auto& column) { return column.cover; }); cover != value.columns.end() && cover != value.columns.begin()) {
        std::rotate(value.columns.begin(), cover, cover + 1);
    }
    if (std::none_of(value.columns.begin(), value.columns.end(), [](const auto& c) { return c.visible && !c.cover; })) {
        if (const auto title = std::find_if(value.columns.begin(), value.columns.end(),
                [](const auto& c) { return c.id == "builtin.title"; }); title != value.columns.end()) title->visible = true;
    }
    std::unordered_set<std::string> profileIds;
    std::erase_if(value.profiles, [&](LayoutProfile& profile) {
        playlist_config_detail::limit(profile.id, 96); playlist_config_detail::limit(profile.label, 128);
        playlist_config_detail::limit(profile.groupId, 96);
        if (profile.id.empty() || profile.label.empty() || !profileIds.insert(profile.id).second) return true;
        if (!groupIds.contains(profile.groupId)) profile.groupId = "builtin.album.simple";
        if (profile.trackRowLayout < TrackRowLayout::compact || profile.trackRowLayout > TrackRowLayout::twoLine)
            profile.trackRowLayout = TrackRowLayout::standard;
        if (profile.groupHeaderStyle < GroupHeaderStyle::detailed || profile.groupHeaderStyle > GroupHeaderStyle::compactLine)
            profile.groupHeaderStyle = GroupHeaderStyle::detailed;
        std::unordered_set<std::string> used;
        std::erase_if(profile.columns, [&](ProfileColumn& state) {
            playlist_config_detail::limit(state.columnId, 96);
            if (!columnIds.contains(state.columnId) || !used.insert(state.columnId).second) return true;
            state.widthWeight = std::clamp(state.widthWeight, 100, 5000);
            return false;
        });
        for (const auto& column : value.columns) if (!used.contains(column.id))
            profile.columns.push_back({column.id, column.visible, column.widthWeight});
        return false;
    });
    auto defaultProfile = std::find_if(value.profiles.begin(), value.profiles.end(),
        [](const auto& profile) { return profile.id == "builtin.default"; });
    if (defaultProfile == value.profiles.end()) {
        LayoutProfile profile{"builtin.default", "Default", value.activeGroupId,
            TrackRowLayout::standard, GroupHeaderStyle::detailed, value.autoCollapse,
            value.collapseByDefault, true, {}};
        for (const auto& column : value.columns) profile.columns.push_back({column.id, column.visible, column.widthWeight});
        value.profiles.insert(value.profiles.begin(), std::move(profile));
    } else {
        defaultProfile->label = "Default";
        defaultProfile->builtIn = true;
    }
    profileIds.clear();
    for (const auto& profile : value.profiles) profileIds.insert(profile.id);
    std::unordered_set<std::string> assignedPlaylists;
    std::erase_if(value.assignments, [&](PlaylistProfileAssignment& assignment) {
        playlist_config_detail::limit(assignment.playlistGuid, 64);
        playlist_config_detail::limit(assignment.profileId, 96);
        return assignment.playlistGuid.empty() || !profileIds.contains(assignment.profileId)
            || assignment.profileId == "builtin.default"
            || !assignedPlaylists.insert(assignment.playlistGuid).second;
    });
    return value;
}

[[nodiscard]] inline PlaylistViewSettings applyLayoutProfile(PlaylistViewSettings value, std::string_view profileId) {
    value = normalizePlaylistViewSettings(std::move(value));
    auto profile = std::find_if(value.profiles.begin(), value.profiles.end(), [&](const auto& candidate) {
        return candidate.id == profileId;
    });
    if (profile == value.profiles.end()) profile = std::find_if(value.profiles.begin(), value.profiles.end(),
        [](const auto& candidate) { return candidate.id == "builtin.default"; });
    if (profile == value.profiles.end()) return value;
    value.activeGroupId = profile->groupId;
    value.autoCollapse = profile->autoCollapse;
    value.collapseByDefault = profile->collapseByDefault;
    std::vector<ColumnDefinition> ordered;
    ordered.reserve(value.columns.size());
    for (const auto& state : profile->columns) {
        const auto found = std::find_if(value.columns.begin(), value.columns.end(), [&](const auto& column) {
            return column.id == state.columnId;
        });
        if (found == value.columns.end()) continue;
        auto column = *found; column.visible = state.visible; column.widthWeight = state.widthWeight;
        ordered.push_back(std::move(column));
    }
    value.columns = std::move(ordered);
    return value;
}

[[nodiscard]] inline std::string serializePlaylistViewSettings(const PlaylistViewSettings& input) {
    const auto value = normalizePlaylistViewSettings(input);
    auto encoded = [](std::string_view text) { return playlist_config_detail::encode(text); };
    std::string out = "RPV2\nS\t" + encoded(value.activeGroupId) + "\t" + (value.autoCollapse ? "1" : "0")
        + "\t" + (value.collapseByDefault ? "1" : "0") + "\n";
    for (const auto& group : value.groups) {
        out += "G\t" + encoded(group.id) + "\t" + encoded(group.label) + "\t" + encoded(group.keyFormat)
            + "\t" + encoded(group.sortFormat) + "\t" + encoded(group.leftPrimary) + "\t" + encoded(group.rightPrimary)
            + "\t" + encoded(group.leftSecondary) + "\t" + encoded(group.rightSecondary) + "\t"
            + std::to_string(static_cast<int>(group.artwork)) + "\t" + (group.builtIn ? "1" : "0") + "\n";
    }
    for (const auto& column : value.columns) {
        out += "C\t" + encoded(column.id) + "\t" + encoded(column.label) + "\t" + encoded(column.displayFormat)
            + "\t" + encoded(column.secondaryDisplayFormat) + "\t" + encoded(column.sortFormat)
            + "\t" + std::to_string(static_cast<int>(column.alignment)) + "\t"
            + (column.visible ? "1" : "0") + "\t" + std::to_string(column.widthWeight) + "\t"
            + (column.builtIn ? "1" : "0") + "\t" + (column.cover ? "1" : "0") + "\n";
    }
    for (const auto& profile : value.profiles) {
        out += "P\t" + encoded(profile.id) + "\t" + encoded(profile.label) + "\t" + encoded(profile.groupId)
            + "\t" + std::to_string(static_cast<int>(profile.trackRowLayout))
            + "\t" + std::to_string(static_cast<int>(profile.groupHeaderStyle))
            + "\t" + (profile.autoCollapse ? "1" : "0") + "\t" + (profile.collapseByDefault ? "1" : "0")
            + "\t" + (profile.builtIn ? "1" : "0") + "\n";
        for (const auto& column : profile.columns) out += "PC\t" + encoded(profile.id) + "\t"
            + encoded(column.columnId) + "\t" + (column.visible ? "1" : "0") + "\t"
            + std::to_string(column.widthWeight) + "\n";
    }
    for (const auto& assignment : value.assignments) out += "A\t" + encoded(assignment.playlistGuid)
        + "\t" + encoded(assignment.profileId) + "\n";
    return out;
}

[[nodiscard]] inline PlaylistViewSettings deserializePlaylistViewSettings(std::string_view text) {
    const auto version2 = text.starts_with("RPV2\n");
    if (!version2 && !text.starts_with("RPV1\n")) return defaultPlaylistViewSettings();
    PlaylistViewSettings value;
    value.groups.clear(); value.columns.clear();
    for (const auto line : playlist_config_detail::split(text.substr(5), '\n')) {
        const auto fields = playlist_config_detail::split(line, '\t');
        if (fields.empty()) continue;
        if (fields[0] == "S" && fields.size() == 4) {
            if (!playlist_config_detail::decode(fields[1], value.activeGroupId)) continue;
            value.autoCollapse = fields[2] == "1"; value.collapseByDefault = fields[3] == "1";
        } else if (fields[0] == "G" && fields.size() == 11) {
            GroupDefinition group;
            if (!playlist_config_detail::decode(fields[1], group.id)
                || !playlist_config_detail::decode(fields[2], group.label)
                || !playlist_config_detail::decode(fields[3], group.keyFormat)
                || !playlist_config_detail::decode(fields[4], group.sortFormat)
                || !playlist_config_detail::decode(fields[5], group.leftPrimary)
                || !playlist_config_detail::decode(fields[6], group.rightPrimary)
                || !playlist_config_detail::decode(fields[7], group.leftSecondary)
                || !playlist_config_detail::decode(fields[8], group.rightSecondary)) continue;
            int artwork{};
            if (!playlist_config_detail::number(fields[9], artwork)) continue;
            group.artwork = static_cast<GroupArtworkSource>(artwork); group.builtIn = fields[10] == "1";
            value.groups.push_back(std::move(group));
        } else if (fields[0] == "C" && ((!version2 && fields.size() == 10) || (version2 && fields.size() == 11))) {
            ColumnDefinition column;
            if (!playlist_config_detail::decode(fields[1], column.id)
                || !playlist_config_detail::decode(fields[2], column.label)
                || !playlist_config_detail::decode(fields[3], column.displayFormat)) continue;
            const auto sortIndex = version2 ? 5U : 4U;
            if (version2 && !playlist_config_detail::decode(fields[4], column.secondaryDisplayFormat)) continue;
            if (!playlist_config_detail::decode(fields[sortIndex], column.sortFormat)) continue;
            int alignment{}; int width{};
            if (!playlist_config_detail::number(fields[sortIndex + 1], alignment)
                || !playlist_config_detail::number(fields[sortIndex + 3], width)) continue;
            column.alignment = static_cast<ColumnAlignment>(alignment); column.visible = fields[sortIndex + 2] == "1";
            column.widthWeight = width; column.builtIn = fields[sortIndex + 4] == "1"; column.cover = fields[sortIndex + 5] == "1";
            value.columns.push_back(std::move(column));
        } else if (version2 && fields[0] == "P" && fields.size() == 9) {
            LayoutProfile profile;
            if (!playlist_config_detail::decode(fields[1], profile.id)
                || !playlist_config_detail::decode(fields[2], profile.label)
                || !playlist_config_detail::decode(fields[3], profile.groupId)) continue;
            int row{}, header{};
            if (!playlist_config_detail::number(fields[4], row)
                || !playlist_config_detail::number(fields[5], header)) continue;
            profile.trackRowLayout = static_cast<TrackRowLayout>(row);
            profile.groupHeaderStyle = static_cast<GroupHeaderStyle>(header);
            profile.autoCollapse = fields[6] == "1"; profile.collapseByDefault = fields[7] == "1";
            profile.builtIn = fields[8] == "1"; value.profiles.push_back(std::move(profile));
        } else if (version2 && fields[0] == "PC" && fields.size() == 5) {
            std::string profileId; ProfileColumn column;
            if (!playlist_config_detail::decode(fields[1], profileId)
                || !playlist_config_detail::decode(fields[2], column.columnId)
                || !playlist_config_detail::number(fields[4], column.widthWeight)) continue;
            column.visible = fields[3] == "1";
            const auto profile = std::find_if(value.profiles.begin(), value.profiles.end(), [&](const auto& candidate) {
                return candidate.id == profileId;
            });
            if (profile != value.profiles.end()) profile->columns.push_back(std::move(column));
        } else if (version2 && fields[0] == "A" && fields.size() == 3) {
            PlaylistProfileAssignment assignment;
            if (playlist_config_detail::decode(fields[1], assignment.playlistGuid)
                && playlist_config_detail::decode(fields[2], assignment.profileId))
                value.assignments.push_back(std::move(assignment));
        }
    }
    return normalizePlaylistViewSettings(std::move(value));
}

} // namespace refrain
