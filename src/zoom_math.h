#pragma once

#include "precise_decimal.h"

#include <array>
#include <optional>

double easedZoomProgress(double time, double duration, double dampingTime);
double zoomCenterProgress(
    double time, double panTime, double panDampingTime);

std::optional<std::array<PreciseDecimal, 4>> fitZoomCamera(
    const std::array<PreciseDecimal, 4>& source,
    int sourceWidth, int sourceHeight, int outputWidth, int outputHeight);

std::array<PreciseDecimal, 4> interpolateZoomAxes(
    const std::array<PreciseDecimal, 4>& start,
    const std::array<PreciseDecimal, 4>& end,
    double scaleProgress, double centerProgress);

std::array<PreciseDecimal, 4> squareZoomPreview(
    const std::array<PreciseDecimal, 4>& axes);

int adaptiveZoomPrecision(
    const std::array<PreciseDecimal, 4>& axes,
    int width, int height, int maximumBits = 2048);
