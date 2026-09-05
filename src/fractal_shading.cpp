#include "fractal_shading.h"

#include "views.h"

#include <algorithm>
#include <cmath>

namespace {
double linearChannel(int channel) {
    const double value = channel / 255.;
    return value <= .04045 ? value / 12.92
                           : std::pow((value + .055) / 1.055, 2.4);
}
int srgbChannel(double value) {
    value = std::clamp(value, 0., 1.);
    const double encoded = value <= .0031308
                               ? 12.92 * value
                               : 1.055 * std::pow(value, 1. / 2.4) - .055;
    return std::clamp(int(std::lround(encoded * 255)), 0, 255);
}
struct Oklab { double l = 0, a = 0, b = 0; };
Oklab toOklab(QColor color) {
    const double r = linearChannel(color.red());
    const double g = linearChannel(color.green());
    const double b = linearChannel(color.blue());
    const double ll = std::cbrt(.4122214708*r + .5363325363*g + .0514459929*b);
    const double mm = std::cbrt(.2119034982*r + .6806995451*g + .1073969566*b);
    const double ss = std::cbrt(.0883024619*r + .2817188376*g + .6299787005*b);
    return {.2104542553*ll + .7936177850*mm - .0040720468*ss,
            1.9779984951*ll - 2.4285922050*mm + .4505937099*ss,
            .0259040371*ll + .7827717662*mm - .8086757660*ss};
}
QColor fromOklab(Oklab color) {
    const double l0 = color.l + .3963377774*color.a + .2158037573*color.b;
    const double m0 = color.l - .1055613458*color.a - .0638541728*color.b;
    const double s0 = color.l - .0894841775*color.a - 1.2914855480*color.b;
    const double l = l0*l0*l0, m = m0*m0*m0, s = s0*s0*s0;
    return QColor(srgbChannel(4.0767416621*l - 3.3077115913*m + .2309699292*s),
                  srgbChannel(-1.2684380046*l + 2.6097574011*m - .3413193965*s),
                  srgbChannel(-.0041960863*l - .7034186147*m + 1.7076147010*s));
}
}

QColor shadeFractalResult(
    const Result& result, bool enabled, double strength, double scale) {
    const QColor base = colorFor(result);
    if (!enabled || result.outcome != Outcome::Periodic ||
        !std::isfinite(result.periodStability) ||
        result.periodStability < 0 || !(scale > 0) || strength <= 0)
        return base;

    Oklab color = toOklab(base);
    // Zero is a fragile period boundary; robust causal components approach
    // +1 after logarithmic compression. A fixed normalized scale keeps
    // overlapping renders and zoom frames chromatically consistent.
    const double logarithmic = std::log1p(result.periodStability / scale);
    const double signal = 2 * logarithmic / (1 + logarithmic) - 1;
    color.l = std::clamp(color.l + .28 * std::clamp(strength, 0., 1.) * signal,
                         .18, .94);
    return fromOklab(color);
}
