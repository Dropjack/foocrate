#pragma once

#include <cstddef>

namespace foocrate {

enum class PlaybackOrderIconKind {
    defaultOrder,
    repeatPlaylist,
    repeatTrack,
    random,
    shuffleTracks,
    shuffleAlbums,
    shuffleFolders,
    other,
};

[[nodiscard]] constexpr PlaybackOrderIconKind playbackOrderIconKind(std::size_t index) noexcept {
    switch (index) {
    case 0: return PlaybackOrderIconKind::defaultOrder;
    case 1: return PlaybackOrderIconKind::repeatPlaylist;
    case 2: return PlaybackOrderIconKind::repeatTrack;
    case 3: return PlaybackOrderIconKind::random;
    case 4: return PlaybackOrderIconKind::shuffleTracks;
    case 5: return PlaybackOrderIconKind::shuffleAlbums;
    case 6: return PlaybackOrderIconKind::shuffleFolders;
    default: return PlaybackOrderIconKind::other;
    }
}

} // namespace foocrate
