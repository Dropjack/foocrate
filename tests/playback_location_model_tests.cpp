#include "playback_location_model.h"

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    using foocrate::restoredPlaylistTopRow;
    expect(restoredPlaylistTopRow(0, 4, 100, 10) == 0, "anchor before first row must clamp to zero");
    expect(restoredPlaylistTopRow(20, 4, 100, 10) == 16, "saved viewport anchor must be restored");
    expect(restoredPlaylistTopRow(99, 4, 100, 10) == 90, "last row must clamp to maximum top row");
    expect(restoredPlaylistTopRow(3, 1, 5, 10) == 0, "short playlist must remain at top");
    expect(restoredPlaylistTopRow(3, 1, 0, 10) == 0, "empty playlist must remain at top");
    expect(restoredPlaylistTopRow(3, 1, 10, 0) == 0, "zero capacity must remain safe");
    return 0;
}
