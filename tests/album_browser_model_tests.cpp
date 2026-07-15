#include "album_browser_model.h"

#include <cassert>

int main() {
    using refrain::AlbumSourceTrack;

    const std::vector<AlbumSourceTrack> tracks{
        {0, "Same", "Artist A", "Second", "Artist A", "2020", 1, 2},
        {1, "same", "artist a", "First", "Artist A", "2020", 1, 1},
        {2, "Same", "Artist B", "Other", "Artist B", "2021", 1, 1},
        {3, "Multi", "Artist A", "Disc two", "Artist A", "2019", 2, 1},
        {4, "Multi", "Artist A", "Disc one", "Artist A", "2019", 1, 1},
        {5, "", "", "Loose one", "Guest", "", 0, 0},
        {6, "", "", "Loose two", "Guest", "", 0, 0},
    };
    const auto albums = refrain::buildAlbumBrowserAlbums(tracks);
    assert(albums.size() == 5);
    assert(albums[0].album == "Multi");
    assert((albums[0].sourceIndices == std::vector<std::size_t>{4, 3}));
    assert(albums[1].album == "Same");
    assert(albums[1].albumArtist == "Artist A");
    assert((albums[1].sourceIndices == std::vector<std::size_t>{1, 0}));
    assert(albums[2].albumArtist == "Artist B");
    assert(albums[3].album == "Single");
    assert(albums[4].album == "Single");

    const std::vector<AlbumSourceTrack> subset{
        {9, "Partial", "Band", "Track 3", "Band", "", 1, 3},
        {7, "Partial", "Band", "Track 1", "Band", "", 1, 1},
    };
    const auto partial = refrain::buildAlbumBrowserAlbums(subset);
    assert(partial.size() == 1);
    assert((partial[0].sourceIndices == std::vector<std::size_t>{7, 9}));

    std::vector<AlbumSourceTrack> large;
    large.reserve(50000);
    for (std::size_t index = 0; index < 50000; ++index) {
        const auto album = index / 10;
        large.push_back({index, "Album " + std::to_string(album), "Artist " + std::to_string(album % 100),
            "Track " + std::to_string(index), "Artist", "2025", 1,
            static_cast<unsigned>(index % 10 + 1)});
    }
    const auto largeAlbums = refrain::buildAlbumBrowserAlbums(large);
    assert(largeAlbums.size() == 5000);
    std::size_t largeTrackCount{};
    for (const auto& album : largeAlbums) largeTrackCount += album.sourceIndices.size();
    assert(largeTrackCount == large.size());
    return 0;
}
