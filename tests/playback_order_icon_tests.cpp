#include "playback_order_icon.h"

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
    using foocrate::PlaybackOrderIconKind;
    using foocrate::playbackOrderIconKind;

    expect(playbackOrderIconKind(0) == PlaybackOrderIconKind::defaultOrder, "Default icon mapping failed");
    expect(playbackOrderIconKind(1) == PlaybackOrderIconKind::repeatPlaylist, "Repeat playlist icon mapping failed");
    expect(playbackOrderIconKind(2) == PlaybackOrderIconKind::repeatTrack, "Repeat track icon mapping failed");
    expect(playbackOrderIconKind(3) == PlaybackOrderIconKind::random, "Random icon mapping failed");
    expect(playbackOrderIconKind(4) == PlaybackOrderIconKind::shuffleTracks, "Shuffle tracks icon mapping failed");
    expect(playbackOrderIconKind(5) == PlaybackOrderIconKind::shuffleAlbums, "Shuffle albums icon mapping failed");
    expect(playbackOrderIconKind(6) == PlaybackOrderIconKind::shuffleFolders, "Shuffle folders icon mapping failed");
    expect(playbackOrderIconKind(7) == PlaybackOrderIconKind::other, "Unknown icon fallback failed");
    expect(playbackOrderIconKind(100) == PlaybackOrderIconKind::other, "Large unknown icon fallback failed");
    return 0;
}
