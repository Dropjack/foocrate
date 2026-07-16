#pragma once

#include <array>
#include <cstddef>

namespace foocrate {

inline constexpr float kCompactTrackHeaderHeight = 28.0F;
inline constexpr float kCompactTrackRowHeight = 26.0F;

enum class CompactTrackField { ordinal, title, artist, source, length };

struct CompactTrackColumn {
    CompactTrackField field{};
    float leftFraction{};
    float rightFraction{};
    bool centered{};
};

[[nodiscard]] constexpr std::array<CompactTrackColumn, 5> compactTrackColumns() noexcept {
    return {{{CompactTrackField::ordinal, 0.00F, 0.12F, true},
        {CompactTrackField::title, 0.12F, 0.52F, false},
        {CompactTrackField::artist, 0.52F, 0.73F, false},
        {CompactTrackField::source, 0.73F, 0.90F, false},
        {CompactTrackField::length, 0.90F, 1.00F, true}}};
}

[[nodiscard]] constexpr float compactTrackColumnX(
    float left, float width, float fraction) noexcept {
    return left + width * fraction;
}

} // namespace foocrate
