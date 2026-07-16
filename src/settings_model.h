#pragma once

#include "theme_model.h"

#include <cstdint>
#include <string>
#include <utility>

namespace foocrate {

enum class TimeDisplayMode : std::int64_t {
    total = 0,
    remaining = 1,
};

enum class LowerRightView : std::int64_t {
    lyrics = 0,
    trackDetails = 1,
};

enum class TrackActivationAction : std::int64_t { play = 0, addToQueue = 1 };
enum class RightPanelFollow : std::int64_t { playingTrack = 0, playlistSelection = 1 };
enum class AlbumTileSize : std::int64_t { smallTile = 0, medium = 1, largeTile = 2 };
enum class StartupBehavior : std::int64_t {
    resumePlayback = 0,
    restoreLastTrack = 1,
    startAtHome = 2,
};

inline constexpr std::int64_t kArtworkFront = 1 << 0;
inline constexpr std::int64_t kArtworkBack = 1 << 1;
inline constexpr std::int64_t kArtworkDisc = 1 << 2;
inline constexpr std::int64_t kArtworkArtist = 1 << 3;
inline constexpr std::int64_t kAllArtworkSources =
    kArtworkFront | kArtworkBack | kArtworkDisc | kArtworkArtist;

struct SettingsValues {
    TimeDisplayMode timeDisplay{TimeDisplayMode::total};
    bool showTooltips{true};
    bool showSettingsButton{true};
    bool lyricsAutoSwitch{true};
    LowerRightView lowerRightView{LowerRightView::lyrics};
    bool showReplayGain{};
    std::int64_t rightHeaderPermille{500};
    std::int64_t rightColumnPermille{230};
    ThemePreset themePreset{ThemePreset::mist};
    ColourMode colourMode{ColourMode::foocratePreset};
    TrackActivationAction trackActivation{TrackActivationAction::play};
    StartupBehavior startupBehavior{StartupBehavior::startAtHome};
    RightPanelFollow rightPanelFollow{RightPanelFollow::playingTrack};
    bool detailsMetadata{true};
    bool detailsLocation{true};
    bool detailsTechnical{true};
    bool detailsPlaybackStatistics{true};
    bool detailsReplayGain{};
    bool artworkRotationEnabled{true};
    std::int64_t artworkSourceMask{kAllArtworkSources};
    std::int64_t artworkRotationSeconds{20};
    AlbumTileSize albumTileSize{AlbumTileSize::medium};
    std::string nowPlayingSummaryFormat{"$if2(%title%,$filename_ext(%path%))\n$if2(%artist%,'Unknown artist') | $if2(%album%,'Unknown album')"};

