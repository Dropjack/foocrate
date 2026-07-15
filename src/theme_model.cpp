#include "theme_model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace refrain {
namespace {

struct HslColor {
    double hue{};
    double saturation{};
    double lightness{};
};

[[nodiscard]] double channel(std::uint8_t value) noexcept {
    const auto normalized = static_cast<double>(value) / 255.0;
    return normalized <= 0.04045 ? normalized / 12.92
        : std::pow((normalized + 0.055) / 1.055, 2.4);
}

[[nodiscard]] RgbColor mix(RgbColor first, RgbColor second, double amount) noexcept {
    amount = std::clamp(amount, 0.0, 1.0);
    const auto blend = [amount](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(std::lround(
            static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * amount));
    };
    return {blend(first.red, second.red), blend(first.green, second.green), blend(first.blue, second.blue)};
}

[[nodiscard]] HslColor toHsl(RgbColor value) noexcept {
    const auto red = static_cast<double>(value.red) / 255.0;
    const auto green = static_cast<double>(value.green) / 255.0;
    const auto blue = static_cast<double>(value.blue) / 255.0;
    const auto maximum = std::max({red, green, blue});
    const auto minimum = std::min({red, green, blue});
    const auto delta = maximum - minimum;
    HslColor result;
    result.lightness = (maximum + minimum) * 0.5;
    if (delta <= std::numeric_limits<double>::epsilon()) return result;
    result.saturation = delta / (1.0 - std::abs(2.0 * result.lightness - 1.0));
    if (maximum == red) result.hue = std::fmod((green - blue) / delta, 6.0);
    else if (maximum == green) result.hue = (blue - red) / delta + 2.0;
    else result.hue = (red - green) / delta + 4.0;
    result.hue *= 60.0;
    if (result.hue < 0.0) result.hue += 360.0;
    return result;
}

[[nodiscard]] RgbColor fromHsl(HslColor value) noexcept {
    value.hue = std::fmod(value.hue, 360.0);
    if (value.hue < 0.0) value.hue += 360.0;
    value.saturation = std::clamp(value.saturation, 0.0, 1.0);
    value.lightness = std::clamp(value.lightness, 0.0, 1.0);
    const auto chroma = (1.0 - std::abs(2.0 * value.lightness - 1.0)) * value.saturation;
    const auto section = value.hue / 60.0;
    const auto intermediate = chroma * (1.0 - std::abs(std::fmod(section, 2.0) - 1.0));
    double red{};
    double green{};
    double blue{};
    if (section < 1.0) { red = chroma; green = intermediate; }
    else if (section < 2.0) { red = intermediate; green = chroma; }
    else if (section < 3.0) { green = chroma; blue = intermediate; }
    else if (section < 4.0) { green = intermediate; blue = chroma; }
    else if (section < 5.0) { red = intermediate; blue = chroma; }
    else { red = chroma; blue = intermediate; }
    const auto offset = value.lightness - chroma * 0.5;
    const auto convert = [offset](double component) {
        return static_cast<std::uint8_t>(std::lround((component + offset) * 255.0));
    };
    return {convert(red), convert(green), convert(blue)};
}

[[nodiscard]] RgbColor bestAccentText(RgbColor accent) noexcept {
    constexpr auto dark = rgb(0x101515);
    constexpr auto light = rgb(0xFFFFFF);
    return contrastRatio(dark, accent) >= contrastRatio(light, accent) ? dark : light;
}

[[nodiscard]] RgbColor ensureVisibleAccent(RgbColor accent, RgbColor background, bool dark) noexcept {
    auto hsl = toHsl(accent);
    hsl.saturation = std::clamp(hsl.saturation, 0.35, 0.82);
    hsl.lightness = dark ? std::clamp(hsl.lightness, 0.48, 0.72)
                         : std::clamp(hsl.lightness, 0.28, 0.58);
    auto result = fromHsl(hsl);
    for (int attempt = 0; attempt < 10 && contrastRatio(result, background) < 3.0; ++attempt) {
        hsl.lightness += dark ? 0.035 : -0.035;
        hsl.lightness = std::clamp(hsl.lightness, 0.18, 0.82);
        result = fromHsl(hsl);
    }
    return result;
}

} // namespace

double contrastRatio(RgbColor first, RgbColor second) noexcept {
    const auto luminance = [](RgbColor value) {
        return 0.2126 * channel(value.red) + 0.7152 * channel(value.green) + 0.0722 * channel(value.blue);
    };
    auto firstValue = luminance(first);
    auto secondValue = luminance(second);
    if (firstValue < secondValue) std::swap(firstValue, secondValue);
    return (firstValue + 0.05) / (secondValue + 0.05);
}

