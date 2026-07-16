#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace foocrate {

struct AlbumSourceTrack {
    std::size_t sourceIndex{};
    std::string album;
    std::string albumArtist;
    std::string title;
    std::string artist;
    std::string date;
    unsigned disc{};
    unsigned track{};
};

struct AlbumBrowserAlbum {
    std::string key;
    std::string album;
    std::string albumArtist;
    std::string date;
    std::vector<std::size_t> sourceIndices;
};

[[nodiscard]] inline std::string normalizeAlbumIdentityPart(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    bool pendingSpace{};
    for (const auto raw : value) {
        const auto character = static_cast<unsigned char>(raw);
        if (std::isspace(character)) {
            pendingSpace = !result.empty();
            continue;
        }
        if (pendingSpace) result.push_back(' ');
        pendingSpace = false;
        result.push_back(static_cast<char>(std::tolower(character)));
    }
    return result;
}

[[nodiscard]] inline std::string albumIdentityKey(
    const AlbumSourceTrack& track, std::size_t inputPosition) {
    const auto album = normalizeAlbumIdentityPart(track.album);
    if (album.empty()) {
        return std::string("single\x1f") + std::to_string(inputPosition);
    }
    return album + '\x1f' + normalizeAlbumIdentityPart(track.albumArtist);
}

[[nodiscard]] inline std::vector<AlbumBrowserAlbum> buildAlbumBrowserAlbums(
    const std::vector<AlbumSourceTrack>& tracks) {
    std::vector<AlbumBrowserAlbum> albums;
    albums.reserve(tracks.size());
    std::unordered_map<std::string, std::size_t> albumIndices;
    albumIndices.reserve(tracks.size());
    for (std::size_t position = 0; position < tracks.size(); ++position) {
        const auto& track = tracks[position];
        const auto key = albumIdentityKey(track, position);
        const auto found = albumIndices.find(key);
        if (found == albumIndices.end()) {
            albumIndices.emplace(key, albums.size());
            albums.push_back({key, track.album.empty() ? "Single" : track.album,
                track.albumArtist.empty() ? track.artist : track.albumArtist,
                track.date, {track.sourceIndex}});
        } else {
            auto& album = albums[found->second];
            album.sourceIndices.push_back(track.sourceIndex);
            if (album.date.empty()) album.date = track.date;
        }
    }

    std::unordered_map<std::size_t, const AlbumSourceTrack*> sourceTracks;
    sourceTracks.reserve(tracks.size());
    for (const auto& track : tracks) sourceTracks.emplace(track.sourceIndex, &track);
    const auto trackAt = [&](std::size_t sourceIndex) -> const AlbumSourceTrack& {
        const auto found = sourceTracks.find(sourceIndex);
        return found != sourceTracks.end() ? *found->second : tracks.front();
    };
    for (auto& album : albums) {
        std::stable_sort(album.sourceIndices.begin(), album.sourceIndices.end(), [&](const auto left, const auto right) {
            const auto& a = trackAt(left);
            const auto& b = trackAt(right);
            const auto aDisc = a.disc == 0 ? std::numeric_limits<unsigned>::max() : a.disc;
            const auto bDisc = b.disc == 0 ? std::numeric_limits<unsigned>::max() : b.disc;
            const auto aTrack = a.track == 0 ? std::numeric_limits<unsigned>::max() : a.track;
            const auto bTrack = b.track == 0 ? std::numeric_limits<unsigned>::max() : b.track;
            if (aDisc != bDisc) return aDisc < bDisc;
            if (aTrack != bTrack) return aTrack < bTrack;
            return normalizeAlbumIdentityPart(a.title) < normalizeAlbumIdentityPart(b.title);
        });
    }
    std::stable_sort(albums.begin(), albums.end(), [](const auto& a, const auto& b) {
        const auto aAlbum = normalizeAlbumIdentityPart(a.album);
        const auto bAlbum = normalizeAlbumIdentityPart(b.album);
        if (aAlbum != bAlbum) return aAlbum < bAlbum;
        const auto aArtist = normalizeAlbumIdentityPart(a.albumArtist);
        const auto bArtist = normalizeAlbumIdentityPart(b.albumArtist);
        if (aArtist != bArtist) return aArtist < bArtist;
        return a.date < b.date;
    });
    return albums;
}

} // namespace foocrate
