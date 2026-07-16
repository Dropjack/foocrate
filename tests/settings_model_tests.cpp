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
    using namespace refrain;

    const auto defaults = defaultSettings();
    expect(defaults.timeDisplay == TimeDisplayMode::total, "default time mode must be total");
    expect(defaults.showTooltips, "tooltips must default on");
    expect(defaults.showSettingsButton, "settings button must default on");
    expect(defaults.restorePlaylistView, "playlist view restore must default on");
    expect(defaults.lyricsAutoSwitch, "lyrics auto switch must default on");
    expect(defaults.lowerRightView == LowerRightView::lyrics, "lower right view must default to lyrics");
    expect(!defaults.showReplayGain, "ReplayGain details must default off");
    expect(defaults.rightHeaderPermille == 500, "right header must default to half height");
    expect(defaults.rightColumnPermille == 230, "right column must default to 23 percent");
    expect(defaults.themePreset == ThemePreset::mist, "theme must default to mist");
    expect(defaults.colourMode == ColourMode::refrainPreset, "colour mode must default to Refrain preset");

    const auto firstRun = migrateSettings({0, 0, true, true, true, 0, false, 500, 230});
    expect(firstRun.versionToKeep == 7, "version zero must migrate to seven");
    expect(firstRun.rewriteKnownValues, "version migration must be persisted");
    expect(firstRun.values.restorePlaylistView, "old settings must enable playlist view restore");

    const auto invalid = migrateSettings({kCurrentSettingsVersion, 77, false, false, false, 77, true, 999, 999, 77, 77});
    expect(invalid.values.timeDisplay == TimeDisplayMode::total, "invalid enum must fall back to total");
    expect(invalid.rewriteKnownValues, "invalid current value must be repaired");
    expect(!invalid.values.showTooltips && !invalid.values.showSettingsButton, "valid booleans must survive repair");

    expect(invalid.values.lowerRightView == LowerRightView::lyrics, "invalid lower view must fall back");
    expect(invalid.values.rightHeaderPermille == 500, "invalid divider must fall back");
    expect(invalid.values.rightColumnPermille == 230, "invalid right column must fall back");
    expect(invalid.values.themePreset == ThemePreset::mist, "invalid theme must fall back to mist");
    expect(invalid.values.colourMode == ColourMode::refrainPreset, "invalid colour mode must fall back");

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
    expect(upgradedRestore.values.restorePlaylistView,
        "version-six upgrade must enable playlist view restore by default");
    expect(upgradedRestore.rewriteKnownValues,
        "version-six upgrade must persist the new restore setting");

    const auto oldRefrain = migrateSettings({4, 0, true, true, true, 0, false, 500, 230, 0, 0});
    const auto oldWindows = migrateSettings({4, 0, true, true, true, 0, false, 500, 230, 0, 1});
    const auto oldColumns = migrateSettings({4, 0, true, true, true, 0, false, 500, 230, 0, 2});
    expect(oldRefrain.values.colourMode == ColourMode::refrainPreset,
        "version-four Refrain source must map to Refrain preset");
    expect(oldWindows.values.colourMode == ColourMode::windows,
        "version-four Windows source must map to Windows");
    expect(oldColumns.values.colourMode == ColourMode::refrainPreset,
        "removed Columns UI source must safely map to Refrain preset");

    const auto future = migrateSettings({9, 77, false, true, true, 77, false, 999, 999});
    expect(future.versionToKeep == 9, "future version must not be downgraded");
    expect(!future.rewriteKnownValues, "future configuration must not be rewritten by old code");
    expect(future.values.timeDisplay == TimeDisplayMode::total, "future invalid enum needs a safe runtime fallback");

    return failures == 0 ? 0 : 1;
}
