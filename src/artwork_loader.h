#pragma once

#include "theme_model.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>

#include <foobar2000/SDK/foobar2000.h>

namespace refrain {

enum class ArtworkStatus {
    ready,
    missing,
    unavailable,
    aborted,
};

struct ArtworkPixels {
    ArtworkStatus status{ArtworkStatus::unavailable};
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> bgra;
};

[[nodiscard]] ArtworkPixels loadFrontArtwork(const metadb_handle_ptr& target, abort_callback& aborter) noexcept;
[[nodiscard]] ArtworkPixels loadArtwork(
    const metadb_handle_ptr& target, const GUID& artworkId, abort_callback& aborter) noexcept;
[[nodiscard]] ArtworkPixels loadGroupArtwork(
    metadb_handle_list_cref targets, const GUID& artworkId, abort_callback& aborter,
    std::uint32_t maxDimension = 1024U) noexcept;
[[nodiscard]] std::optional<ThemePalette> loadArtworkTheme(
    const metadb_handle_ptr& target, const GUID& artworkId, abort_callback& aborter) noexcept;

} // namespace refrain
