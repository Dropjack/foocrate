#include "playlist_view_model.h"

int main() {
    using namespace foocrate;

    if (visibleRowCapacity(100.0F, 25.0F) != 4) return 1;
    if (visibleRowCapacity(0.0F, 25.0F) != 0) return 2;
    if (maximumTopRow(100, 10) != 90) return 3;
    if (maximumTopRow(5, 10) != 0) return 4;
    if (clampTopRow(99, 100, 10) != 90) return 5;

    const auto rows = visibleRows(10, 100, 100.0F, 25.0F);
    if (rows.first != 9 || rows.end != 15) return 6;
    if (ensureRowVisible(10, 9, 100, 5) != 9) return 7;
    if (ensureRowVisible(10, 14, 100, 5) != 10) return 8;
    if (ensureRowVisible(10, 15, 100, 5) != 11) return 9;
    const auto reversedRange = inclusiveRange(8, 3);
    if (reversedRange.first != 3 || reversedRange.second != 8) return 10;

    if (scrollbarThumbHeight(100.0F, 100, 10) != 24.0F) return 11;
    if (scrollbarThumbHeight(100.0F, 5, 10) != 100.0F) return 12;
    volatile auto top = scrollbarThumbTop(10.0F, 100.0F, 24.0F, 45, 90);
    if (top != 48.0F) return 13;
    if (topRowFromScrollbar(48.0F, 0.0F, 10.0F, 100.0F, 24.0F, 90) != 45) return 14;

    return 0;
}
