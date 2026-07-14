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
    expect(defaults.lyricsAutoSwitch, "lyrics auto switch must default on");
    expect(defaults.lowerRightView == LowerRightView::lyrics, "lower right view must default to lyrics");
    expect(!defaults.showReplayGain, "ReplayGain details must default off");
    expect(defaults.rightHeaderPermille == 500, "right header must default to half height");
    expect(defaults.rightColumnPermille == 230, "right column must default to 23 percent");

    const auto firstRun = migrateSettings({0, 0, true, true, true, 0, false, 500, 230});
    expect(firstRun.versionToKeep == 3, "version zero must migrate to three");
    expect(firstRun.rewriteKnownValues, "version migration must be persisted");

    const auto invalid = migrateSettings({3, 77, false, false, false, 77, true, 999, 999});
    expect(invalid.values.timeDisplay == TimeDisplayMode::total, "invalid enum must fall back to total");
    expect(invalid.rewriteKnownValues, "invalid current value must be repaired");
    expect(!invalid.values.showTooltips && !invalid.values.showSettingsButton, "valid booleans must survive repair");

    expect(invalid.values.lowerRightView == LowerRightView::lyrics, "invalid lower view must fall back");
    expect(invalid.values.rightHeaderPermille == 500, "invalid divider must fall back");
    expect(invalid.values.rightColumnPermille == 230, "invalid right column must fall back");

    const auto remaining = migrateSettings({3, 1, true, false, false, 1, true, 640, 360});
    expect(remaining.values.timeDisplay == TimeDisplayMode::remaining, "remaining mode must survive");
    expect(!remaining.rewriteKnownValues, "valid current values must not be rewritten");
    expect(remaining.values.lowerRightView == LowerRightView::trackDetails, "details view must survive");
    expect(remaining.values.rightHeaderPermille == 640, "divider position must survive");
    expect(remaining.values.rightColumnPermille == 360, "right column position must survive");

    const auto future = migrateSettings({9, 77, false, true, true, 77, false, 999, 999});
    expect(future.versionToKeep == 9, "future version must not be downgraded");
    expect(!future.rewriteKnownValues, "future configuration must not be rewritten by old code");
    expect(future.values.timeDisplay == TimeDisplayMode::total, "future invalid enum needs a safe runtime fallback");

    return failures == 0 ? 0 : 1;
}
