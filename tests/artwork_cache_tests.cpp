#include "artwork_cache.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
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
    {
        const std::array statuses{foocrate::ArtworkStatus::missing,
            foocrate::ArtworkStatus::unavailable, foocrate::ArtworkStatus::ready,
            foocrate::ArtworkStatus::ready};
        std::size_t calls{};
        const auto selected = foocrate::selectFirstReadyArtwork(statuses.size(), [&](std::size_t index) {
            ++calls;
            return foocrate::ArtworkPixels{statuses[index], 1, 1,
                statuses[index] == foocrate::ArtworkStatus::ready
                    ? std::vector<std::uint8_t>{static_cast<std::uint8_t>(index)}
                    : std::vector<std::uint8_t>{}};
        });
        expect(selected.status == foocrate::ArtworkStatus::ready
                && selected.bgra == std::vector<std::uint8_t>{2} && calls == 3,
            "mixed group artwork must select the first decodable item in stable order");

        const auto aborted = foocrate::selectFirstReadyArtwork(3, [](std::size_t index) {
            return foocrate::ArtworkPixels{index == 1
                ? foocrate::ArtworkStatus::aborted : foocrate::ArtworkStatus::missing};
        });
        expect(aborted.status == foocrate::ArtworkStatus::aborted,
            "group artwork fallback must stop immediately when cancelled");

        const auto unavailable = foocrate::selectFirstReadyArtwork(2, [](std::size_t index) {
            return foocrate::ArtworkPixels{index == 0
                ? foocrate::ArtworkStatus::unavailable : foocrate::ArtworkStatus::missing};
        });
        expect(unavailable.status == foocrate::ArtworkStatus::unavailable,
            "a group with only damaged and missing artwork must report unavailable");
    }

    const auto root = std::filesystem::temp_directory_path()
        / ("foocrate-artwork-cache-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    foocrate::ArtworkDiskCache cache(root, 1024U * 1024U);

    foocrate::ArtworkPixels pixels{foocrate::ArtworkStatus::ready, 2, 2,
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
    cache.storePixels("pixel-key", 320, pixels);
    const auto loadedPixels = cache.loadPixels("pixel-key", 320);
    expect(loadedPixels.has_value() && loadedPixels->width == 2 && loadedPixels->height == 2
            && loadedPixels->bgra == pixels.bgra,
        "persistent artwork cache must round-trip bounded pixels");
    expect(!cache.loadPixels("pixel-key", 160).has_value(),
        "persistent artwork cache must separate requested dimensions");

    auto palette = foocrate::presetPalette(foocrate::ThemePreset::ink);
    cache.storeTheme("theme-key", palette);
    const auto loadedTheme = cache.loadTheme("theme-key");
    expect(loadedTheme.has_value() && loadedTheme->dark == palette.dark
            && loadedTheme->backgroundBase == palette.backgroundBase
            && loadedTheme->accent == palette.accent
            && loadedTheme->lyricsNormal == palette.lyricsNormal
            && loadedTheme->lyricsHighlight == palette.lyricsHighlight,
        "persistent artwork cache must round-trip the complete theme palette");

    {
        const auto corrupt = root / "theme-colors" / "theme-key.rtm";
        std::ofstream output(corrupt, std::ios::binary | std::ios::trunc);
        output << "broken";
    }
    expect(!cache.loadTheme("theme-key").has_value()
            && !std::filesystem::exists(root / "theme-colors" / "theme-key.rtm"),
        "corrupt persistent cache entries must be discarded safely");

    const auto trimRoot = root / "trim";
    foocrate::ArtworkDiskCache smallCache(trimRoot, 70U);
    smallCache.storePixels("first", 320, pixels);
    smallCache.storePixels("second", 320, pixels);
    std::size_t cachedFiles{};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(trimRoot)) {
        if (entry.is_regular_file()) ++cachedFiles;
    }
    expect(cachedFiles <= 1, "persistent artwork cache must enforce its disk size limit");

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return 0;
}
