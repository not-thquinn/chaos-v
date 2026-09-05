#include "fractal_shading.h"

#include "views.h"

#include <algorithm>
#include <cmath>

QColor shadeFractalResult(
    const Result& result, bool enabled, double strength, double scale) {
    const QColor base = colorFor(result);
    if (!enabled || result.outcome != Outcome::Periodic ||
        !std::isfinite(result.expansionMargin) ||
        result.expansionMargin < 0 || !(scale > 0) || strength <= 0)
        return base;

    // A zero-margin near miss receives the full fractional-period shift.
    // `scale` is the margin at which that shift has fallen to one half.
    const double falloff = scale / (scale + result.expansionMargin);
    const double periodShift =
        std::clamp(strength, 0., 1.) * std::clamp(falloff, 0., 1.);
    return periodColor(double(result.period) + periodShift);
}
