#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <optional>

namespace refrain {

enum class ThemePreset : std::int64_t {
    mist = 0,
    paper = 1,
    pine = 2,
    ink = 3,
};

enum class ColourMode : std::int64_t {
    windows = 0,
    refrainPreset = 1,
    albumArtwork = 2,
};

struct RgbColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};

    bool operator==(const RgbColor&) const = default;
};

struct ThemePalette {
    bool dark{};
    RgbColor backgroundBase{};
    RgbColor surfacePanel{};
    RgbColor surfaceMuted{};
    RgbColor surfaceElevated{};
    RgbColor textPrimary{};
    RgbColor textSecondary{};
    RgbColor textDisabled{};
    RgbColor textOnAccent{};
    RgbColor borderSubtle{};
    RgbColor accent{};
    RgbColor accentHover{};
    RgbColor stateHover{};
    RgbColor stateSelected{};
    RgbColor focus{};
    RgbColor defaultPlaylist{};
    RgbColor error{};
    RgbColor lyricsHighlight{};
    RgbColor lyricsNormal{};
};

[[nodiscard]] constexpr RgbColor rgb(std::uint32_t value) noexcept {
    return {static_cast<std::uint8_t>((value >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((value >> 8U) & 0xFFU),
        static_cast<std::uint8_t>(value & 0xFFU)};
}

[[nodiscard]] constexpr std::uint32_t rgbValue(RgbColor value) noexcept {
    return (static_cast<std::uint32_t>(value.red) << 16U)
        | (static_cast<std::uint32_t>(value.green) << 8U)
        | static_cast<std::uint32_t>(value.blue);
}

[[nodiscard]] constexpr bool isDarkPreset(ThemePreset preset) noexcept {
    return preset == ThemePreset::pine || preset == ThemePreset::ink;
}

[[nodiscard]] constexpr bool isValidThemePreset(std::int64_t value) noexcept {
    return value >= static_cast<std::int64_t>(ThemePreset::mist)
        && value <= static_cast<std::int64_t>(ThemePreset::ink);
}

[[nodiscard]] constexpr bool isValidColourMode(std::int64_t value) noexcept {
    return value >= static_cast<std::int64_t>(ColourMode::windows)
        && value <= static_cast<std::int64_t>(ColourMode::albumArtwork);
}

[[nodiscard]] ThemePalette presetPalette(ThemePreset preset) noexcept;
[[nodiscard]] ThemePalette deriveExternalPalette(
    RgbColor background, RgbColor foreground, RgbColor selection,
    RgbColor selectionText, RgbColor focus, bool dark) noexcept;
[[nodiscard]] ThemePalette applyAccent(ThemePalette palette, RgbColor accent) noexcept;
[[nodiscard]] RgbColor extractArtworkAccent(std::span<const std::uint8_t> premultipliedBgra,
    std::uint32_t width, std::uint32_t height, RgbColor fallback, bool darkBackground) noexcept;
[[nodiscard]] std::optional<ThemePalette> extractArtworkTheme(
    std::span<const std::uint8_t> premultipliedBgra,
    std::uint32_t width, std::uint32_t height) noexcept;
[[nodiscard]] double contrastRatio(RgbColor first, RgbColor second) noexcept;

} // namespace refrain
