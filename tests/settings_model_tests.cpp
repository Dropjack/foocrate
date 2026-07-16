#include "settings_model.h"

#include <iostream>

namespace {

int failures{};

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    using namespace foocrate;

    const auto defaults = defaultSettings();
    expect(defaults.timeDisplay == TimeDisplayMode::total, "default time mode must be total");
    expect(defaults.showTooltips, "tooltips must default on");
    expect(defaults.showSettingsButton, "settings button must default on");
    expect(defaults.startupBehavior == StartupBehavior::startAtHome,
        "startup must default to the home screen");
    expect(defaults.lyricsAutoSwitch, "lyrics auto switch must default on");
    expect(defaults.lowerRightView == LowerRightView::lyrics, "lower right view must default to lyrics");
    expect(!defaults.showReplayGain, "ReplayGain details must default off");
    expect(defaults.rightHeaderPermille == 500, "right header must default to half height");
    expect(defaults.rightColumnPermille == 230, "right column must default to 23 percent");
    expect(defaults.themePreset == ThemePreset::mist, "theme must default to mist");
    expect(defaults.colourMode == ColourMode::foocratePreset, "colour mode must default to FooCrate preset");

    const auto firstRun = migrateSettings({0, 0, true, true, true, 0, false, 500, 230});
    expect(firstRun.versionToKeep == 8, "version zero must migrate to eight");
    expect(firstRun.rewriteKnownValues, "version migration must be persisted");
    expect(firstRun.values.startupBehavior == StartupBehavior::startAtHome,
        "first run must remain stopped at home");

    const auto invalid = migrateSettings({kCurrentSettingsVersion, 77, false, false, false, 77, true, 999, 999, 77, 77});
    expect(invalid.values.timeDisplay == TimeDisplayMode::total, "invalid enum must fall back to total");
    expect(invalid.rewriteKnownValues, "invalid current value must be repaired");
    expect(!invalid.values.showTooltips && !invalid.values.showSettingsButton, "valid booleans must survive repair");

    expect(invalid.values.lowerRightView == LowerRightView::lyrics, "invalid lower view must fall back");
    expect(invalid.values.rightHeaderPermille == 500, "invalid divider must fall back");
    expect(invalid.values.rightColumnPermille == 230, "invalid right column must fall back");
    expect(invalid.values.themePreset == ThemePreset::mist, "invalid theme must fall back to mist");
    expect(invalid.values.colourMode == ColourMode::foocratePreset, "invalid colour mode must fall back");

    const auto remaining = migrateSettings({kCurrentSettingsVersion, 1, true, false, false, 1, true, 640, 360, 3, 2});
    expect(remaining.values.timeDisplay == TimeDisplayMode::remaining, "remaining mode must survive");
    expect(!remaining.rewriteKnownValues, "valid current values must not be rewritten");
    expect(remaining.values.lowerRightView == LowerRightView::trackDetails, "details view must survive");
    expect(remaining.values.rightHeaderPermille == 640, "divider position must survive");
    expect(remaining.values.rightColumnPermille == 360, "right column position must survive");
    expect(remaining.values.themePreset == ThemePreset::ink, "valid theme must survive");
    expect(remaining.values.colourMode == ColourMode::albumArtwork, "valid colour mode must survive");
    expect(remaining.values.trackActivation == TrackActivationAction::play,
        "track activation must safely default to Play");
    expect(remaining.values.rightPanelFollow == RightPanelFollow::playingTrack,
        "right panel must safely default to playing track");
    expect(remaining.values.artworkSourceMask == kAllArtworkSources,
        "all artwork sources must default on");

    auto versionSix = StoredSettings{};
    versionSix.version = 6;
    versionSix.restorePlaylistView = false;
    const auto upgradedRestore = migrateSettings(versionSix);
    expect(upgradedRestore.values.startupBehavior == StartupBehavior::restoreLastTrack,
        "version-six upgrade must preserve playlist view restore");
    expect(upgradedRestore.rewriteKnownValues,
        "version-six upgrade must persist the new restore setting");

    auto versionSevenDisabled = StoredSettings{};
    versionSevenDisabled.version = 7;
    versionSevenDisabled.restorePlaylistView = false;
    const auto upgradedHome = migrateSettings(versionSevenDisabled);
    expect(upgradedHome.values.startupBehavior == StartupBehavior::startAtHome,
        "disabled legacy restore must migrate to home");

    auto currentResume = StoredSettings{};
    currentResume.version = kCurrentSettingsVersion;
    currentResume.startupBehavior = static_cast<std::int64_t>(StartupBehavior::resumePlayback);
    const auto preservedResume = migrateSettings(currentResume);
    expect(preservedResume.values.startupBehavior == StartupBehavior::resumePlayback,
        "resume playback must survive current settings");

    const auto oldFooCrate = migrateSettings({4, 0, true, true, true, 0, false, 500, 230, 0, 0});
    const auto oldWindows = migrateSettings({4, 0, true, true, true, 0, false, 500, 230, 0, 1});
    const auto oldColumns = migrateSettings({4, 0, true, true, true, 0, false, 500, 230, 0, 2});
    expect(oldFooCrate.values.colourMode == ColourMode::foocratePreset,
        "version-four FooCrate source must map to FooCrate preset");
    expect(oldWindows.values.colourMode == ColourMode::windows,
        "version-four Windows source must map to Windows");
    expect(oldColumns.values.colourMode == ColourMode::foocratePreset,
        "removed Columns UI source must safely map to FooCrate preset");

    const auto future = migrateSettings({9, 77, false, true, true, 77, false, 999, 999});
    expect(future.versionToKeep == 9, "future version must not be downgraded");
    expect(!future.rewriteKnownValues, "future configuration must not be rewritten by old code");
    expect(future.values.timeDisplay == TimeDisplayMode::total, "future invalid enum needs a safe runtime fallback");

    return failures == 0 ? 0 : 1;
}
