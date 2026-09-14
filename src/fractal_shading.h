#pragma once

#include "physics.h"
#include "views.h"

#include <QColor>

QColor shadeFractalResult(
    const Result& result, bool enabled, double strength, double scale,
    const FractalPaletteSettings& palette = {});
float fractalColorValue(
    const Result& result, bool enabled, double strength, double scale);
