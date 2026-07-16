#include "theme_model.h"

#include <array>
#include <iostream>

namespace {

int failures{};

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    using namespace foocrate;

    const auto mist = presetPalette(ThemePreset::mist);
    const auto paper = presetPalette(ThemePreset::paper);
    const auto pine = presetPalette(ThemePreset::pine);
    const auto ink = presetPalette(ThemePreset::ink);
    expect(!mist.dark && !paper.dark && pine.dark && ink.dark, "two presets must be light and two dark");
    expect(rgbValue(mist.backgroundBase) == 0xEEF4F7, "mist background must match approved palette");
    expect(rgbValue(paper.accent) == 0xE7A43B, "paper accent must match approved palette");
    expect(rgbValue(pine.backgroundBase) == 0x151B1A, "pine background must match approved palette");
    expect(rgbValue(ink.accent) == 0xE4A94F, "ink accent must match approved palette");

    for (const auto* palette : {&mist, &paper, &pine, &ink}) {
        expect(contrastRatio(palette->textPrimary, palette->backgroundBase) >= 4.5,
            "primary text must meet normal-text contrast");
        expect(contrastRatio(palette->textSecondary, palette->backgroundBase) >= 4.5,
            "secondary text must meet normal-text contrast");
        expect(contrastRatio(palette->textOnAccent, palette->accent) >= 4.5,
            "accent foreground must meet normal-text contrast");
        expect(contrastRatio(palette->error, palette->backgroundBase) >= 4.5,
            "error text must meet normal-text contrast");
        expect(palette->lyricsHighlight != palette->accent,
            "preset lyric highlight must use a contrasting second colour");
        expect(contrastRatio(palette->lyricsNormal, palette->backgroundBase) >= 4.5,
            "preset normal lyrics must remain readable");
        expect(contrastRatio(palette->lyricsHighlight, palette->backgroundBase) >= 7.0,
            "preset lyric highlight must meet enhanced contrast");
    }

    const auto custom = applyAccent(mist, rgb(0xFF00FF));
    expect(custom.accent != mist.accent, "custom accent must replace recommended accent");
    expect(contrastRatio(custom.textOnAccent, custom.accent) >= 4.5,
        "dynamic accent must choose readable foreground");

    std::array<std::uint8_t, 16> pixels{
        0x20, 0x60, 0xD0, 0xFF, 0x20, 0x60, 0xD0, 0xFF,
        0x20, 0x60, 0xD0, 0xFF, 0x20, 0x60, 0xD0, 0xFF};
    const auto artwork = extractArtworkAccent(pixels, 2, 2, mist.accent, mist.dark);
    expect(artwork != mist.accent, "colourful artwork must produce a dynamic accent");
    expect(contrastRatio(artwork, mist.backgroundBase) >= 3.0,
        "artwork accent must remain visible on the theme background");

    std::array<std::uint8_t, 16> grey{
        0x80, 0x80, 0x80, 0xFF, 0x80, 0x80, 0x80, 0xFF,
        0x80, 0x80, 0x80, 0xFF, 0x80, 0x80, 0x80, 0xFF};
    expect(extractArtworkAccent(grey, 2, 2, mist.accent, mist.dark) == mist.accent,
        "low-saturation artwork must use the safe fallback");
    expect(extractArtworkAccent({}, 0, 0, mist.accent, mist.dark) == mist.accent,
        "missing artwork must use the safe fallback");

    const auto artworkTheme = extractArtworkTheme(pixels, 2, 2);
    expect(artworkTheme.has_value(), "valid artwork must produce a complete theme");
    expect(artworkTheme && artworkTheme->backgroundBase != mist.backgroundBase,
        "artwork theme must change the base colour, not only the accent");
    expect(artworkTheme && contrastRatio(artworkTheme->textPrimary, artworkTheme->backgroundBase) >= 4.5,
        "artwork theme primary text must remain readable");
    expect(artworkTheme && contrastRatio(artworkTheme->textSecondary, artworkTheme->backgroundBase) >= 4.5,
        "artwork theme secondary text must remain readable");
    expect(artworkTheme
            && contrastRatio(artworkTheme->lyricsNormal, artworkTheme->backgroundBase) >= 4.5,
        "artwork normal lyrics must remain readable");
    std::array<std::uint8_t, 32> twoColourArtwork{
        0x20, 0x60, 0xD0, 0xFF, 0x20, 0x60, 0xD0, 0xFF,
        0x20, 0x60, 0xD0, 0xFF, 0x20, 0x60, 0xD0, 0xFF,
        0xD0, 0x60, 0x20, 0xFF, 0xD0, 0x60, 0x20, 0xFF,
        0xD0, 0x60, 0x20, 0xFF, 0xD0, 0x60, 0x20, 0xFF};
    const auto contrastingArtworkTheme = extractArtworkTheme(twoColourArtwork, 4, 2);
    expect(contrastingArtworkTheme
            && contrastingArtworkTheme->lyricsHighlight != contrastingArtworkTheme->accent,
        "artwork lyric highlight must prefer a distinct second artwork colour");
    expect(contrastingArtworkTheme
            && contrastRatio(contrastingArtworkTheme->lyricsHighlight,
                contrastingArtworkTheme->backgroundBase) >= 7.0,
        "artwork lyric highlight must meet enhanced contrast");
    const auto greyTheme = extractArtworkTheme(grey, 2, 2);
    expect(greyTheme.has_value(), "greyscale artwork must still produce a neutral base theme");
    expect(!extractArtworkTheme({}, 0, 0).has_value(), "missing artwork must not produce a stale theme");

    const auto external = deriveExternalPalette(rgb(0x101820), rgb(0xF0F0F0),
        rgb(0x4070A0), rgb(0xFFFFFF), rgb(0x80A0C0), true);
    expect(external.dark, "external dark palette must preserve dark mode");
    expect(external.backgroundBase == rgb(0x101820), "external palette must use supplied background");

    return failures == 0 ? 0 : 1;
}
