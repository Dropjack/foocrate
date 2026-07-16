#pragma once

#include "artwork_loader.h"
#include "theme_model.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace foocrate {

class ArtworkDiskCache {
public:
    explicit ArtworkDiskCache(std::filesystem::path root, std::uintmax_t maximumBytes = 256U * 1024U * 1024U);

    [[nodiscard]] std::optional<ArtworkPixels> loadPixels(
        const std::string& identity, std::uint32_t maximumDimension) noexcept;
    void storePixels(const std::string& identity, std::uint32_t maximumDimension,
        const ArtworkPixels& pixels) noexcept;

    [[nodiscard]] std::optional<ThemePalette> loadTheme(const std::string& identity) noexcept;
    void storeTheme(const std::string& identity, const ThemePalette& palette) noexcept;

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return m_root; }

private:
    void prepare() noexcept;
    void trim() noexcept;

    std::filesystem::path m_root;
    std::uintmax_t m_maximumBytes{};
    std::mutex m_mutex;
    bool m_prepared{};
    unsigned m_pixelWritesSinceTrim{};
};

[[nodiscard]] ArtworkDiskCache& profileArtworkCache() noexcept;

} // namespace foocrate