ThemePalette presetPalette(ThemePreset preset) noexcept {
    switch (preset) {
    case ThemePreset::paper:
        return {false, rgb(0xF4F1EA), rgb(0xEEE9DF), rgb(0xDDD7CC), rgb(0xFFFCF7),
            rgb(0x252A2E), rgb(0x6F6B64), rgb(0xA5A099), rgb(0x252A2E), rgb(0xC9C1B4),
            rgb(0xE7A43B), rgb(0xF0B34F), rgb(0xECE6DB), rgb(0xE2DBCF), rgb(0xB66D27),
            rgb(0x586D8F), rgb(0xA8443A)};
    case ThemePreset::pine:
        return {true, rgb(0x151B1A), rgb(0x1D2623), rgb(0x27322E), rgb(0x303C37),
            rgb(0xEDF4F0), rgb(0xAEBBB5), rgb(0x707C77), rgb(0x102019), rgb(0x3B4843),
            rgb(0x6FA58C), rgb(0x80B89F), rgb(0x23302B), rgb(0x2C4338), rgb(0x8EC2AA),
            rgb(0x7FA8D9), rgb(0xE27878)};
    case ThemePreset::ink:
        return {true, rgb(0x111820), rgb(0x18232E), rgb(0x21303D), rgb(0x2B3A48),
            rgb(0xF0F4F7), rgb(0xAAB7C2), rgb(0x6F7E89), rgb(0x241A09), rgb(0x394958),
            rgb(0xE4A94F), rgb(0xF0B95F), rgb(0x202D39), rgb(0x314254), rgb(0xF4C46F),
            rgb(0x7FB1E3), rgb(0xE27972)};
    case ThemePreset::mist:
    default:
        return {false, rgb(0xEEF4F7), rgb(0xF3F5F2), rgb(0xE4EAE7), rgb(0xFFFFFF),
            rgb(0x1F2A2A), rgb(0x63706E), rgb(0xA7B1AE), rgb(0xFFFFFF), rgb(0xC9D4D0),
            rgb(0x4F7F6A), rgb(0x5F927B), rgb(0xE8EFEC), rgb(0xDCE6E2), rgb(0x2E5FA8),
            rgb(0x2E5FA8), rgb(0xB34444)};
    }
}

ThemePalette deriveExternalPalette(RgbColor background, RgbColor foreground, RgbColor selection,
    RgbColor selectionText, RgbColor focus, bool dark) noexcept {
    const auto reference = presetPalette(dark ? ThemePreset::pine : ThemePreset::mist);
    auto result = reference;
    result.dark = dark;
    result.backgroundBase = background;
    if (contrastRatio(foreground, background) < 4.5) {
        foreground = contrastRatio(rgb(0x101515), background) >= contrastRatio(rgb(0xFFFFFF), background)
            ? rgb(0x101515) : rgb(0xFFFFFF);
    }
    result.surfacePanel = mix(background, foreground, dark ? 0.07 : 0.035);
    result.surfaceMuted = mix(background, foreground, dark ? 0.13 : 0.09);
    result.surfaceElevated = mix(background, foreground, dark ? 0.18 : 0.015);
    result.textPrimary = foreground;
    result.textSecondary = mix(foreground, background, dark ? 0.28 : 0.30);
    for (double amount = dark ? 0.24 : 0.26;
         contrastRatio(result.textSecondary, background) < 4.5 && amount >= 0.0; amount -= 0.04) {
        result.textSecondary = mix(foreground, background, amount);
    }
    result.textDisabled = mix(foreground, background, dark ? 0.55 : 0.52);
    result.borderSubtle = mix(foreground, background, dark ? 0.72 : 0.68);
    result.stateHover = mix(background, selection, 0.28);
    result.stateSelected = mix(background, selection, 0.48);
    result.focus = focus;
    result = applyAccent(result, selection);
    result.textOnAccent = contrastRatio(selectionText, result.accent) >= 4.5
        ? selectionText : bestAccentText(result.accent);
    result.defaultPlaylist = contrastRatio(focus, background) >= 4.5 ? focus : foreground;
    result.error = contrastRatio(reference.error, background) >= 4.5 ? reference.error : foreground;
    return result;
}

ThemePalette applyAccent(ThemePalette palette, RgbColor accent) noexcept {
    accent = ensureVisibleAccent(accent, palette.backgroundBase, palette.dark);
    palette.accent = accent;
    palette.textOnAccent = bestAccentText(accent);
    palette.accentHover = mix(accent, rgb(0xFFFFFF), 0.14);
    return palette;
}

