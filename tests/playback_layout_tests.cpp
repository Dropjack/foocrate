#include "playback_layout.h"

#include <cassert>

int main()
{
    using Metrics = refrain::layout::PlaybackBarVerticalMetrics;
    using Chrome = refrain::layout::InteriorChromeMetrics;
    using Header = refrain::layout::NowPlayingHeaderMetrics;
    using Wall = refrain::layout::AlbumCoverWallMetrics;

    static_assert(Metrics::height == 68.0F);
    static_assert(Metrics::controlsFit());

    assert(Metrics::surfaceTop(500.0F) == 432.0F);
    assert(Metrics::surfaceTop(Metrics::height) == 0.0F);
    assert(Metrics::surfaceTop(40.0F) == 0.0F);

    static_assert(Metrics::buttonTopOffset + Metrics::buttonSize < Metrics::height);
    static_assert(Metrics::primaryButtonTopOffset + Metrics::primaryButtonSize < Metrics::height);
    static_assert(Metrics::seekCenterOffset + Metrics::seekHitHalfHeight
        < Metrics::primaryButtonTopOffset);
    static_assert(Metrics::centralGroupHalfWidth() == 78.0F);
    static_assert(Metrics::rightGroupWidth() == 224.0F);
    static_assert(Metrics::primaryButtonTopOffset
        - (Metrics::seekCenterOffset + Metrics::seekThumbRadius) == 11.5F);
    static_assert(Metrics::height
        - (Metrics::primaryButtonTopOffset + Metrics::primaryButtonSize) == 11.0F);
    static_assert(Chrome::dividerVisualWidth < Chrome::dividerHitWidth);
    static_assert(Chrome::scrollbarVisualWidth < Chrome::scrollbarHitWidth);
    static_assert(Chrome::scrollbarHideDelayMs == 1000);
    static_assert(Chrome::rightPanelMinWidth < Chrome::rightPanelMaxWidth);
    static_assert(Chrome::contentBottom(432.0F) == 431.0F);
    static_assert(Chrome::contentBottom(0.5F) == 0.0F);

    static_assert(Header::artworkSize(280.0F, 400.0F) == 280.0F);
    static_assert(Header::artworkLeft(1000.0F, 280.0F, 280.0F) == 1000.0F);
    static_assert(Header::artworkSize(440.0F, 405.0F) == 300.0F);
    static_assert(Header::artworkLeft(1000.0F, 440.0F, 300.0F) == 1070.0F);
    static_assert(Header::artworkSize(280.0F, 80.0F) == Header::minimumArtworkSize);

    static_assert(Wall::columns(183.0F) == 1);
    static_assert(Wall::columns(1492.0F) == 8);
    static_assert(Wall::cellSize(1492.0F) == 186.5F);
    static_assert(Wall::visibleRows(900.0F, 186.5F) == 4);
    constexpr auto landscapeCrop = Wall::centeredSquareCrop(600.0F, 400.0F);
    static_assert(landscapeCrop.left == 100.0F && landscapeCrop.top == 0.0F);
    static_assert(landscapeCrop.right == 500.0F && landscapeCrop.bottom == 400.0F);
    constexpr auto portraitCrop = Wall::centeredSquareCrop(300.0F, 500.0F);
    static_assert(portraitCrop.left == 0.0F && portraitCrop.top == 100.0F);
    static_assert(portraitCrop.right == 300.0F && portraitCrop.bottom == 400.0F);

    return 0;
}
