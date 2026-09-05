#pragma once

#include "physics.h"

#include <atomic>

// Primitive-only result shared by the independently compiled fixed-precision
// engines. Keeping MPFR values out of this boundary lets the renderer select a
// backend without converting an entire trajectory between number types.
struct FixedProbeResult {
    int period = 0;
    int ballsSpawnedAtDetection = 0;
    int collisionsAtDetection = 0;
    Outcome outcome = Outcome::Unresolved;
    int collisionEvents = 0;
    double periodStability = std::numeric_limits<double>::quiet_NaN();
    double expansionMargin = std::numeric_limits<double>::quiet_NaN();
    double contractionMargin = std::numeric_limits<double>::quiet_NaN();
};

FixedProbeResult runFixed128Probe(
    const Config& config, const std::atomic_bool* cancel = nullptr);
FixedProbeResult runFixed257Probe(
    const Config& config, const std::atomic_bool* cancel = nullptr);
FixedProbeResult runFixed513Probe(
    const Config& config, const std::atomic_bool* cancel = nullptr);
