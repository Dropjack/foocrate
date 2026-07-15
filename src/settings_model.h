#pragma once

#include "theme_model.h"

#include <cstdint>

namespace refrain {

enum class TimeDisplayMode : std::int64_t {
    total = 0,
    remaining = 1,
};

enum class LowerRightView : std::int64_t {
    lyrics = 0,
    trackDetails = 1,
};

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
    ColourMode colourMode{ColourMode::refrainPreset};

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
};

struct SettingsMigration {
    SettingsValues values{};
    std::int64_t versionToKeep{};
    bool rewriteKnownValues{};
};

inline constexpr std::int64_t kCurrentSettingsVersion = 5;

[[nodiscard]] constexpr SettingsValues defaultSettings() noexcept {
    return {};
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

[[nodiscard]] constexpr SettingsMigration migrateSettings(StoredSettings stored) noexcept {
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
        // Version 4 used Refrain=0, Windows=1 and Columns UI=2. Columns UI is removed;
        // its old value safely returns to the Refrain preset instead of changing meaning.
        result.values.colourMode = stored.colourMode == 1
            ? ColourMode::windows : ColourMode::refrainPreset;
    } else {
        result.values.colourMode = isValidColourMode(stored.colourMode)
            ? static_cast<ColourMode>(stored.colourMode) : ColourMode::refrainPreset;
    }

    if (stored.version <= kCurrentSettingsVersion) {
        result.versionToKeep = kCurrentSettingsVersion;
        result.rewriteKnownValues = stored.version != kCurrentSettingsVersion
            || !isValidTimeDisplay(stored.timeDisplay)
            || !isValidLowerRightView(stored.lowerRightView)
            || !isValidRightHeaderPermille(stored.rightHeaderPermille)
            || !isValidRightColumnPermille(stored.rightColumnPermille)
            || !isValidThemePreset(stored.themePreset)
            || (stored.version >= 5 && !isValidColourMode(stored.colourMode));
    } else {
        result.versionToKeep = stored.version;
        result.rewriteKnownValues = false;
    }
    return result;
}

} // namespace refrain
