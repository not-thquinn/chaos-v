#include "fractal_shading.h"

#include "views.h"

#include <algorithm>
#include <cmath>

QColor shadeFractalResult(
    const Result& result, bool enabled, double strength, double scale,
    const FractalPaletteSettings& palette) {
    const QColor base = colorFor(result, palette);
    if (!enabled || result.outcome != Outcome::Periodic ||
        !std::isfinite(result.expansionMargin) ||
        result.expansionMargin < 0 || !(scale > 0) || strength <= 0)
        return base;

    return periodColor(fractalColorValue(result, enabled, strength, scale),
                       palette);
}

float fractalColorValue(
    const Result& result, bool enabled, double strength, double scale) {
    if (result.outcome != Outcome::Periodic)
        return float(-int(result.outcome) - 1);
    if (!enabled || !std::isfinite(result.expansionMargin) ||
        result.expansionMargin < 0 || !(scale > 0) || strength <= 0)
        return float(result.period);

    // A zero-margin near miss receives the full fractional-period shift.
    // `scale` is the margin at which that shift has fallen to one half.
    const double falloff = scale / (scale + result.expansionMargin);
    const double periodShift =
        std::clamp(strength, 0., 1.) * std::clamp(falloff, 0., 1.);
    return float(double(result.period) + periodShift);
}
