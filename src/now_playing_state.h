#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace foocrate {

enum class TopBarPlaybackState { stopped, playing, paused };

[[nodiscard]] inline std::wstring buildTopBarSummary(TopBarPlaybackState state,
    std::wstring_view title, std::wstring_view artist, std::wstring_view album,
    const std::vector<std::wstring>& technicalFields) {
    if (state == TopBarPlaybackState::stopped) return {};
    std::wstring result = state == TopBarPlaybackState::paused ? L"Ⅱ " : L"▶ ";
    result += title.empty() ? L"Unknown title" : title;
    if (!artist.empty()) {
        result += L" - ";
        result += artist;
    }
    const auto appendField = [&](std::wstring_view value) {
        if (value.empty()) return;
        result += L" | ";
        result += value;
    };
    appendField(album);
    for (const auto& value : technicalFields) appendField(value);
    return result;
}

[[nodiscard]] inline int normalizeRating(double value) noexcept {
    if (!std::isfinite(value)) {
        return 0;
    }
    const auto rounded = static_cast<int>(std::lround(value));
    return rounded >= 1 && rounded <= 5 ? rounded : 0;
}

[[nodiscard]] inline int ratingFromPoint(float x, float left, float starSize, float gap) noexcept {
    if (!std::isfinite(x) || starSize <= 0.0F || gap < 0.0F || x < left) {
        return 0;
    }
    for (int star = 1; star <= 5; ++star) {
        const auto starLeft = left + static_cast<float>(star - 1) * (starSize + gap);
        if (x >= starLeft && x <= starLeft + starSize) {
            return star;
        }
    }
    return 0;
}

[[nodiscard]] inline int ratingCommandValue(int currentRating, int clickedRating) noexcept {
    const auto clicked = std::clamp(clickedRating, 0, 5);
    return clicked > 0 && clicked == currentRating ? 0 : clicked;
}

[[nodiscard]] inline std::string normalizeMenuPath(std::string_view path) {
    std::string result;
    result.reserve(path.size());
    for (const auto character : path) {
        if (character == '&') continue;
        if (character == '\\') {
            result.push_back('/');
        } else if (character >= 'A' && character <= 'Z') {
            result.push_back(static_cast<char>(character - 'A' + 'a'));
        } else {
            result.push_back(character);
        }
    }
    return result;
}

[[nodiscard]] inline bool menuPathMatches(std::string_view actual, std::string_view expected) {
    const auto normalizedActual = normalizeMenuPath(actual);
    const auto normalizedExpected = normalizeMenuPath(expected);
    if (normalizedActual == normalizedExpected) return true;
    return normalizedActual.size() > normalizedExpected.size()
        && normalizedActual[normalizedActual.size() - normalizedExpected.size() - 1] == '/'
        && normalizedActual.ends_with(normalizedExpected);
}

} // namespace foocrate
