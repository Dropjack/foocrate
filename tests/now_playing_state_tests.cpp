#include "now_playing_state.h"

#include <cassert>
#include <limits>

int main() {
    using namespace foocrate;

    assert(normalizeRating(1.0) == 1);
    assert(normalizeRating(4.6) == 5);
    assert(normalizeRating(0.0) == 0);
    assert(normalizeRating(6.0) == 0);
    assert(normalizeRating(std::numeric_limits<double>::quiet_NaN()) == 0);

    assert(ratingFromPoint(10.0F, 10.0F, 18.0F, 4.0F) == 1);
    assert(ratingFromPoint(28.0F, 10.0F, 18.0F, 4.0F) == 1);
    assert(ratingFromPoint(31.0F, 10.0F, 18.0F, 4.0F) == 0);
    assert(ratingFromPoint(32.0F, 10.0F, 18.0F, 4.0F) == 2);
    assert(ratingFromPoint(120.0F, 10.0F, 18.0F, 4.0F) == 0);

    assert(ratingCommandValue(3, 3) == 0);
    assert(ratingCommandValue(3, 5) == 5);
    assert(ratingCommandValue(0, 2) == 2);
    assert(ratingCommandValue(5, 0) == 0);

    assert(artistAlbumLineFormat("$if2(%artist%,Unknown) | $if2(%album%,Unknown)")
        == "$if2(%artist%,Unknown) | $if2(%album%,Unknown)");
    assert(artistAlbumLineFormat("%title%\n%artist% | %album%") == "%artist% | %album%");
    assert(artistAlbumLineFormat("%title%\r\n%artist% | %album%") == "%artist% | %album%");

    assert(menuPathMatches("Playback Statistics/Rating/5", "Playback Statistics/Rating/5"));
    assert(menuPathMatches("Utilities/Playback Statistics/Rating/5", "Playback Statistics/Rating/5"));
    assert(menuPathMatches("&Playback Statistics/&Rating/5", "Playback Statistics/Rating/5"));
    assert(menuPathMatches("Playback Statistics\\Rating\\<not set>",
        "Playback Statistics/Rating/<not set>"));
    assert(!menuPathMatches("Other Component/Rating/5", "Playback Statistics/Rating/5"));

    const std::vector<std::wstring> technical{L"FLAC", L"lossless", L"Stereo", L"24 bits", L"96000 Hz"};
    assert(buildTopBarSummary(TopBarPlaybackState::playing, L"Song", L"Artist", L"Album", technical)
        == L"▶ Song - Artist | Album | FLAC | lossless | Stereo | 24 bits | 96000 Hz");
    assert(buildTopBarSummary(TopBarPlaybackState::paused, L"Song", L"", L"", {}) == L"Ⅱ Song");
    assert(buildTopBarSummary(TopBarPlaybackState::playing, L"", L"Artist", L"", {})
        == L"▶ Unknown title - Artist");
    assert(buildTopBarSummary(TopBarPlaybackState::stopped, L"Song", L"Artist", L"Album", technical).empty());
    return 0;
}
