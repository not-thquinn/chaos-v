#include "fractal_classifier.h"
#include "fractal_shading.h"
#include "views.h"

#include <cmath>
#include <iostream>

int main() {
    Config config;
    config.gravity = 400; config.radius = 35; config.restitution = .9;
    config.gap = 320; config.segmentLength = 150; config.spawnX = -160;
    config.spawnY = -230; config.spawnInterval = 2; config.cutoffY = 560;
    config.maxBalls = 12; config.analysisBalls = 50;
    config.collisionBudget = 1000; config.precisionBits = 64;
    config.leftDeg = 72.0703125; config.rightDeg = -70.6640625;
    config.trackPeriodStability = true;

    const Result result = classifyFractalPoint(config);
    if (result.outcome != Outcome::Periodic || result.period != 3 ||
        !std::isfinite(result.periodStability) ||
        !std::isfinite(result.expansionMargin) ||
        !std::isfinite(result.contractionMargin) ||
        result.periodStability <= 0) {
        std::cerr << "period or causal stability margin failed\n";
        return 1;
    }
    if (shadeFractalResult(result, false, 1, 5) != colorFor(result)) {
        std::cerr << "disabled shading changed the period color\n";
        return 1;
    }
    const QColor shaded = shadeFractalResult(result, true, .7, 5);
    if (!shaded.isValid() || shaded == colorFor(result)) {
        std::cerr << "exit shading produced an invalid color\n";
        return 1;
    }
    return 0;
}
