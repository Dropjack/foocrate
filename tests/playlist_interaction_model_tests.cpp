#include "playlist_interaction_model.h"
#include "compact_track_list_model.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool value, const char* message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    using namespace foocrate;

    expect(playlistDragStartMode(true) == PlaylistDragStartMode::internalReorder,
        "a reorderable playlist must begin with internal drag handling");
    expect(playlistDragStartMode(false) == PlaylistDragStartMode::outgoingCopy,
        "a locked autoplaylist must begin an outgoing copy drag instead of cancelling");

    const auto contiguous = planPlaylistMove(6, {1, 2}, 5);
    expect(contiguous.order == std::vector<std::size_t>({0, 3, 4, 1, 2, 5}),
        "contiguous selection must preserve relative order");
    expect(contiguous.movedDestinations == std::vector<std::size_t>({3, 4}),
        "moved destinations must identify the new selection");
    expect(contiguous.changed, "a real move must be marked changed");

    const auto nonContiguous = planPlaylistMove(7, {1, 4, 6}, 3);
    expect(nonContiguous.order == std::vector<std::size_t>({0, 2, 1, 4, 6, 3, 5}),
        "non-contiguous selection must remain ordered");

    const auto noOp = planPlaylistMove(5, {1, 2}, 3);
    expect(noOp.order == std::vector<std::size_t>({0, 1, 2, 3, 4}) && !noOp.changed,
        "dropping at an equivalent boundary must be a no-op");

    const auto normalized = planPlaylistMove(4, {3, 3, 99, 1}, 0);
    expect(normalized.order == std::vector<std::size_t>({1, 3, 0, 2}),
        "invalid and duplicate selection indexes must be ignored");

    expect(compactQueuePositions({}) == L"", "empty queue positions must be blank");
    expect(compactQueuePositions({1}) == L"2", "single queue position must be one-based");
    expect(compactQueuePositions({1, 4, 7}) == L"2 +2", "duplicates must use compact count syntax");
    expect(prioritizeIndexOrder(4, 2) == std::vector<std::size_t>({2, 0, 1, 3}),
        "play-now order must move only the chosen queue item to the front");
    expect(prioritizeIndexOrder(2, 2).empty(), "invalid play-now index must be rejected");
    expect(uniqueValuesNotIn(std::vector<int>{1, 2}, std::vector<int>{2, 3, 3, 4})
            == std::vector<int>({3, 4}),
        "playlist drops must skip existing and repeated incoming identities");
    expect(insertionBoundaryForRow(2, false, 5) == 2, "upper row half inserts before");
    expect(insertionBoundaryForRow(2, true, 5) == 3, "lower row half inserts after");
    const auto ratingStrip = playlistRatingStrip(100.0F, 200.0F, 20.0F, 26.0F);
    expect(ratingStrip.valid() && ratingStrip.starSize == 16.0F
            && ratingStrip.left == 110.0F && ratingStrip.top == 25.0F,
        "playlist rating strip must center five Foobox-sized stars in the column");
    expect(playlistRatingFromPoint(ratingStrip, 110.0F, 30.0F) == 1,
        "playlist rating hit testing must begin at the first drawn star");
    expect(playlistRatingFromPoint(ratingStrip, 173.9F, 30.0F) == 4,
        "playlist rating hit testing must use the same contiguous star cells as drawing");
    expect(playlistRatingFromPoint(ratingStrip, 190.0F, 30.0F) == 5,
        "playlist rating hit testing must include the final drawn edge");
    expect(playlistRatingFromPoint(ratingStrip, 105.0F, 30.0F) == 0,
        "playlist rating column padding must not activate an invisible star");
    const auto narrowRatingStrip = playlistRatingStrip(0.0F, 68.0F, 0.0F, 22.0F);
    expect(narrowRatingStrip.valid() && narrowRatingStrip.starSize == 12.0F,
        "narrow rating columns must scale all five stars and preserve alignment");
    const auto columns = compactTrackColumns();
    expect(columns.front().field == CompactTrackField::ordinal && columns.front().leftFraction == 0.0F,
        "compact track list must start with ordinal");
    expect(columns.back().field == CompactTrackField::length && columns.back().rightFraction == 1.0F,
        "compact track list must end with length");
    expect(compactTrackColumnX(100.0F, 300.0F, 0.5F) == 250.0F,
        "compact column geometry must be reusable in any right panel");

    return 0;
}
