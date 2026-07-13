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

    const auto firstRun = migrateSettings({0, 0, true, true});
    expect(firstRun.versionToKeep == 1, "version zero must migrate to one");
    expect(firstRun.rewriteKnownValues, "version migration must be persisted");

    const auto invalid = migrateSettings({1, 77, false, false});
    expect(invalid.values.timeDisplay == TimeDisplayMode::total, "invalid enum must fall back to total");
    expect(invalid.rewriteKnownValues, "invalid current value must be repaired");
    expect(!invalid.values.showTooltips && !invalid.values.showSettingsButton, "valid booleans must survive repair");

    const auto remaining = migrateSettings({1, 1, true, false});
    expect(remaining.values.timeDisplay == TimeDisplayMode::remaining, "remaining mode must survive");
    expect(!remaining.rewriteKnownValues, "valid current values must not be rewritten");

    const auto future = migrateSettings({9, 77, false, true});
    expect(future.versionToKeep == 9, "future version must not be downgraded");
    expect(!future.rewriteKnownValues, "future configuration must not be rewritten by old code");
    expect(future.values.timeDisplay == TimeDisplayMode::total, "future invalid enum needs a safe runtime fallback");

    return failures == 0 ? 0 : 1;
}
