#include "zoom_math.h"

#include <cmath>
#include <iostream>

namespace {

bool near(double first, double second, double tolerance = 1e-12) {
    return std::abs(first - second) <= tolerance;
}

double asDouble(const PreciseDecimal& value) {
    return preciseDouble(value);
}

} // namespace

int main() {
    bool passed = true;

    if (!near(easedZoomProgress(0, 10, 2), 0) ||
        !near(easedZoomProgress(10, 10, 2), 1) ||
        !near(easedZoomProgress(5, 10, 2), .5) ||
        !near(easedZoomProgress(2, 10, 2), .125) ||
        !near(easedZoomProgress(8, 10, 2), .875)) {
        std::cerr << "zoom easing profile is incorrect\n";
        passed = false;
    }

    const auto fitted = fitZoomCamera(
        {PreciseDecimal(0), PreciseDecimal(90),
         PreciseDecimal(-90), PreciseDecimal(0)},
        1024, 1024, 1920, 1080);
    if (!fitted || !near(asDouble((*fitted)[1] - (*fitted)[0]), 90) ||
        !near(asDouble((*fitted)[3] - (*fitted)[2]), 50.625) ||
        !near(asDouble(((*fitted)[2] + (*fitted)[3]) / 2), -45)) {
        std::cerr << "aspect-preserving camera fit is incorrect\n";
        passed = false;
    }

    const std::array<PreciseDecimal, 4> start{
        PreciseDecimal(0), PreciseDecimal(100),
        PreciseDecimal(0), PreciseDecimal(50)};
    const std::array<PreciseDecimal, 4> end{
        PreciseDecimal(90), PreciseDecimal(100),
        PreciseDecimal(40), PreciseDecimal(50)};
    const auto midpoint = interpolateZoomAxes(start, end, .5, .5);
    const double midpointWidth = asDouble(midpoint[1] - midpoint[0]);
    const double midpointHeight = asDouble(midpoint[3] - midpoint[2]);
    if (!near(midpointWidth, std::sqrt(1000.)) ||
        !near(midpointHeight, std::sqrt(500.)) ||
        !near(asDouble((midpoint[0] + midpoint[1]) / 2), 72.5) ||
        !near(asDouble((midpoint[2] + midpoint[3]) / 2), 35)) {
        std::cerr << "geometric camera interpolation is incorrect: "
                  << midpointWidth << ", " << midpointHeight << ", "
                  << asDouble((midpoint[0] + midpoint[1]) / 2) << ", "
                  << asDouble((midpoint[2] + midpoint[3]) / 2) << '\n';
        passed = false;
    }

    const auto centerSettled = interpolateZoomAxes(start, end, .1, 1);
    if (!near(asDouble((centerSettled[0] + centerSettled[1]) / 2), 95) ||
        !near(asDouble((centerSettled[2] + centerSettled[3]) / 2), 45)) {
        std::cerr << "zoom center did not settle independently of scale\n";
        passed = false;
    }

    if (!near(zoomCenterProgress(0, 4, 1), 0) ||
        !near(zoomCenterProgress(1, 4, 1), 1. / 6) ||
        !near(zoomCenterProgress(2, 4, 1), .5) ||
        !near(zoomCenterProgress(4, 4, 1), 1) ||
        !near(zoomCenterProgress(8, 4, 1), 1)) {
        std::cerr << "zoom center damping is incorrect\n";
        passed = false;
    }

    const auto square = squareZoomPreview(
        {PreciseDecimal(0), PreciseDecimal(100),
         PreciseDecimal(-25), PreciseDecimal(25)});
    if (!near(asDouble(square[1] - square[0]), 50) ||
        !near(asDouble(square[3] - square[2]), 50) ||
        !near(asDouble((square[0] + square[1]) / 2), 50)) {
        std::cerr << "square zoom preview is incorrect\n";
        passed = false;
    }

    if (adaptiveZoomPrecision(
            {PreciseDecimal(0), PreciseDecimal(90),
             PreciseDecimal(-90), PreciseDecimal(0)},
            1024, 1024) != 64 ||
        adaptiveZoomPrecision(
            {PreciseDecimal("71.331338366274"),
             PreciseDecimal("71.331338366277"),
             PreciseDecimal("-71.248636077857"),
             PreciseDecimal("-71.248636077854")},
            1024, 1024) != 128) {
        std::cerr << "adaptive zoom precision selected the wrong band\n";
        passed = false;
    }

    return passed ? 0 : 1;
}
