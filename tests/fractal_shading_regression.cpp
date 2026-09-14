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
    config.trackExpansionMargin = true;

    const Result result = classifyFractalPoint(config);
    if (result.outcome != Outcome::Periodic || result.period != 3 ||
        !std::isfinite(result.expansionMargin) ||
        result.expansionMargin <= 0) {
        std::cerr << "period or expansion margin failed\n";
        return 1;
    }
    if (shadeFractalResult(result, false, 1, 5) != colorFor(result)) {
        std::cerr << "disabled shading changed the period color\n";
        return 1;
    }
    const QColor shaded = shadeFractalResult(result, true, .7, 5);
    if (!shaded.isValid() || shaded == colorFor(result)) {
        std::cerr << "expansion shading produced an invalid color\n";
        return 1;
    }
    Result boundary = result;
    boundary.expansionMargin = 0;
    if (shadeFractalResult(boundary, true, 1, 5) !=
        periodColor(result.period + 1)) {
        std::cerr << "zero margin did not reach the next period color\n";
        return 1;
    }
    boundary.expansionMargin = 5;
    if (shadeFractalResult(boundary, true, 1, 5) !=
        periodColor(double(result.period) + .5)) {
        std::cerr << "scale did not produce a half-period color shift\n";
        return 1;
    }

    const FractalPaletteSettings linearUltra{true, 0, 20};
    if (periodColor(1., linearUltra) != QColor(0, 7, 100) ||
        periodColor(20., linearUltra) != QColor(0, 0, 0) ||
        periodColor(200., linearUltra) != periodColor(20., linearUltra)) {
        std::cerr << "Ultra Fractal gradient endpoints or clamping failed\n";
        return 1;
    }
    const FractalPaletteSettings sublinear{true, -1, 20};
    const FractalPaletteSettings superlinear{true, 1, 20};
    if (periodColor(10., sublinear) == periodColor(10., linearUltra) ||
        periodColor(10., superlinear) == periodColor(10., linearUltra) ||
        periodColor(10., sublinear) == periodColor(10., superlinear)) {
        std::cerr << "palette mapping curve had no effect\n";
        return 1;
    }
    Result budget; budget.outcome = Outcome::CollisionBudget;
    Result blocked; blocked.outcome = Outcome::SpawnBlocked;
    Result capacity; capacity.outcome = Outcome::LiveCapacity;
    Result unresolved; unresolved.outcome = Outcome::Unresolved;
    if (colorFor(budget, linearUltra) != QColor(0, 0, 0) ||
        colorFor(blocked, linearUltra) != colorFor(capacity, linearUltra) ||
        colorFor(blocked, linearUltra) != colorFor(unresolved, linearUltra) ||
        colorFor(blocked, linearUltra) == colorFor(budget, linearUltra)) {
        std::cerr << "alternate error colors are incorrect\n";
        return 1;
    }
    boundary.expansionMargin = 0;
    if (shadeFractalResult(boundary, true, 1, 5, linearUltra) !=
        periodColor(result.period + 1., linearUltra)) {
        std::cerr << "fractional shading did not use alternate palette\n";
        return 1;
    }
    return 0;
}