    bool operator==(const SettingsValues&) const = default;
};

struct StoredSettings {
    std::int64_t version{};
    std::int64_t timeDisplay{};
    bool showTooltips{true};
    bool showSettingsButton{true};
    bool lyricsAutoSwitch{true};
    std::int64_t lowerRightView{};
    bool showReplayGain{};
    std::int64_t rightHeaderPermille{500};
    std::int64_t rightColumnPermille{230};
    std::int64_t themePreset{};
    std::int64_t colourMode{};
    std::int64_t trackActivation{};
    bool restorePlaylistView{true};
    std::int64_t rightPanelFollow{};
    bool detailsMetadata{true};
    bool detailsLocation{true};
    bool detailsTechnical{true};
    bool detailsPlaybackStatistics{true};
    bool detailsReplayGain{};
    bool artworkRotationEnabled{true};
    std::int64_t artworkSourceMask{kAllArtworkSources};
    std::int64_t artworkRotationSeconds{20};
    std::int64_t albumTileSize{static_cast<std::int64_t>(AlbumTileSize::medium)};
    std::string nowPlayingSummaryFormat{"$if2(%title%,$filename_ext(%path%))\n$if2(%artist%,'Unknown artist') | $if2(%album%,'Unknown album')"};
    std::int64_t startupBehavior{static_cast<std::int64_t>(StartupBehavior::startAtHome)};
};

struct SettingsMigration {
    SettingsValues values{};
    std::int64_t versionToKeep{};
    bool rewriteKnownValues{};
};

inline constexpr std::int64_t kCurrentSettingsVersion = 8;

[[nodiscard]] inline SettingsValues defaultSettings() {
    return {};
}

[[nodiscard]] constexpr bool isValidTrackActivation(std::int64_t value) noexcept {
    return value >= 0 && value <= 1;
}

[[nodiscard]] constexpr bool isValidRightPanelFollow(std::int64_t value) noexcept {
    return value >= 0 && value <= 1;
}

[[nodiscard]] constexpr bool isValidAlbumTileSize(std::int64_t value) noexcept {
    return value >= 0 && value <= 2;
}

[[nodiscard]] constexpr bool isValidStartupBehavior(std::int64_t value) noexcept {
    return value >= 0 && value <= 2;
}

[[nodiscard]] constexpr bool isValidArtworkSourceMask(std::int64_t value) noexcept {
    return value > 0 && (value & ~kAllArtworkSources) == 0;
}

[[nodiscard]] constexpr bool isValidArtworkRotationSeconds(std::int64_t value) noexcept {
    return value == 10 || value == 20 || value == 30 || value == 60;
}

[[nodiscard]] constexpr bool isValidTimeDisplay(std::int64_t value) noexcept {
    return value == static_cast<std::int64_t>(TimeDisplayMode::total)
        || value == static_cast<std::int64_t>(TimeDisplayMode::remaining);
}

[[nodiscard]] constexpr bool isValidLowerRightView(std::int64_t value) noexcept {
    return value == static_cast<std::int64_t>(LowerRightView::lyrics)
        || value == static_cast<std::int64_t>(LowerRightView::trackDetails);
}

[[nodiscard]] constexpr bool isValidRightHeaderPermille(std::int64_t value) noexcept {
    return value >= 250 && value <= 800;
}

[[nodiscard]] constexpr bool isValidRightColumnPermille(std::int64_t value) noexcept {
    return value >= 100 && value <= 800;
}

[[nodiscard]] inline SettingsMigration migrateSettings(StoredSettings stored) {
    SettingsMigration result;
    result.values.timeDisplay = isValidTimeDisplay(stored.timeDisplay)
        ? static_cast<TimeDisplayMode>(stored.timeDisplay)
        : TimeDisplayMode::total;
    result.values.showTooltips = stored.showTooltips;
    result.values.showSettingsButton = stored.showSettingsButton;
    result.values.lyricsAutoSwitch = stored.lyricsAutoSwitch;
    result.values.lowerRightView = isValidLowerRightView(stored.lowerRightView)
        ? static_cast<LowerRightView>(stored.lowerRightView)
        : LowerRightView::lyrics;
    result.values.showReplayGain = stored.showReplayGain;
    result.values.rightHeaderPermille = isValidRightHeaderPermille(stored.rightHeaderPermille)
        ? stored.rightHeaderPermille : 500;
    result.values.rightColumnPermille = isValidRightColumnPermille(stored.rightColumnPermille)
        ? stored.rightColumnPermille : 230;
    result.values.themePreset = isValidThemePreset(stored.themePreset)
        ? static_cast<ThemePreset>(stored.themePreset) : ThemePreset::mist;
    if (stored.version <= 4) {
        // Version 4 used FooCrate=0, Windows=1 and Columns UI=2. Columns UI is removed;
        // its old value safely returns to the FooCrate preset instead of changing meaning.
        result.values.colourMode = stored.colourMode == 1
            ? ColourMode::windows : ColourMode::foocratePreset;
    } else {
        result.values.colourMode = isValidColourMode(stored.colourMode)
            ? static_cast<ColourMode>(stored.colourMode) : ColourMode::foocratePreset;
    }
    result.values.trackActivation = isValidTrackActivation(stored.trackActivation)
        ? static_cast<TrackActivationAction>(stored.trackActivation) : TrackActivationAction::play;
    if (stored.version == 0) {
        result.values.startupBehavior = StartupBehavior::startAtHome;
    } else if (stored.version <= 7) {
        const auto legacyRestore = stored.version <= 6 ? true : stored.restorePlaylistView;
        result.values.startupBehavior = legacyRestore
            ? StartupBehavior::restoreLastTrack : StartupBehavior::startAtHome;
    } else {
        result.values.startupBehavior = isValidStartupBehavior(stored.startupBehavior)
            ? static_cast<StartupBehavior>(stored.startupBehavior) : StartupBehavior::startAtHome;
    }
    result.values.rightPanelFollow = isValidRightPanelFollow(stored.rightPanelFollow)
        ? static_cast<RightPanelFollow>(stored.rightPanelFollow) : RightPanelFollow::playingTrack;
    result.values.detailsMetadata = stored.detailsMetadata;
    result.values.detailsLocation = stored.detailsLocation;
    result.values.detailsTechnical = stored.detailsTechnical;
    result.values.detailsPlaybackStatistics = stored.detailsPlaybackStatistics;
    // Version 5 only exposed ReplayGain. Preserve that value during the section migration.
    result.values.detailsReplayGain = stored.version <= 5 ? stored.showReplayGain : stored.detailsReplayGain;
    result.values.showReplayGain = result.values.detailsReplayGain;
    result.values.artworkRotationEnabled = stored.artworkRotationEnabled;
    result.values.artworkSourceMask = isValidArtworkSourceMask(stored.artworkSourceMask)
        ? stored.artworkSourceMask : kAllArtworkSources;
    result.values.artworkRotationSeconds = isValidArtworkRotationSeconds(stored.artworkRotationSeconds)
        ? stored.artworkRotationSeconds : 20;
    result.values.albumTileSize = isValidAlbumTileSize(stored.albumTileSize)
        ? static_cast<AlbumTileSize>(stored.albumTileSize) : AlbumTileSize::medium;
    result.values.nowPlayingSummaryFormat = stored.nowPlayingSummaryFormat.empty()
        ? "$if2(%title%,$filename_ext(%path%))\n$if2(%artist%,'Unknown artist') | $if2(%album%,'Unknown album')"
        : stored.nowPlayingSummaryFormat;

    if (stored.version <= kCurrentSettingsVersion) {
        result.versionToKeep = kCurrentSettingsVersion;
        result.rewriteKnownValues = stored.version != kCurrentSettingsVersion
            || !isValidTimeDisplay(stored.timeDisplay)
            || !isValidLowerRightView(stored.lowerRightView)
            || !isValidRightHeaderPermille(stored.rightHeaderPermille)
            || !isValidRightColumnPermille(stored.rightColumnPermille)
            || !isValidThemePreset(stored.themePreset)
            || (stored.version >= 5 && !isValidColourMode(stored.colourMode))
            || !isValidTrackActivation(stored.trackActivation)
            || (stored.version >= 8 && !isValidStartupBehavior(stored.startupBehavior))
            || !isValidRightPanelFollow(stored.rightPanelFollow)
            || !isValidArtworkSourceMask(stored.artworkSourceMask)
            || !isValidArtworkRotationSeconds(stored.artworkRotationSeconds)
            || !isValidAlbumTileSize(stored.albumTileSize)
            || stored.nowPlayingSummaryFormat.empty();
    } else {
        result.versionToKeep = stored.version;
        result.rewriteKnownValues = false;
    }
    return result;
}

} // namespace foocrate
