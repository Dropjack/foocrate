#pragma once

#include <cstddef>

namespace refrain::layout {

struct PlaybackBarVerticalMetrics {
    static constexpr float height = 68.0F;
    static constexpr float seekCenterOffset = 12.0F;
    static constexpr float seekHitHalfHeight = 7.0F;
    static constexpr float seekThumbRadius = 5.5F;
    static constexpr float seekHorizontalInset = 68.0F;
    static constexpr float timeTopOffset = 1.0F;
    static constexpr float timeBottomOffset = 23.0F;
    static constexpr float buttonTopOffset = 31.0F;
    static constexpr float buttonSize = 24.0F;
    static constexpr float primaryButtonTopOffset = 29.0F;
    static constexpr float primaryButtonSize = 28.0F;
    static constexpr float buttonGap = 8.0F;
    static constexpr float horizontalMargin = 16.0F;
    static constexpr float volumeWidth = 96.0F;
    static constexpr float volumeHitHeight = 16.0F;
    static constexpr float regularIconDesignSize = 36.0F;
    static constexpr float primaryIconDesignSize = 42.0F;

    [[nodiscard]] static constexpr float surfaceTop(float clientHeight) noexcept
    {
        return clientHeight > height ? clientHeight - height : 0.0F;
    }

    [[nodiscard]] static constexpr bool controlsFit() noexcept
    {
        const auto seekTop = seekCenterOffset - seekHitHalfHeight;
        const auto seekBottom = seekCenterOffset + seekHitHalfHeight;
        const auto primaryTop = primaryButtonTopOffset;
        const auto primaryBottom = primaryButtonTopOffset + primaryButtonSize;
        return seekTop >= 0.0F && seekBottom < primaryTop && primaryTop >= 0.0F
            && primaryBottom <= height && timeTopOffset >= 0.0F && timeBottomOffset <= height;
    }

    [[nodiscard]] static constexpr float centralGroupHalfWidth() noexcept
    {
        return primaryButtonSize * 0.5F + 2.0F * (buttonGap + buttonSize);
    }

    [[nodiscard]] static constexpr float rightGroupWidth() noexcept
    {
        return volumeWidth + 4.0F * buttonSize + 4.0F * buttonGap;
    }
};

struct InteriorChromeMetrics {
    static constexpr float dividerHitWidth = 8.0F;
    static constexpr float dividerVisualWidth = 1.0F;
    static constexpr float scrollbarHitWidth = 12.0F;
    static constexpr float scrollbarVisualWidth = 3.0F;
    static constexpr unsigned scrollbarHideDelayMs = 1000;
    static constexpr float rightPanelMinWidth = 280.0F;
    static constexpr float rightPanelMaxWidth = 440.0F;
};

struct NowPlayingHeaderMetrics {
    static constexpr float informationHeight = 105.0F;
    static constexpr float minimumArtworkSize = 32.0F;
    static constexpr float starTopGap = 6.0F;

    [[nodiscard]] static constexpr float artworkSize(float panelWidth, float headerHeight) noexcept
    {
        const auto availableHeight = headerHeight > informationHeight
            ? headerHeight - informationHeight : minimumArtworkSize;
        const auto boundedWidth = panelWidth > minimumArtworkSize ? panelWidth : minimumArtworkSize;
        const auto boundedHeight = availableHeight > minimumArtworkSize
            ? availableHeight : minimumArtworkSize;
        return boundedWidth < boundedHeight ? boundedWidth : boundedHeight;
    }

    [[nodiscard]] static constexpr float artworkLeft(
        float panelLeft, float panelWidth, float size) noexcept
    {
        return panelLeft + (panelWidth - size) * 0.5F;
    }
};

struct AlbumCoverWallMetrics {
    static constexpr float preferredCellSize = 184.0F;
    static constexpr float titleStripHeight = 24.0F;
    static constexpr float selectionStrokeWidth = 3.0F;

    struct CropRectangle {
        float left{};
        float top{};
        float right{};
        float bottom{};
    };

    [[nodiscard]] static constexpr std::size_t columns(float width) noexcept
    {
        return width >= preferredCellSize
            ? static_cast<std::size_t>(width / preferredCellSize) : 1;
    }

    [[nodiscard]] static constexpr float cellSize(float width) noexcept
    {
        return width > 0.0F ? width / static_cast<float>(columns(width)) : 0.0F;
    }

    [[nodiscard]] static constexpr std::size_t visibleRows(float height, float cell) noexcept
    {
        return height > 0.0F && cell > 0.0F
            ? (static_cast<std::size_t>(height / cell) > 0
                    ? static_cast<std::size_t>(height / cell) : 1)
            : 1;
    }

    [[nodiscard]] static constexpr CropRectangle centeredSquareCrop(float width, float height) noexcept
    {
        if (width <= 0.0F || height <= 0.0F) return {};
        const auto side = width < height ? width : height;
        const auto left = (width - side) * 0.5F;
        const auto top = (height - side) * 0.5F;
        return {left, top, left + side, top + side};
    }
};

static_assert(PlaybackBarVerticalMetrics::controlsFit());

} // namespace refrain::layout
