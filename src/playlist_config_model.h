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
    std::string sortFormat;
    ColumnAlignment alignment{ColumnAlignment::leading};
    bool visible{};
    std::int32_t widthWeight{800};
    bool builtIn{};
    bool cover{};
    bool operator==(const ColumnDefinition&) const = default;
};

struct PlaylistViewSettings {
    std::int32_t version{1};
    std::string activeGroupId{"builtin.album.simple"};
    bool autoCollapse{};
    bool collapseByDefault{};
    std::vector<GroupDefinition> groups;
    std::vector<ColumnDefinition> columns;
    bool operator==(const PlaylistViewSettings&) const = default;
};

inline constexpr std::int32_t kPlaylistViewSettingsVersion = 1;

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
        {"builtin.cover", "Cover", "", "", ColumnAlignment::center, true, 900, true, true},
        {"builtin.state", "State", "", "", ColumnAlignment::center, true, 600, true, false},
        {"builtin.track", "#", "$if(%discnumber%,$num(%discnumber%,1)'.',)$if(%tracknumber%,$num(%tracknumber%,2),%list_index%)",
            "%tracknumber% | %album artist% | %album% | %discnumber% | %title%",
            ColumnAlignment::trailing, true, 650, true, false},
        {"builtin.index", "Index", "$num(%list_index%,$len(%list_total%))", "",
            ColumnAlignment::trailing, false, 650, true, false},
        {"builtin.title", "Title", "$if2(%title%,$filename_ext(%path%))",
            "%title% | %album artist% | %album% | %discnumber% | %tracknumber%",
            ColumnAlignment::leading, true, 2300, true, false},
        {"builtin.artist", "Artist", "[%artist%]", "%artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::leading, true, 1600, true, false},
        {"builtin.albumartist", "Album Artist", "[%album artist%]",
            "%album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::leading, false, 1500, true, false},
        {"builtin.album", "Album", "[%album%]", "%album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::leading, true, 1700, true, false},
        {"builtin.date", "Date", "[%date%]", "%date% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::center, false, 700, true, false},
        {"builtin.genre", "Genre", "[%genre%]", "%genre% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::leading, true, 900, true, false},
        {"builtin.mood", "Mood", "$if(%mood%,1,0)", "%mood% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::center, false, 650, true, false},
        {"builtin.rating", "Rating", "$if2(%rating%,0)", "%rating% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::center, true, 900, true, false},
        {"builtin.plays", "Plays", "$if2(%play_count%,0)", "$if2(%play_count%,0) | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::trailing, false, 650, true, false},
        {"builtin.bitrate", "Bitrate", "[$if2(%__bitrate_dynamic%,%bitrate%)K]",
            "%bitrate% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::trailing, true, 750, true, false},
        {"builtin.codec", "Codec", "[%codec%]", "%codec% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::center, true, 650, true, false},
        {"builtin.samplerate", "Sample Rate", "[$div(%samplerate%,1000) kHz]",
            "%samplerate% | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::trailing, false, 800, true, false},
        {"builtin.length", "Length", "[%length%]", "$if2(%length%,' 0:00') | %album artist% | %album% | %discnumber% | %tracknumber% | %title%",
            ColumnAlignment::trailing, true, 700, true, false},
    };
}

[[nodiscard]] inline PlaylistViewSettings defaultPlaylistViewSettings() {
    PlaylistViewSettings value;
    value.groups = defaultGroupDefinitions();
    value.columns = defaultColumnDefinitions();
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
        playlist_config_detail::limit(column.displayFormat, 2048); playlist_config_detail::limit(column.sortFormat, 2048);
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
            *found = column;
            found->visible = visible;
            found->widthWeight = width;
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
    return value;
}

[[nodiscard]] inline std::string serializePlaylistViewSettings(const PlaylistViewSettings& input) {
    const auto value = normalizePlaylistViewSettings(input);
    auto encoded = [](std::string_view text) { return playlist_config_detail::encode(text); };
    std::string out = "RPV1\nS\t" + encoded(value.activeGroupId) + "\t" + (value.autoCollapse ? "1" : "0")
        + "\t" + (value.collapseByDefault ? "1" : "0") + "\n";
    for (const auto& group : value.groups) {
        out += "G\t" + encoded(group.id) + "\t" + encoded(group.label) + "\t" + encoded(group.keyFormat)
            + "\t" + encoded(group.sortFormat) + "\t" + encoded(group.leftPrimary) + "\t" + encoded(group.rightPrimary)
            + "\t" + encoded(group.leftSecondary) + "\t" + encoded(group.rightSecondary) + "\t"
            + std::to_string(static_cast<int>(group.artwork)) + "\t" + (group.builtIn ? "1" : "0") + "\n";
    }
    for (const auto& column : value.columns) {
        out += "C\t" + encoded(column.id) + "\t" + encoded(column.label) + "\t" + encoded(column.displayFormat)
            + "\t" + encoded(column.sortFormat) + "\t" + std::to_string(static_cast<int>(column.alignment)) + "\t"
            + (column.visible ? "1" : "0") + "\t" + std::to_string(column.widthWeight) + "\t"
            + (column.builtIn ? "1" : "0") + "\t" + (column.cover ? "1" : "0") + "\n";
    }
    return out;
}

[[nodiscard]] inline PlaylistViewSettings deserializePlaylistViewSettings(std::string_view text) {
    if (!text.starts_with("RPV1\n")) return defaultPlaylistViewSettings();
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
        } else if (fields[0] == "C" && fields.size() == 10) {
            ColumnDefinition column;
            if (!playlist_config_detail::decode(fields[1], column.id)
                || !playlist_config_detail::decode(fields[2], column.label)
                || !playlist_config_detail::decode(fields[3], column.displayFormat)
                || !playlist_config_detail::decode(fields[4], column.sortFormat)) continue;
            int alignment{}; int width{};
            if (!playlist_config_detail::number(fields[5], alignment)
                || !playlist_config_detail::number(fields[7], width)) continue;
            column.alignment = static_cast<ColumnAlignment>(alignment); column.visible = fields[6] == "1";
            column.widthWeight = width; column.builtIn = fields[8] == "1"; column.cover = fields[9] == "1";
            value.columns.push_back(std::move(column));
        }
    }
    return normalizePlaylistViewSettings(std::move(value));
}

} // namespace refrain