RgbColor extractArtworkAccent(std::span<const std::uint8_t> pixels, std::uint32_t width,
    std::uint32_t height, RgbColor fallback, bool darkBackground) noexcept {
    const auto expected = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4ULL;
    if (width == 0 || height == 0 || expected > pixels.size()) return fallback;
    std::array<double, 4096> scores{};
    const auto pixelCount = static_cast<std::uint64_t>(width) * height;
    const auto step = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(
        std::sqrt(static_cast<double>(pixelCount) / 4096.0)));
    for (std::uint64_t index = 0; index < pixelCount; index += step) {
        const auto offset = static_cast<std::size_t>(index * 4ULL);
        const auto alpha = pixels[offset + 3];
        if (alpha < 128) continue;
        const auto unpremultiply = [alpha](std::uint8_t value) {
            return alpha == 255 ? value : static_cast<std::uint8_t>(std::min(255U,
                (static_cast<unsigned>(value) * 255U + alpha / 2U) / alpha));
        };
        const RgbColor colour{unpremultiply(pixels[offset + 2]),
            unpremultiply(pixels[offset + 1]), unpremultiply(pixels[offset])};
        const auto hsl = toHsl(colour);
        if (hsl.lightness < 0.08 || hsl.lightness > 0.92 || hsl.saturation < 0.18) continue;
        const auto bucket = static_cast<std::size_t>((colour.red >> 4U) << 8U
            | (colour.green >> 4U) << 4U | (colour.blue >> 4U));
        scores[bucket] += 0.5 + hsl.saturation;
    }
    const auto found = std::max_element(scores.begin(), scores.end());
    if (found == scores.end() || *found <= 0.0) return fallback;
    const auto bucket = static_cast<std::size_t>(std::distance(scores.begin(), found));
    const RgbColor candidate{static_cast<std::uint8_t>(((bucket >> 8U) & 0xFU) * 17U),
        static_cast<std::uint8_t>(((bucket >> 4U) & 0xFU) * 17U),
        static_cast<std::uint8_t>((bucket & 0xFU) * 17U)};
    return ensureVisibleAccent(candidate,
        darkBackground ? presetPalette(ThemePreset::pine).backgroundBase
                       : presetPalette(ThemePreset::mist).backgroundBase,
        darkBackground);
}

std::optional<ThemePalette> extractArtworkTheme(std::span<const std::uint8_t> pixels,
    std::uint32_t width, std::uint32_t height) noexcept {
    const auto expected = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4ULL;
    if (width == 0 || height == 0 || expected > pixels.size()) return std::nullopt;

    double luminanceTotal{};
    std::uint64_t samples{};
    const auto pixelCount = static_cast<std::uint64_t>(width) * height;
    const auto step = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(
        std::sqrt(static_cast<double>(pixelCount) / 4096.0)));
    for (std::uint64_t index = 0; index < pixelCount; index += step) {
        const auto offset = static_cast<std::size_t>(index * 4ULL);
        const auto alpha = pixels[offset + 3];
        if (alpha < 128) continue;
        const auto unpremultiply = [alpha](std::uint8_t value) {
            return alpha == 255 ? value : static_cast<std::uint8_t>(std::min(255U,
                (static_cast<unsigned>(value) * 255U + alpha / 2U) / alpha));
        };
        const auto red = unpremultiply(pixels[offset + 2]);
        const auto green = unpremultiply(pixels[offset + 1]);
        const auto blue = unpremultiply(pixels[offset]);
        luminanceTotal += (static_cast<double>(red) * 299.0
            + static_cast<double>(green) * 587.0 + static_cast<double>(blue) * 114.0) / 255000.0;
        ++samples;
    }
    if (samples == 0) return std::nullopt;

    const auto dark = luminanceTotal / static_cast<double>(samples) < 0.44;
    const auto reference = presetPalette(dark ? ThemePreset::pine : ThemePreset::mist);
    constexpr auto sentinel = rgb(0x000000);
    const auto extracted = extractArtworkAccent(pixels, width, height, sentinel, dark);
    const auto hasChromaticColour = extracted != sentinel;
    const auto accent = hasChromaticColour ? extracted : reference.accent;
    const auto accentHsl = toHsl(accent);
    const auto background = fromHsl({accentHsl.hue,
        hasChromaticColour ? std::clamp(accentHsl.saturation * 0.22, 0.06, 0.16) : 0.0,
        dark ? 0.10 : 0.94});
    auto palette = deriveExternalPalette(background, reference.textPrimary, accent,
        bestAccentText(accent), accent, dark);
    palette = applyAccent(palette, accent);
    return palette;
}

} // namespace refrain
