#pragma once

#include "physics.h"

#include <atomic>

struct NativeProbeResult {
    int period = 0;
    int ballsSpawnedAtDetection = 0;
    int collisionsAtDetection = 0;
    Outcome outcome = Outcome::Unresolved;
    int collisionEvents = 0;
    double periodStability = std::numeric_limits<double>::quiet_NaN();
    double expansionMargin = std::numeric_limits<double>::quiet_NaN();
    double contractionMargin = std::numeric_limits<double>::quiet_NaN();
};

NativeProbeResult runNativeProbe(
    const Config& config, const std::atomic_bool* cancel = nullptr);
