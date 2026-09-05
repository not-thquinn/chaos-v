#include "fixed_probe.h"

#ifndef CHAOSV_FIXED_PROBE_FUNCTION
#error CHAOSV_FIXED_PROBE_FUNCTION must name this backend's adapter function
#endif

FixedProbeResult CHAOSV_FIXED_PROBE_FUNCTION(
    const Config& config, const std::atomic_bool* cancel) {
    const auto result = Simulator(config).run(false, cancel);
    return {
        result.period,
        result.ballsSpawnedAtDetection,
        result.collisionsAtDetection,
        result.outcome,
        result.collisionEvents,
        result.periodStability,
        result.expansionMargin,
        result.contractionMargin
    };
}
