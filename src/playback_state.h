#pragma once

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <optional>
#include <string>

namespace refrain {

struct PlaybackState {
    bool playing{};
    bool paused{};
    bool seekable{};
    bool customVolume{};
    double position{};
    double length{};
    float volumeDb{};
    float lastAudibleVolumeDb{};

    [[nodiscard]] bool hasKnownLength() const noexcept {
        return std::isfinite(length) && length > 0.0;
    }

    [[nodiscard]] bool canSeek() const noexcept {
        return playing && seekable && hasKnownLength();
    }

    [[nodiscard]] bool muted() const noexcept {
        return volumeDb <= -99.9F;
    }

    void setVolume(float value) noexcept {
        volumeDb = std::clamp(value, -100.0F, 0.0F);
        if (!muted()) {
            lastAudibleVolumeDb = volumeDb;
        }
    }
};

[[nodiscard]] inline double clampUnit(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] inline double seekFraction(double position, double length) noexcept {
    if (!std::isfinite(position) || !std::isfinite(length) || length <= 0.0) {
        return 0.0;
    }
    return clampUnit(position / length);
}

[[nodiscard]] inline double seekPosition(double fraction, double length) noexcept {
    if (!std::isfinite(length) || length <= 0.0) {
        return 0.0;
    }
    return clampUnit(fraction) * length;
}

[[nodiscard]] inline std::optional<double> committedSeekTarget(
    bool previewing, bool releasedInside, bool canSeek, double previewPosition, double length) noexcept {
    if (!previewing || !releasedInside || !canSeek || !std::isfinite(length) || length <= 0.0) {
        return std::nullopt;
    }
    return std::clamp(previewPosition, 0.0, length);
}

// Match Columns UI's Volume toolbar transfer function so that both controls show
// the same relative position for a given foobar2000 dB value.
[[nodiscard]] inline float volumeDbFromFraction(double fraction) noexcept {
    const auto normalized = clampUnit(fraction);
    if (normalized <= 0.0) {
        return -100.0F;
    }
    return static_cast<float>(std::max(10.0 * std::log2(normalized), -100.0));
}

[[nodiscard]] inline double volumeFractionFromDb(float volumeDb) noexcept {
    const auto clamped = std::clamp(static_cast<double>(volumeDb), -100.0, 0.0);
    return clampUnit(std::pow(2.0, clamped / 10.0));
}

[[nodiscard]] inline std::wstring formatPlaybackTime(double seconds, bool known = true) {
    if (!known || !std::isfinite(seconds) || seconds < 0.0) {
        return L"--:--";
    }

    const auto rounded = static_cast<unsigned long long>(seconds);
    const auto hours = rounded / 3600ULL;
    const auto minutes = (rounded % 3600ULL) / 60ULL;
    const auto remainingSeconds = rounded % 60ULL;
    wchar_t buffer[32]{};
    if (hours > 0ULL) {
        std::swprintf(buffer, std::size(buffer), L"%llu:%02llu:%02llu", hours, minutes, remainingSeconds);
    } else {
        std::swprintf(buffer, std::size(buffer), L"%llu:%02llu", minutes, remainingSeconds);
    }
    return buffer;
}

} // namespace refrain
