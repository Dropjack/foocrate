#include "playback_state.h"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace foocrate;

    assert(seekFraction(30.0, 120.0) == 0.25);
    assert(seekFraction(-5.0, 120.0) == 0.0);
    assert(seekFraction(150.0, 120.0) == 1.0);
    assert(seekFraction(1.0, 0.0) == 0.0);
    assert(seekFraction(std::numeric_limits<double>::quiet_NaN(), 100.0) == 0.0);
    assert(seekPosition(0.25, 120.0) == 30.0);
    assert(seekPosition(2.0, 120.0) == 120.0);
    assert(committedSeekTarget(true, true, true, 30.0, 120.0) == 30.0);
    assert(committedSeekTarget(true, true, true, 150.0, 120.0) == 120.0);
    assert(!committedSeekTarget(false, true, true, 30.0, 120.0));
    assert(!committedSeekTarget(true, false, true, 30.0, 120.0));
    assert(!committedSeekTarget(true, true, false, 30.0, 120.0));

    assert(volumeDbFromFraction(0.0) == -100.0F);
    assert(volumeDbFromFraction(1.0) == 0.0F);
    assert(std::abs(volumeDbFromFraction(0.5) - (-10.0F)) < 0.001F);
    assert(std::abs(volumeFractionFromDb(-10.0F) - 0.5) < 0.001);
    assert(std::abs(volumeFractionFromDb(-20.0F) - 0.25) < 0.001);
    assert(volumeFractionFromDb(-100.0F) > 0.0);
    assert(volumeFractionFromDb(-100.0F) < 0.001);
    assert(volumeFractionFromDb(0.0F) == 1.0);

    assert(formatPlaybackTime(0.0) == L"0:00");
    assert(formatPlaybackTime(65.9) == L"1:05");
    assert(formatPlaybackTime(3661.0) == L"1:01:01");
    assert(formatPlaybackTime(0.0, false) == L"--:--");
    assert(formatRemainingTime(30.0, 120.0) == L"-1:30");
    assert(formatRemainingTime(150.0, 120.0) == L"-0:00");
    assert(formatRemainingTime(0.0, 0.0) == L"--:--");

    PlaybackState state;
    state.playing = true;
    state.seekable = true;
    state.length = 100.0;
    assert(state.canSeek());
    state.length = 0.0;
    assert(!state.canSeek());
    state.setVolume(-12.0F);
    assert(!state.muted());
    assert(state.lastAudibleVolumeDb == -12.0F);
    state.setVolume(-100.0F);
    assert(state.muted());
    assert(state.lastAudibleVolumeDb == -12.0F);

    return 0;
}
