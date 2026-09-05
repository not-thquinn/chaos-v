#include "zoom_math.h"

#include <algorithm>
#include <cmath>

double easedZoomProgress(
    double time, double duration, double dampingTime) {
    if (!(duration > 0))
        return 1;
    const double t = std::clamp(time, 0., duration);
    const double damping = std::clamp(dampingTime, 0., duration / 2);
    if (damping == 0)
        return t / duration;

    const double distanceScale = duration - damping;
    const auto rampDistance = [=](double rampTime) {
        const double u = std::clamp(rampTime / damping, 0., 1.);
        return damping * (u * u * u - .5 * u * u * u * u) /
               distanceScale;
    };
    if (t < damping)
        return rampDistance(t);
    if (t > duration - damping)
        return 1 - rampDistance(duration - t);
    return (t - .5 * damping) / distanceScale;
}

double zoomCenterProgress(
    double time, double panTime, double panDampingTime) {
    if (time <= 0)
        return 0;
    if (panTime <= 0)
        return 1;
    return easedZoomProgress(
        std::min(time, panTime), panTime, panDampingTime);
}

std::optional<std::array<PreciseDecimal, 4>> fitZoomCamera(
    const std::array<PreciseDecimal, 4>& source,
    int sourceWidth, int sourceHeight, int outputWidth, int outputHeight) {
    const PreciseDecimal xSpan = abs(source[1] - source[0]);
    const PreciseDecimal ySpan = abs(source[3] - source[2]);
    if (xSpan == 0 || ySpan == 0 || sourceWidth <= 0 || sourceHeight <= 0 ||
        outputWidth <= 0 || outputHeight <= 0)
        return {};

    const PreciseDecimal pixelAspect =
        xSpan * sourceHeight / (ySpan * sourceWidth);
    const PreciseDecimal desiredAxisAspect =
        pixelAspect * outputWidth / outputHeight;
    PreciseDecimal fittedX = xSpan;
    PreciseDecimal fittedY = ySpan;
    if (xSpan / ySpan > desiredAxisAspect)
        fittedX = ySpan * desiredAxisAspect;
    else
        fittedY = xSpan / desiredAxisAspect;

    const PreciseDecimal xCenter = (source[0] + source[1]) / 2;
    const PreciseDecimal yCenter = (source[2] + source[3]) / 2;
    if (source[1] < source[0])
        fittedX = -fittedX;
    if (source[3] < source[2])
        fittedY = -fittedY;
    return std::array<PreciseDecimal, 4>{
        xCenter - fittedX / 2, xCenter + fittedX / 2,
        yCenter - fittedY / 2, yCenter + fittedY / 2};
}

std::array<PreciseDecimal, 4> interpolateZoomAxes(
    const std::array<PreciseDecimal, 4>& start,
    const std::array<PreciseDecimal, 4>& end,
    double scaleProgress, double centerProgress) {
    const PreciseDecimal scaleP(std::clamp(scaleProgress, 0., 1.));
    const PreciseDecimal centerP(std::clamp(centerProgress, 0., 1.));
    const auto interpolateAxis = [&](int first, int last) {
        const PreciseDecimal startCenter = (start[first] + start[last]) / 2;
        const PreciseDecimal endCenter = (end[first] + end[last]) / 2;
        const PreciseDecimal center =
            startCenter + centerP * (endCenter - startCenter);
        const PreciseDecimal startSpan = start[last] - start[first];
        const PreciseDecimal endSpan = end[last] - end[first];
        PreciseDecimal span = exp(
            (PreciseDecimal(1) - scaleP) * log(abs(startSpan)) +
            scaleP * log(abs(endSpan)));
        if (startSpan < 0)
            span = -span;
        return std::pair<PreciseDecimal, PreciseDecimal>{
            center - span / 2, center + span / 2};
    };

    const auto x = interpolateAxis(0, 1);
    const auto y = interpolateAxis(2, 3);
    return {x.first, x.second, y.first, y.second};
}

std::array<PreciseDecimal, 4> squareZoomPreview(
    const std::array<PreciseDecimal, 4>& axes) {
    const PreciseDecimal xCenter = (axes[0] + axes[1]) / 2;
    const PreciseDecimal yCenter = (axes[2] + axes[3]) / 2;
    const PreciseDecimal magnitude = std::min(
        abs(axes[1] - axes[0]), abs(axes[3] - axes[2]));
    const PreciseDecimal xSpan = axes[1] >= axes[0] ? magnitude : -magnitude;
    const PreciseDecimal ySpan = axes[3] >= axes[2] ? magnitude : -magnitude;
    return {xCenter - xSpan / 2, xCenter + xSpan / 2,
            yCenter - ySpan / 2, yCenter + ySpan / 2};
}

int adaptiveZoomPrecision(
    const std::array<PreciseDecimal, 4>& axes,
    int width, int height, int maximumBits) {
    if (width <= 0 || height <= 0)
        return 64;
    const PreciseDecimal xStep = abs(axes[1] - axes[0]) / width;
    const PreciseDecimal yStep = abs(axes[3] - axes[2]) / height;
    const PreciseDecimal step = std::min(xStep, yStep);
    if (step <= 0)
        return std::max(64, maximumBits);

    PreciseDecimal scale = 1;
    for (const auto& coordinate : axes) {
        const PreciseDecimal magnitude = abs(coordinate);
        scale = std::max(scale, magnitude);
    }
    const double significantBits = preciseDouble(log(scale / step) / log(PreciseDecimal(2)));
    // Sixteen guard bits switch bands well before adjacent coordinates become
    // visibly quantized. Precision bands intentionally begin at 64 bits.
    const int wanted = int(std::ceil(significantBits)) + 16;
    const int rounded = std::max(64, ((wanted + 63) / 64) * 64);
    return std::min(std::max(64, maximumBits), rounded);
}
