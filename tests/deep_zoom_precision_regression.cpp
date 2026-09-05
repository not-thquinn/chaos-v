#include "physics.h"
#include "precise_decimal.h"

#include <iostream>
#include <set>
#include <string>

int main() {
    constexpr int pixels = 1024;
    const PreciseDecimal first("71.331338366274");
    const PreciseDecimal last("71.331338366277");

    std::set<std::string> exactCoordinates;
    std::set<double> legacyCoordinates;
    for (int pixel = 0; pixel < pixels; ++pixel) {
        const PreciseDecimal coordinate =
            precisePixelCenter(first, last, pixel, pixels);
        exactCoordinates.insert(preciseString(coordinate).toStdString());
        legacyCoordinates.insert(preciseDouble(coordinate));
    }

    if (exactCoordinates.size() != pixels) {
        std::cerr << "high-precision render grid collapsed coordinates\n";
        return 1;
    }
    if (legacyCoordinates.size() >= pixels) {
        std::cerr << "test range no longer demonstrates the double limit\n";
        return 1;
    }

    setThreadRealPrecision(192);
    const Real adjacentA = makeReal(*exactCoordinates.begin());
    const Real adjacentB = makeReal(*std::next(exactCoordinates.begin()));
    if (adjacentA == adjacentB) {
        std::cerr << "exact coordinate strings collapsed entering MPFR\n";
        return 1;
    }

    return 0;
}
