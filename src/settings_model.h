#pragma once

#include <cstdint>

namespace refrain {

enum class TimeDisplayMode : std::int64_t {
    total = 0,
    remaining = 1,
};

struct SettingsValues {
    TimeDisplayMode timeDisplay{TimeDisplayMode::total};
    bool showTooltips{true};
    bool showSettingsButton{true};

    bool operator==(const SettingsValues&) const = default;
};

struct StoredSettings {
    std::int64_t version{};
    std::int64_t timeDisplay{};
    bool showTooltips{true};
    bool showSettingsButton{true};
};

struct SettingsMigration {
    SettingsValues values{};
    std::int64_t versionToKeep{};
    bool rewriteKnownValues{};
};

inline constexpr std::int64_t kCurrentSettingsVersion = 1;

[[nodiscard]] constexpr SettingsValues defaultSettings() noexcept {
    return {};
}

[[nodiscard]] constexpr bool isValidTimeDisplay(std::int64_t value) noexcept {
    return value == static_cast<std::int64_t>(TimeDisplayMode::total)
        || value == static_cast<std::int64_t>(TimeDisplayMode::remaining);
}

[[nodiscard]] constexpr SettingsMigration migrateSettings(StoredSettings stored) noexcept {
    SettingsMigration result;
    result.values.timeDisplay = isValidTimeDisplay(stored.timeDisplay)
        ? static_cast<TimeDisplayMode>(stored.timeDisplay)
        : TimeDisplayMode::total;
    result.values.showTooltips = stored.showTooltips;
    result.values.showSettingsButton = stored.showSettingsButton;

    if (stored.version <= kCurrentSettingsVersion) {
        result.versionToKeep = kCurrentSettingsVersion;
        result.rewriteKnownValues = stored.version != kCurrentSettingsVersion
            || !isValidTimeDisplay(stored.timeDisplay);
    } else {
        result.versionToKeep = stored.version;
        result.rewriteKnownValues = false;
    }
    return result;
}

} // namespace refrain
